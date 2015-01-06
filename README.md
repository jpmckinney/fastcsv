# FastCSV

[![Gem Version](https://badge.fury.io/rb/fastcsv.svg)](http://badge.fury.io/rb/fastcsv)
[![Build Status](https://secure.travis-ci.org/jpmckinney/fastcsv.png)](http://travis-ci.org/jpmckinney/fastcsv)
[![Dependency Status](https://gemnasium.com/jpmckinney/fastcsv.png)](https://gemnasium.com/jpmckinney/fastcsv)
[![Coverage Status](https://coveralls.io/repos/jpmckinney/fastcsv/badge.png?branch=master)](https://coveralls.io/r/jpmckinney/fastcsv)
[![Code Climate](https://codeclimate.com/github/jpmckinney/fastcsv.png)](https://codeclimate.com/github/jpmckinney/fastcsv)

A fast [Ragel](http://www.colm.net/open-source/ragel/)-based CSV parser, compatible with Ruby's CSV.

**Only reads CSVs using `"` as the quote character, `,` as the delimiter and `\r`, `\n` or `\r\n` as the line terminator.**

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

FastCSV can be used as a drop-in replacement for [CSV](http://ruby-doc.org/stdlib-2.1.1/libdoc/csv/rdoc/CSV.html) (replace `CSV` with `FastCSV`) except:

* The `:quote_char` (`"`), `:col_sep` (`,`) and `:row_sep` (`:auto`) options are ignored. [#2](https://github.com/jpmckinney/fastcsv/issues/2)
* If FastCSV raises an error, you can't continue reading. [#3](https://github.com/jpmckinney/fastcsv/issues/3) Its error messages don't perfectly match those of CSV.

A few minor caveats:

* Use `FastCSV.parse_line(string, options)` instead of `string.parse_csv(options)`.
* If you were passing CSV an IO object on which you had wrapped `#gets` (for example, as described in [this article](http://graysoftinc.com/rubies-in-the-rough/decorators-verses-the-mix-in)), `#gets` will not be called.
* The `:field_size_limit` option is ignored. If you need to prevent DoS attacks – the [ostensible reason](http://ruby-doc.org/stdlib-2.1.1/libdoc/csv/rdoc/CSV.html#new-method) for this option – limit the size of the input, not the size of quoted fields.
* FastCSV doesn't support UTF-16 or UTF-32. See [UTF-8 Everywhere](http://utf8everywhere.org/).

## Development

    ragel -G2 ext/fastcsv/fastcsv.rl
    ragel -Vp ext/fastcsv/fastcsv.rl | dot -Tpng -o machine.png
    rake compile
    gem uninstall fastcsv
    rake install
    rake
    rspec test/runner.rb test/csv

### Implementation

FastCSV implements its Ragel-based CSV parser in C at `FastCSV::Parser`.

FastCSV is a subclass of [CSV](http://ruby-doc.org/stdlib-2.1.1/libdoc/csv/rdoc/CSV.html). It overrides `#shift`, replacing the parsing code, in order to act as a drop-in replacement.

FastCSV's `raw_parse` requires a block to which it yields one row at a time. FastCSV uses [Fiber](http://www.ruby-doc.org/core-2.1.1/Fiber.html)s to pass control back to `#shift` while parsing.

CSV delegates IO methods to the IO object it's reading. IO methods that move the pointer within the file like `rewind` changes the behavior of CSV's `#shift`. However, FastCSV's C code won't take notice. We therefore null the Fiber whenever the pointer is moved, so that `#shift` uses a new Fiber.

CSV's `#shift` runs the regular expression in the `:skip_lines` option against a row's raw text. `FastCSV::Parser` implements a `row` method, which returns the most recently parsed row's raw text.

FastCSV is tested against the same tests as CSV. See [TESTS.md](https://github.com/jpmckinney/fastcsv/blob/master/TESTS.md) for details.

## Why?

We evaluated [many CSV Ruby gems](https://github.com/jpmckinney/csv-benchmark#benchmark), and they were either too slow or had implementation errors. [rcsv](https://github.com/fiksu/rcsv) is fast and [libcsv](http://sourceforge.net/projects/libcsv/)-based, but it skips blank rows (Ruby's CSV module returns an empty array) and silently fails on input with an unclosed quote. [bamfcsv](https://github.com/jondistad/bamfcsv) is well implemented, but it's considerably slower on large files. We looked for Ragel-based CSV parsers to copy, but they either had implementation errors or could not handle large files. [commas](https://github.com/aklt/commas/blob/master/csv.rl) looks good, but it performs a memory check on each character, which is overkill.

## Acknowledgements

Started as a Ruby 2.1 fork of MoonWolf <moonwolf@moonwolf.com>'s CSVScan, found in [this commit](https://github.com/nickstenning/csvscan/commit/11ec30f71a27cc673bca09738ee8a63942f416f0.patch). CSVScan uses Ragel code from [HPricot](https://github.com/hpricot/hpricot/blob/master/ext/hpricot_scan/hpricot_scan.rl) from [this commit](https://github.com/hpricot/hpricot/blob/908a4ae64bc8b935c4415c47ca6aea6492c6ce0a/ext/hpricot_scan/hpricot_scan.rl). Most of the Ruby (i.e. non-C, non-Ragel) methods are copied from [CSV](https://github.com/ruby/ruby/blob/ab337e61ecb5f42384ba7d710c36faf96a454e5c/lib/csv.rb).

Copyright (c) 2014 James McKinney, released under the MIT license
