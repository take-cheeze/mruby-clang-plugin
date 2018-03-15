MRuby::Gem::Specification.new('mruby-clang-plugin') do |spec|
  spec.author = 'Takeshi Watanabe'
  spec.license = 'MIT'
  spec.summary = 'clang plugin to check mruby API call mistakes.'

  spec.objs = spec.objs.delete_if{|v| File.basename(v).include? 'mruby_clang_checker' }
  spec.test_objs = []

  unless spec.build.toolchains.include?('clang')
     print "Skipping non-clang toolchain.\n"
     next
  end
  if spec.build.kind_of? MRuby::CrossBuild
     print "Skipping cross build.\n"
     next
  end

  so_pos = File.expand_path "#{build.build_dir}/../host/mrbgems/mruby-clang-plugin/libmruby-clang-checker.so"

  plugin_flags = %W[-Xclang -load -Xclang #{so_pos} -Xclang -add-plugin -Xclang mruby-clang-checker]
  if Object.const_defined? :HAS_HOST_MRUBY_CLANG_PLUGIN
    build.cc.flags += plugin_flags
    build.cxx.flags += plugin_flags
    next
  end

  fail 'llvm-config not found' unless system 'llvm-config --version'
  fail 'cmake not found' unless system 'cmake --version'

  HAS_HOST_MRUBY_CLANG_PLUGIN = true
  build.cc.flags += plugin_flags
  build.cxx.flags += plugin_flags

  file "#{build_dir}/CMakeCache.txt" do |t|
    FileUtils.mkdir_p build_dir
    Dir.chdir build_dir do
      sh "cmake -DCMAKE_BUILD_TYPE=DEBUG -DMRUBY_ROOT=#{MRUBY_ROOT} #{dir}"
    end
  end

  task "#{build.name}_clang_plugin" => "#{build_dir}/CMakeCache.txt" do |t|
    sh "make -C #{build_dir} all"
  end

  task :test do
    sh "make -C #{build_dir} test"
  end

  file "#{build.build_dir}/lib/libmruby.flags.mak" => "#{build.name}_clang_plugin"
end
