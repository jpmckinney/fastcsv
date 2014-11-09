# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name        = "csvscan2"
  s.version     = '0.0.1'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Open North"]
  s.email       = ["info@opennorth.ca"]
  s.homepage    = "http://github.com/opennorth/csvscan2"
  s.summary     = %q{A Ragel-based CSV parser}
  s.license     = 'MIT'

  s.files         = `git ls-files`.split("\n")
  s.extensions    = ['ext/csvscan2/extconf.rb']

  s.add_development_dependency('rake')
  s.add_development_dependency('rake-compiler')
end
