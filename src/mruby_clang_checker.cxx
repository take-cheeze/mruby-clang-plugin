/*
 * This is mruby variadic arguments checker clang plugin.
 * Use this with:
 * -Xclang -load -Xclang "path/to/libmruby-clang-checker.so" -Xclang -add-plugin -Xclang mruby-clang-checker
 */

#include <unordered_set>
#include <iostream>

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>

namespace {

using namespace clang;

struct CheckMRuby : public ASTConsumer, public RecursiveASTVisitor<CheckMRuby> {
  CallExpr::arg_iterator arg;
  CallExpr* call_expr = NULL;
  StringRef expected_type;
  CompilerInstance& compiler;
  DiagnosticsEngine& diagnostics;
  struct {
    unsigned invalid_format_spec, argument_type, question_format_spec;
    unsigned argument_count, prefer_mrb_intern_lit, must_be_pointer, prefer_str_new_lit;
  } diag_ids;

  std::unordered_set<std::string> mrb_functions = {
    "mrb_get_args",
    "mrb_funcall",
    "mrb_funcall_id",
    "mrb_raisef",
    "mrb_name_error",
    "mrb_no_method_error",
    "mrb_warn",
    "mrb_bug",
    "mrb_format",
    "mrb_intern_cstr",
    "mrb_str_new_cstr",
  };

  CheckMRuby(CompilerInstance& inst)
      : compiler(inst), diagnostics(inst.getDiagnostics())
  {
    diag_ids.invalid_format_spec = diagnostics.getCustomDiagID(DiagnosticsEngine::Error, "Invalid format specifier: '%0'");
    diag_ids.argument_type = diagnostics.getCustomDiagID(DiagnosticsEngine::Error, "Wrong argument passed to variadic mruby C API.");
    diag_ids.question_format_spec = diagnostics.getCustomDiagID(DiagnosticsEngine::Error, "'?' format specifier must come after '|' format specifier.");
    diag_ids.argument_count = diagnostics.getCustomDiagID(DiagnosticsEngine::Error, "Wrong number of arguments passed to variadic mruby C API. Expected: %0, Actual: %1");
    diag_ids.prefer_mrb_intern_lit = diagnostics.getCustomDiagID(DiagnosticsEngine::Warning, "`mrb_intern_lit` is preferred when getting symbol from string literal.");
    diag_ids.must_be_pointer = diagnostics.getCustomDiagID(DiagnosticsEngine::Error, "Variadic argument of `mrb_get_args` must be a pointer.");
    diag_ids.prefer_str_new_lit = diagnostics.getCustomDiagID(DiagnosticsEngine::Warning, "`mrb_str_new_lit` is preferred when creating string object from literal.");
  }

  void HandleTranslationUnit(ASTContext& ctx) override {
    TraverseDecl(ctx.getTranslationUnitDecl());
  }

  std::string get_type_name(Expr const* exp) {
    return exp->getType()->getPointeeType().getAsString();
  }

  void type_error(Expr* exp) {
    std::string t = (std::string("Expected pointer of: ") + expected_type).str();
    diagnostics.Report(exp->getLocStart(), diag_ids.argument_type)
        << FixItHint::CreateReplacement(SourceRange(exp->getLocStart(), exp->getLocEnd()), t);
  }

  llvm::Optional<unsigned> format_spec_arg_req(StringLiteral const& lit) {
    unsigned expected = 0;
    bool optional_begin = false;
    char prev_i = 0;
    for(auto const& i : lit.getString()) {
      switch(i) {
        case '?':
          if(not optional_begin) {
            diagnostics.Report(lit.getLocStart(), diag_ids.question_format_spec);
            return llvm::None;
          }

          // fall through
        case 'b':

        case 'o':
        case 'C':
        case 'S':
        case 'A':
        case 'H':
        case '&':
        case 'z':
        case 'f':
        case 'i':
        case 'n':
          expected += 1;
          break;

        case '|': optional_begin = true; break;

        case '!':
          expected += 0;
          // skip type checking
          switch (prev_i) {
          case 'S': case 'A': case 'H': case 'z': case '&':
            continue;
          case 'a': case 's': case '*':
            continue;
          case 0:
          default:
            diagnostics.Report(lit.getLocStart(), diag_ids.argument_type);
            continue;
          }
          break;

        case 'd':
        case '*':
        case 's':
        case 'a':
          expected += 2; break;

        default: {
          char const str[] = { i, '\0' };
          diagnostics.Report(lit.getLocStart(), diag_ids.invalid_format_spec) << str;
          return llvm::None;
        }
      }
      prev_i = i;
    }
    return expected;
  }

  bool argument_count_error(unsigned expected) {
    assert(expected != call_expr->getNumArgs());
    diagnostics.Report(call_expr->getLocStart(), diag_ids.argument_count) << expected << call_expr->getNumArgs();
    return true;
  }

  bool VisitCallExpr(CallExpr* exp) {
    FunctionDecl* d = dyn_cast_or_null<FunctionDecl>(exp->getCalleeDecl());
    if(not d or not d->isExternC()) { return true; }

    StringRef const name = dyn_cast_or_null<NamedDecl>(d)->getIdentifier()->getName();
    if(mrb_functions.find(name) == mrb_functions.end()) {
      return true;
    }

    call_expr = exp;

    if(name == "mrb_intern_cstr") {
      if(dyn_cast_or_null<StringLiteral>(call_expr->getArg(1)->IgnoreImplicit())) {
        diagnostics.Report(call_expr->getLocStart(), diag_ids.prefer_mrb_intern_lit)
            << FixItHint::CreateReplacement(SourceRange(call_expr->getLocStart(), call_expr->getLocEnd()), "mrb_intern_lit");
      }
      return true;
    }

    if (name == "mrb_str_new_cstr") {
      if (dyn_cast_or_null<StringLiteral>(call_expr->getArg(1)->IgnoreImplicit())) {
        diagnostics.Report(call_expr->getLocStart(), diag_ids.prefer_str_new_lit)
            << FixItHint::CreateReplacement(SourceRange(call_expr->getLocStart(), call_expr->getLocEnd()), "mrb_str_new_lit");
      }
      return true;
    }

    assert(d->isVariadic());

    if(name == "mrb_get_args") {
      StringLiteral* lit = dyn_cast_or_null<StringLiteral>(exp->getArg(1)->IgnoreImplicit());
      if(not lit) { return true; }

      StringRef const format = lit->getString();
      llvm::Optional<unsigned> const required_args = format_spec_arg_req(*lit);

      if(not required_args) {
        return true; // some error occur in check
      }

      if(exp->getNumArgs() != (required_args.getValue() + d->param_size())) {
        return argument_count_error((required_args.getValue() + d->param_size()));
      }

      arg = exp->arg_begin() + d->param_size();

      for(auto const& i : format) {
#if LLVM_VERSION_MAJOR > 4 || LLVM_VERSION_MINOR >= 0
        if(exp->arg_end() > arg and not (*arg)->getType()->isPointerType()) {
          diagnostics.Report((*arg)->getLocStart(), diag_ids.must_be_pointer);
          return true;
        }
#else
        if(exp->arg_end() > arg and not arg->getType()->isPointerType()) {
          diagnostics.Report(arg->getLocStart(), diag_ids.must_be_pointer);
          return true;
        }
#endif

        switch(i) {
          case '!':
            continue;

          case 'o':
          case 'C':
          case 'S':
          case 'A':
          case 'H':
          case '&':
            expected_type = "mrb_value"; break;

          case '?':
            // fall through
          case 'b':
            expected_type = "mrb_bool"; break;

          case 'z':
            expected_type = "char *";
            if (get_type_name(*arg) != "const char *" and
                get_type_name(*arg) != "char *") { type_error(*arg); }
            ++arg;
            continue;

          case 'f': expected_type = "mrb_float"; break;
          case 'i': expected_type = "mrb_int"; break;
          case 'n': expected_type = "mrb_sym"; break;

          case '|': continue;

          case 'd':
            expected_type = "void*";
            if(not (*arg)->getType()->getPointeeType()->isPointerType()) { type_error(*arg); }
            ++arg;
            expected_type = "mrb_data_type";
            if(get_type_name(*arg) != "struct mrb_data_type" and
               get_type_name(*arg) != "const struct mrb_data_type") { type_error(*arg); }
            ++arg;
            continue;

          case '*':
            expected_type = "mrb_value *";
            if(get_type_name(*arg) != expected_type) { type_error(*arg); }
            ++arg;
            expected_type = "mrb_int";
            break;

          case 's':
            expected_type = "char *";
            if(get_type_name(*arg) != "char *" and
               get_type_name(*arg) != "const char *") { type_error(*arg); }
            ++arg;
            expected_type = "mrb_int";
            break;

          case 'a':
            expected_type = "mrb_value *";
            if(get_type_name(*arg) != expected_type) { type_error(*arg); }
            ++arg;
            expected_type = "mrb_int";
            break;

          default: assert(false);
        }
        if(get_type_name(*arg) != expected_type) { type_error(*arg); }
        ++arg;
      }
      assert(arg == exp->arg_end());
      return true;
    }

    if(name == "mrb_funcall" or name == "mrb_funcall_id") {
      if(IntegerLiteral* lit = dyn_cast_or_null<IntegerLiteral>(exp->getArg(3)->IgnoreImplicit())) {
        if(lit->getValue() != (exp->getNumArgs() - d->param_size())) {
          return argument_count_error(*lit->getValue().getRawData() + d->param_size());
        }
      }
    } else {
      if(StringLiteral* lit = dyn_cast_or_null<StringLiteral>(exp->getArg(d->param_size() - 1)->IgnoreImplicit())) {
        StringRef const format = lit->getString();
        StringRef const search_str = "%S";
        unsigned extra_args_count = 0;
        for(size_t from = 0; (from = format.find(search_str, from)) != StringRef::npos; ++extra_args_count) {
          from += search_str.size();
        }
        if(extra_args_count != (exp->getNumArgs() - d->param_size())) {
          return argument_count_error(extra_args_count + d->param_size());
        }
      }
    }

    for(auto i = exp->arg_begin() + d->param_size(); i != exp->arg_end(); ++i) {
      if((*i)->getType().getAsString() != "mrb_value") {
#if LLVM_VERSION_MAJOR > 4 || LLVM_VERSION_MINOR >= 0
        diagnostics.Report((*i)->getLocStart(), diag_ids.argument_type)
            << FixItHint::CreateReplacement(SourceRange((*i)->getLocStart(), (*i)->getLocEnd()), "Expected `mrb_value`.");
#else
        diagnostics.Report(i->getLocStart(), diag_ids.argument_type)
            << FixItHint::CreateReplacement(SourceRange(i->getLocStart(), i->getLocEnd()), "Expected `mrb_value`.");
#endif
      }
    }

    return true;
  }
};

struct CheckMRubyAction : public PluginASTAction {
#if LLVM_VERSION_MAJOR > 3 || LLVM_VERSION_MINOR >= 6
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& inst, llvm::StringRef str) override {
    return llvm::make_unique<CheckMRuby>(inst);
  }
#else
  ASTConsumer* CreateASTConsumer(CompilerInstance& inst, llvm::StringRef str) override {
    return new CheckMRuby(inst);
  }
#endif

  bool ParseArgs(const CompilerInstance&,
                 const std::vector<std::string>&) override {
    return true;
  }
};

}

static clang::FrontendPluginRegistry::Add<CheckMRubyAction>
X("mruby-clang-checker", "makes mruby code safer");
