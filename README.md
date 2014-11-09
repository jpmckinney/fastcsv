# CSVScan2

[![Gem Version](https://badge.fury.io/rb/csvscan2.svg)](http://badge.fury.io/rb/csvscan2)
[![Dependency Status](https://gemnasium.com/opennorth/csvscan2.png)](https://gemnasium.com/opennorth/csvscan2)

Ruby 2.1 fork of MoonWolf <moonwolf@moonwolf.com>'s CSVScan, found in [this commit](https://github.com/nickstenning/csvscan/commit/11ec30f71a27cc673bca09738ee8a63942f416f0.patch). CSVScan uses Ragel code from [HPricot](https://github.com/hpricot/hpricot/blob/master/ext/hpricot_scan/hpricot_scan.rl) from [this commit](https://github.com/hpricot/hpricot/blob/908a4ae64bc8b935c4415c47ca6aea6492c6ce0a/ext/hpricot_scan/hpricot_scan.rl).

## Usage

```ruby
require 'csvscan2'

open(ARGV.shift) do |io|
  CSVScan.scan(io) do |row|
    p row
  end
end
```

## Development

    ragel ext/csvscan2/csvscan2.rl
    rake compile
    rake install

## Bugs? Questions?

This project's main repository is on GitHub: [http://github.com/opennorth/csvscan2](http://github.com/opennorth/csvscan2), where your contributions, forks, bug reports, feature requests, and feedback are greatly welcomed.

Copyright (c) 2014 Open North Inc., released under the MIT license
