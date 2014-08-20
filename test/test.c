#include "mruby.h"
#include "mruby/data.h"
#include "mruby/error.h"

static mrb_value v;
static mrb_bool b;
static char* c;
static char const *const_str;
static mrb_float f;
static mrb_int mrb_i;
static mrb_sym sym;
static void* voidp;
static struct mrb_data_type type;
static mrb_value* argv;

void test_get_args(mrb_state* M) {
#define MRB_ARGS_ALL_TYPE "oCSAH&bzfin|d?*sa"

  // check all types
  mrb_get_args(M, MRB_ARGS_ALL_TYPE,
               &v, &v, &v, &v, &v, &v, &b, &c, &f, &mrb_i, &sym, &voidp, &type, &b, &argv, &mrb_i, &c, &mrb_i, &argv, &mrb_i);
  mrb_get_args(M, "zs", &const_str, &const_str, &mrb_i);

  // must succeed
  mrb_get_args(M, "|");

  // wrong type check
  mrb_get_args(M, "g", &v); // expected-error {{Invalid format specifier: 'g'}}
  mrb_get_args(M, "o", &b); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "C", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "S", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "A", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "H", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "&", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "b", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "z", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "f", &type); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "i", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "n", &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "|b?", &b, &v); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "b?|", &f, &b); // expected-error {{'?' format specifier must come after '|' format specifier.}}
  mrb_get_args(M, "*", &argv, &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "s", &c, &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_get_args(M, "a", &argv, &f); // expected-error {{Wrong argument passed to variadic mruby C API.}}

  // argument count
  mrb_get_args(M, "|", &v); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 2, Actual: 3}}
  mrb_get_args(M, MRB_ARGS_ALL_TYPE); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 22, Actual: 2}}

  // non pointer argument
  mrb_get_args(M, "o", v); // expected-error {{Variadic argument of `mrb_get_args` must be a pointer.}}
}

void test_mrb_intern_lit(mrb_state* M) {
  char const* str = "test";
  char const str_ary[] = "test";
  mrb_intern_cstr(M, str);
  mrb_intern_cstr(M, str_ary);
  mrb_intern_cstr(M, "test"); // expected-warning {{`mrb_intern_lit` is preferred when getting symbol from string literal.}}
}

void test_mrb_str_new_lit(mrb_state *M) {
  char const* str = "test";
  char const str_ary[] = "test";
  mrb_str_new_cstr(M, str);
  mrb_str_new_cstr(M, str_ary);
  mrb_str_new_cstr(M, "test"); // expected-warning {{`mrb_str_new_lit` is preferred when creating string object from literal.}}
}

void test_funcall(mrb_state* M) {
  mrb_value v;
  mrb_int i;
  mrb_funcall(M, v, "func", 0);
  mrb_funcall(M, v, "func", 0, v); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 4, Actual: 5}}
  mrb_funcall(M, v, "func", 2); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 6, Actual: 4}}
  mrb_funcall(M, v, "func", 1, i); // expected-error {{Wrong argument passed to variadic mruby C API.}}
}

void test_message_funcs(mrb_state* M) {
  mrb_value v;
  mrb_int i;
  mrb_warn(M, "func");
  mrb_name_error(M, mrb_intern_lit(M, "func"), "test", v); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 3, Actual: 4}}
  mrb_raisef(M, mrb_class_get(M, "RuntimeError"), "func", 2); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 3, Actual: 4}}
  mrb_bug(M, "func %S", i); // expected-error {{Wrong argument passed to variadic mruby C API.}}
  mrb_no_method_error(M, mrb_intern_lit(M, "func"), 0, NULL, "test %S"); // expected-error {{Wrong number of arguments passed to variadic mruby C API. Expected: 6, Actual: 5}}
}
