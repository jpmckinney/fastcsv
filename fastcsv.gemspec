# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name        = "fastcsv"
  s.version     = '0.0.9'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["James McKinney"]
  s.homepage    = "https://github.com/jpmckinney/fastcsv"
  s.summary     = %q{A fast Ragel-based CSV parser, compatible with Ruby's CSV}
  s.license     = 'MIT'

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]
  s.extensions    = ["ext/fastcsv/extconf.rb"]
  s.required_ruby_version = '< 2.6'

  s.add_development_dependency('coveralls')
  s.add_development_dependency('rake')
  s.add_development_dependency('rspec', '~> 3.1')

  s.add_development_dependency('rake-compiler')
end
