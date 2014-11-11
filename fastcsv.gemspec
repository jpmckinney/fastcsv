# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name        = "fastcsv"
  s.version     = '0.0.1'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Open North"]
  s.email       = ["info@opennorth.ca"]
  s.homepage    = "http://github.com/opennorth/fastcsv"
  s.summary     = %q{A Ragel-based CSV parser}
  s.license     = 'MIT'

  s.files         = `git ls-files`.split("\n")
  s.extensions    = ['ext/fastcsv/extconf.rb']

  s.add_development_dependency('rake')
  s.add_development_dependency('rake-compiler')
end
