mruby-clang-plugin
==================
clang plugin to check mruby C API call that could be unsafe.

Howto use
---------
Add following line to `build_config.rb`:
```ruby
  conf.gem :github => 'take-cheeze/mruby-clang-plugin'
```

What it checks
--------------
* wrong use of variadic mruby C API such as `mrb_get_args`, `mrb_raisef`, `mrb_funcall` etc.
* 2nd argument of `mrb_intern_cstr` which is string literals. `mrb_intern_lit` is preferred in that case.

Where it works
--------------
 I've tested it on OS X Mavericks and Debian with clang-3.3.
 It isn't tested in C++ code so please [create an issue](https://github.com/take-cheeze/mruby-clang-plugin/issues/new) about it
when you get wrong behavior.

License
-------
The MIT License (MIT)

Copyright (c) 2014 Takeshi Watanabe (takechi101010@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
