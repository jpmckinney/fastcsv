# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name        = "fastcsv"
  s.version     = '0.0.4'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Open North"]
  s.email       = ["info@opennorth.ca"]
  s.homepage    = "http://github.com/opennorth/fastcsv"
  s.summary     = %q{A fast Ragel-based CSV parser, compatible with Ruby's CSV}
  s.license     = 'MIT'

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]
  s.extensions    = ["ext/fastcsv/extconf.rb"]

  s.add_development_dependency('coveralls')
  s.add_development_dependency('json', '~> 1.7.7') # to silence coveralls warning
  s.add_development_dependency('rake')
  s.add_development_dependency('rake-compiler')
  s.add_development_dependency('rspec', '~> 3.1')
end
