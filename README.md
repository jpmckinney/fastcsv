# FastCSV

[![Gem Version](https://badge.fury.io/rb/fastcsv.svg)](http://badge.fury.io/rb/fastcsv)
[![Dependency Status](https://gemnasium.com/opennorth/fastcsv.png)](https://gemnasium.com/opennorth/fastcsv)

A fast [Ragel](http://www.colm.net/open-source/ragel/)-based CSV parser.

## Usage

`FastCSV.raw_parse` is implemented in C and is the fastest way to read CSVs with FastCSV.

```ruby
require 'fastcsv'

# Read from file.
File.open(filename) do |f|
  FastCSV.raw_parse(f) do |row|
    # do stuff
  end
end

# Read from an IO object.
FastCSV.raw_parse(StringIO.new("foo,bar\n")) do |row|
  # do stuff
end

# Read from a string.
FastCSV.raw_parse("foo,bar\n") do |row|
  # do stuff
end

# Transcode like with the CSV module.
FastCSV.raw_parse("\xF1\n", encoding: 'iso-8859-1:utf-8') do |row|
  # ["ñ"]
end
```

Otherwise, FastCSV can be used as a drop-in replacement for CSV (i.e. simply replace `CSV` with `FastCSV`) with a few important caveats:

* The default `:quote_char` (`"`), `:col_sep` (`,`) and `:row_sep` (`:auto`) options cannot be changed. [[#2]](https://github.com/opennorth/fastcsv/issues/2)
* If FastCSV raises an error, you can't continue reading. [[#3]](https://github.com/opennorth/fastcsv/issues/3)
* FastCSV's error messages don't perfectly match those of CSV, and it doesn't raise an error if row separators are inconsistent. [[#6]](https://github.com/opennorth/fastcsv/issues/6)
* If you were passing CSV an IO object on which you had wrapped `#gets` (for example, as described in [this article](http://graysoftinc.com/rubies-in-the-rough/decorators-verses-the-mix-in), `#gets` will not be called.
* The `:field_size_limit` option is ignored. (If you need to prevent DoS attacks – the [ostensible reason](http://ruby-doc.org/stdlib-2.1.1/libdoc/csv/rdoc/CSV.html#new-method) for this option – limit the size of the input, not the size of quoted fields.)
* `string.parse_csv(options)` will still call CSV. Use `FastCSV.parse_line(string, options)` instead.
* FastCSV only implements fast reading. For writing, you may as well use plain CSV.

## Development

    ragel -G2 ext/fastcsv/fastcsv.rl
    ragel -Vp ext/fastcsv/fastcsv.rl | dot -Tpng -o machine.png
    rake compile
    gem uninstall fastcsv
    rake install

## Why?

We evaluated [many CSV Ruby gems](https://github.com/jpmckinney/csv-benchmark#benchmark), and they were either too slow or had implementation errors. [rcsv](https://github.com/fiksu/rcsv) is fast and [libcsv](http://sourceforge.net/projects/libcsv/)-based, but it skips blank rows (Ruby's CSV module returns an empty array) and silently fails on input with an unclosed quote. [bamfcsv](https://github.com/jondistad/bamfcsv) is well implemented, but it's considerably slower on large files. We looked for Ragel-based CSV parsers to copy, but they either had implementation errors or could not handle large files. [commas](https://github.com/aklt/commas/blob/master/csv.rl) looks good, but it performs a memory check on each character, which is overkill.

## Bugs? Questions?

This project's main repository is on GitHub: [http://github.com/opennorth/fastcsv](http://github.com/opennorth/fastcsv), where your contributions, forks, bug reports, feature requests, and feedback are greatly welcomed.

## Acknowledgements

Started as a Ruby 2.1 fork of MoonWolf <moonwolf@moonwolf.com>'s CSVScan, found in [this commit](https://github.com/nickstenning/csvscan/commit/11ec30f71a27cc673bca09738ee8a63942f416f0.patch). CSVScan uses Ragel code from [HPricot](https://github.com/hpricot/hpricot/blob/master/ext/hpricot_scan/hpricot_scan.rl) from [this commit](https://github.com/hpricot/hpricot/blob/908a4ae64bc8b935c4415c47ca6aea6492c6ce0a/ext/hpricot_scan/hpricot_scan.rl). Most of the Ruby (i.e. non-C, non-Ragel) methods are copied from [CSV](https://github.com/ruby/ruby/blob/ab337e61ecb5f42384ba7d710c36faf96a454e5c/lib/csv.rb).

Copyright (c) 2014 Open North Inc., released under the MIT license
