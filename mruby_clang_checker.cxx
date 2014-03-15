/*
 * This is mruby variadic arguments checker clang plugin.
 * Use this with:
 * -Xclang -load -Xclang "path/to/libmruby-clang-checker.so" -Xclang -add-plugin -Xclang mruby-clang-checker
 */

#include <unordered_set>
#include <iostream>
#include <sstream>

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>

namespace {

using namespace clang;

struct CheckMRuby : public ASTConsumer, public RecursiveASTVisitor<CheckMRuby> {
  ASTContext* ctx = nullptr;
  CallExpr::arg_iterator arg;
  CallExpr* call_expr = NULL;
  StringRef expected_type;
  // CompilerInstance& compiler;

  std::unordered_set<std::string> mrb_functions = {
    "mrb_get_args",
    "mrb_funcall",
    "mrb_raisef",
    "mrb_name_error",
    "mrb_warn",
    "mrb_bug",
    "mrb_format",
    "mrb_intern_cstr",
  };

  CheckMRuby(CompilerInstance& /* inst */)
      // : compiler(inst)
  {}

  void HandleTranslationUnit(ASTContext& ctx_) override {
    ctx = &ctx_;
    TraverseDecl(ctx->getTranslationUnitDecl());
    ctx = nullptr;
  }

  std::string get_type_name(Expr const* exp) {
    return exp->getType()->getPointeeType().getAsString();
  }

  void type_error(Expr* exp) {
    assert(ctx);
    auto& diag = ctx->getDiagnostics();
    unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, "Wrong argument passed to variadic mruby C API.");
    std::string t = (std::string("Expected pointer of:") + expected_type).str();
    auto hint = FixItHint::CreateReplacement(SourceRange(exp->getLocStart(), exp->getLocEnd()), t);
    diag.Report(exp->getLocStart(), diag_id).AddFixItHint(hint);
  }

  int format_spec_arg_req(StringLiteral const& lit) {
    unsigned expected = 0;
    bool optional_begin = false;
    for(auto const& i : lit.getString()) {
      switch(i) {
        case '?':
          if(not optional_begin) {
            auto& diag = ctx->getDiagnostics();
            unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, "'?' format specifier must come after '|' format specifier.");
            diag.Report(lit.getLocStart(), diag_id);
            return -1;
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

        case 'd':
        case '*':
        case 's':
        case 'a':
          expected += 2; break;

        default: {
          std::ostringstream oss("Invalid format specifier: ", std::ios_base::ate);
          oss << "'" << i << "'";
          auto& diag = ctx->getDiagnostics();
          unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, oss.str().c_str());
          diag.Report(lit.getLocStart(), diag_id);
          return -1;
        }
      }
    }
    return expected;
  }

  bool argument_count_error(unsigned expected) {
    assert(expected != call_expr->getNumArgs());

    std::ostringstream oss("Wrong number of arguments passed to variadic mruby C API. Expected: ", std::ios_base::ate);
    oss << expected << ", Actual: " << call_expr->getNumArgs();
    auto& diag = ctx->getDiagnostics();
    unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, oss.str().c_str());
    diag.Report(call_expr->getLocStart(), diag_id);
    return true;
  }

  bool VisitCallExpr(CallExpr* exp) {
    FunctionDecl* d = dyn_cast<FunctionDecl>(exp->getCalleeDecl());
    if(not d) { return true; }

    StringRef const name = dyn_cast<NamedDecl>(d)->getIdentifier()->getName();
    if(not d->isExternC() or mrb_functions.find(name) == mrb_functions.end()) {
      return true;
    }

    call_expr = exp;

    if(name == "mrb_intern_cstr") {
      if(dyn_cast<StringLiteral>(call_expr->getArg(1)->IgnoreImplicit())) {
        auto& diag = ctx->getDiagnostics();
        unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, "`mrb_intern_lit` is preferred when getting symbol from string literal.");
        auto hint = FixItHint::CreateReplacement(SourceRange(call_expr->getLocStart(), call_expr->getLocEnd()), "mrb_intern_lit");
        diag.Report(call_expr->getLocStart(), diag_id).AddFixItHint(hint);
      }
      return true;
    }

    assert(d->isVariadic());

    if(name == "mrb_get_args") {
      StringLiteral* lit = dyn_cast<StringLiteral>(exp->getArg(1)->IgnoreImplicit());
      if(not lit) { return true; }

      StringRef const format = lit->getString();
      int const required_args = format_spec_arg_req(*lit);

      if(required_args == -1) {
        return true; // some error occur in check
      }

      if(exp->getNumArgs() != (required_args + d->param_size())) {
        return argument_count_error((required_args + d->param_size()));
      }

      arg = exp->arg_begin() + d->param_size();

      for(auto const& i : format) {
        if(i == '|' and arg == exp->arg_end()) { continue; }

        assert(arg != exp->arg_end());

        if(not arg->getType()->isPointerType()) {
          auto& diag = ctx->getDiagnostics();
          unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, "Variadic argument of `mrb_get_args` must be a pointer.");
          diag.Report(arg->getLocStart(), diag_id);
          return true;
        }

        switch(i) {
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

          case 'z': expected_type = "char *"; break;
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
            expected_type = "int";
            break;

          case 's':
            expected_type = "char *";
            if(get_type_name(*arg) != expected_type) { type_error(*arg); }
            ++arg;
            expected_type = "int";
            break;

          case 'a':
            expected_type = "mrb_value *";
            if(get_type_name(*arg) != expected_type) { type_error(*arg); }
            ++arg;
            expected_type = "mrb_int";
            break;
        }
        if(get_type_name(*arg) != expected_type) { type_error(*arg); }
        ++arg;
      }
      return true;
    }

    if(name == "mrb_funcall") {
      if(IntegerLiteral* lit = dyn_cast<IntegerLiteral>(exp->getArg(3)->IgnoreImplicit())) {
        if(lit->getValue() != (exp->getNumArgs() - d->param_size())) {
          return argument_count_error(*lit->getValue().getRawData() + d->param_size());
        }
      }
    } else {
      if(StringLiteral* lit = dyn_cast<StringLiteral>(exp->getArg(d->param_size() - 1)->IgnoreImplicit())) {
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
        auto& diag = ctx->getDiagnostics();
        unsigned const diag_id = diag.getCustomDiagID(DiagnosticsEngine::Error, "Wrong argument passed to variadic mruby C API.");
        auto hint = FixItHint::CreateReplacement(SourceRange(exp->getLocStart(), exp->getLocEnd()), "Expected mrb_value.");
        diag.Report(exp->getLocStart(), diag_id).AddFixItHint(hint);
      }
    }

    return true;
  }
};

struct CheckMRubyAction : public PluginASTAction {
  ASTConsumer* CreateASTConsumer(CompilerInstance& inst, llvm::StringRef str) override {
    return new CheckMRuby(inst);
  }

  bool ParseArgs(const CompilerInstance&,
                 const std::vector<std::string>&) override {
    return true;
  }
};

}

static clang::FrontendPluginRegistry::Add<CheckMRubyAction>
X("mruby-clang-checker", "makes mruby code safer");
