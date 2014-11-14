Here are some notes on maintaining the `test/` directory.

1. Download Ruby and [test CSV](http://ruby-doc.org/core-2.1.0/doc/contributing_rdoc.html#label-Running+tests).

        git clone https://github.com/ruby/ruby.git
        cd ruby
        git co v2_1_2
        gem uninstall minitest
        gem install minitest --version 4.7.5
        ruby test/runner.rb test/csv 

1. Copy the tests into the project. All the tests should pass.

        cd PROJECT
        mkdir test
        cp path/to/ruby/test/runner.rb test
        cp path/to/ruby/test/with_different_ofs.rb test
        cp -r path/to/ruby/test/csv test/csv
        ruby test/runner.rb test/csv

1. Replace `\bCSV\b` with `FastCSV`. And run:

        sed -i.bak '1s;^;require "fastcsv"\
        ;' test/runner.rb

1. In `test_interface.rb`, replace `\\t|;|(?<=\S)\|(?=\S)` with `,`. In `test_encodings.rb`, replace `(?<=[^\s{])\|(?=\S)` with `,` and replace `Encoding.list` with `Encoding.list.reject{|e| e.name[/\AUTF-\d\d/]}`. These changes are because `:col_sep`, `:row_sep` and `:quote_char` are ignored and because UTF-16 and UTF-32 aren't supported.

1. Comment these tests because `:col_sep`, `:row_sep` and `:quote_char` are ignored:

  * `test_csv_parsing.rb`: the first part of `test_malformed_csv`
  * `test_features.rb`: `test_col_sep`, `test_row_sep`, `test_quote_char`, `test_leading_empty_fields_with_multibyte_col_sep_bug_fix`
  * `test_headers.rb`: `test_csv_header_string_inherits_separators`

1. Comment these tests in `test_csv_encoding.rb` because UTF-16 and UTF-32 aren't supported:

  * `test_parses_utf16be_encoding`
  * the second part of `test_open_allows_you_to_set_encodings`
  * the second part of `test_foreach_allows_you_to_set_encodings`
  * the second part of `test_read_allows_you_to_set_encodings`
  * the second line of `encode_for_tests`

1. Comment `test_field_size_limit_controls_lookahead` in `test_csv_parsing.rb` (`:field_size_limit` not supported). FastCSV reads one more line than CSV in `test_malformed_csv`, but not sure that's worth mirroring.
