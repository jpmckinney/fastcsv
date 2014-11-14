#!/usr/bin/env ruby -w
# encoding: UTF-8

# tc_interface.rb
#
#  Created by James Edward Gray II on 2005-10-31.
#  Copyright 2005 James Edward Gray II. You can redistribute or modify this code
#  under the terms of Ruby's license.

require_relative "base"
require "tempfile"

class TestCSV::Interface < TestCSV
  extend DifferentOFS

  def setup
    super
    @tempfile = Tempfile.new(%w"temp .csv")
    @tempfile.close
    @path = @tempfile.path

    File.open(@path, "wb") do |file|
      file << "1,2,3\r\n"
      file << "4,5\r\n"
    end

    @expected = [%w{1 2 3}, %w{4 5}]
  end

  def teardown
    @tempfile.close(true)
    super
  end

  ### Test Read Interface ###

  def test_foreach
    FastCSV.foreach(@path, col_sep: ",", row_sep: "\r\n") do |row|
      assert_equal(@expected.shift, row)
    end
  end

  def test_foreach_enum
    FastCSV.foreach(@path, col_sep: ",", row_sep: "\r\n").zip(@expected) do |row, exp|
      assert_equal(exp, row)
    end
  end

  def test_open_and_close
    csv = FastCSV.open(@path, "r+", col_sep: ",", row_sep: "\r\n")
    assert_not_nil(csv)
    assert_instance_of(FastCSV, csv)
    assert_equal(false, csv.closed?)
    csv.close
    assert(csv.closed?)

    ret = FastCSV.open(@path) do |new_csv|
      csv = new_csv
      assert_instance_of(FastCSV, new_csv)
      "Return value."
    end
    assert(csv.closed?)
    assert_equal("Return value.", ret)
  end

  def test_parse
    data = File.binread(@path)
    assert_equal( @expected,
                  FastCSV.parse(data, col_sep: ",", row_sep: "\r\n") )

    FastCSV.parse(data, col_sep: ",", row_sep: "\r\n") do |row|
      assert_equal(@expected.shift, row)
    end
  end

  def test_parse_line
    row = FastCSV.parse_line("1,2,3", col_sep: ",")
    assert_not_nil(row)
    assert_instance_of(Array, row)
    assert_equal(%w{1 2 3}, row)

    # shortcut interface
    row = "1,2,3".parse_csv(col_sep: ",")
    assert_not_nil(row)
    assert_instance_of(Array, row)
    assert_equal(%w{1 2 3}, row)
  end

  def test_parse_line_with_empty_lines
    assert_equal(nil,       FastCSV.parse_line(""))  # to signal eof
    assert_equal(Array.new, FastCSV.parse_line("\n1,2,3"))
  end

  def test_read_and_readlines
    assert_equal( @expected,
                  FastCSV.read(@path, col_sep: ",", row_sep: "\r\n") )
    assert_equal( @expected,
                  FastCSV.readlines(@path, col_sep: ",", row_sep: "\r\n") )


    data = FastCSV.open(@path, col_sep: ",", row_sep: "\r\n") do |csv|
      csv.read
    end
    assert_equal(@expected, data)
    data = FastCSV.open(@path, col_sep: ",", row_sep: "\r\n") do |csv|
      csv.readlines
    end
    assert_equal(@expected, data)
  end

  def test_table
    table = FastCSV.table(@path, col_sep: ",", row_sep: "\r\n")
    assert_instance_of(FastCSV::Table, table)
    assert_equal([[:"1", :"2", :"3"], [4, 5, nil]], table.to_a)
  end

  def test_shift  # aliased as gets() and readline()
    FastCSV.open(@path, "rb+", col_sep: ",", row_sep: "\r\n") do |csv|
      assert_equal(@expected.shift, csv.shift)
      assert_equal(@expected.shift, csv.shift)
      assert_equal(nil, csv.shift)
    end
  end

  def test_enumerators_are_supported
    FastCSV.open(@path, col_sep: ",", row_sep: "\r\n") do |csv|
      enum = csv.each
      assert_instance_of(Enumerator, enum)
      assert_equal(@expected.shift, enum.next)
    end
  end

  ### Test Write Interface ###

  def test_generate
    str = FastCSV.generate do |csv|  # default empty String
      assert_instance_of(FastCSV, csv)
      assert_equal(csv, csv << [1, 2, 3])
      assert_equal(csv, csv << [4, nil, 5])
    end
    assert_not_nil(str)
    assert_instance_of(String, str)
    assert_equal("1,2,3\n4,,5\n", str)

    FastCSV.generate(str) do |csv|   # appending to a String
      assert_equal(csv, csv << ["last", %Q{"row"}])
    end
    assert_equal(%Q{1,2,3\n4,,5\nlast,"""row"""\n}, str)
  end

  def test_generate_line
    line = FastCSV.generate_line(%w{1 2 3}, col_sep: ",")
    assert_not_nil(line)
    assert_instance_of(String, line)
    assert_equal("1,2,3\n", line)

    # shortcut interface
    line = %w{1 2 3}.to_csv(col_sep: ",")
    assert_not_nil(line)
    assert_instance_of(String, line)
    assert_equal("1,2,3\n", line)
  end

  def test_write_header_detection
    File.unlink(@path)

    headers = %w{a b c}
    FastCSV.open(@path, "w", headers: true) do |csv|
      csv << headers
      csv << %w{1 2 3}
      assert_equal(headers, csv.instance_variable_get(:@headers))
    end
  end

  def test_write_lineno
    File.unlink(@path)

    FastCSV.open(@path, "w") do |csv|
      lines = 20
      lines.times { csv << %w{a b c} }
      assert_equal(lines, csv.lineno)
    end
  end

  def test_write_hash
    File.unlink(@path)

    lines = [{a: 1, b: 2, c: 3}, {a: 4, b: 5, c: 6}]
    FastCSV.open( @path, "wb", headers:           true,
                           header_converters: :symbol ) do |csv|
      csv << lines.first.keys
      lines.each { |line| csv << line }
    end
    FastCSV.open( @path, "rb", headers:           true,
                           converters:        :all,
                           header_converters: :symbol ) do |csv|
      csv.each { |line| assert_equal(lines.shift, line.to_hash) }
    end
  end

  def test_write_hash_with_string_keys
    File.unlink(@path)

    lines = [{a: 1, b: 2, c: 3}, {a: 4, b: 5, c: 6}]
    FastCSV.open( @path, "wb", headers: true ) do |csv|
      csv << lines.first.keys
      lines.each { |line| csv << line }
    end
    FastCSV.open( @path, "rb", headers: true ) do |csv|
      csv.each do |line|
        csv.headers.each_with_index do |header, h|
          keys = line.to_hash.keys
          assert_instance_of(String, keys[h])
          assert_same(header, keys[h])
        end
      end
    end
  end

  def test_write_hash_with_headers_array
    File.unlink(@path)

    lines = [{a: 1, b: 2, c: 3}, {a: 4, b: 5, c: 6}]
    FastCSV.open(@path, "wb", headers: [:b, :a, :c]) do |csv|
      lines.each { |line| csv << line }
    end

    # test writing fields in the correct order
    File.open(@path, "rb") do |f|
      assert_equal("2,1,3", f.gets.strip)
      assert_equal("5,4,6", f.gets.strip)
    end

    # test reading FastCSV with headers
    FastCSV.open( @path, "rb", headers:    [:b, :a, :c],
                           converters: :all ) do |csv|
      csv.each { |line| assert_equal(lines.shift, line.to_hash) }
    end
  end

  def test_write_hash_with_headers_string
    File.unlink(@path)

    lines = [{"a" => 1, "b" => 2, "c" => 3}, {"a" => 4, "b" => 5, "c" => 6}]
    FastCSV.open(@path, "wb", headers: "b,a,c", col_sep: ",") do |csv|
      lines.each { |line| csv << line }
    end

    # test writing fields in the correct order
    File.open(@path, "rb") do |f|
      assert_equal("2,1,3", f.gets.strip)
      assert_equal("5,4,6", f.gets.strip)
    end

    # test reading FastCSV with headers
    FastCSV.open( @path, "rb", headers:    "b,a,c",
                           col_sep:    ",",
                           converters: :all ) do |csv|
      csv.each { |line| assert_equal(lines.shift, line.to_hash) }
    end
  end

  def test_write_headers
    File.unlink(@path)

    lines = [{"a" => 1, "b" => 2, "c" => 3}, {"a" => 4, "b" => 5, "c" => 6}]
    FastCSV.open( @path, "wb", headers:       "b,a,c",
                           write_headers: true,
                           col_sep:       "," ) do |csv|
      lines.each { |line| csv << line }
    end

    # test writing fields in the correct order
    File.open(@path, "rb") do |f|
      assert_equal("b,a,c", f.gets.strip)
      assert_equal("2,1,3", f.gets.strip)
      assert_equal("5,4,6", f.gets.strip)
    end

    # test reading FastCSV with headers
    FastCSV.open( @path, "rb", headers:    true,
                           col_sep:    ",",
                           converters: :all ) do |csv|
      csv.each { |line| assert_equal(lines.shift, line.to_hash) }
    end
  end

  def test_append  # aliased add_row() and puts()
    File.unlink(@path)

    FastCSV.open(@path, "wb", col_sep: ",", row_sep: "\r\n") do |csv|
      @expected.each { |row| csv << row }
    end

    test_shift

    # same thing using FastCSV::Row objects
    File.unlink(@path)

    FastCSV.open(@path, "wb", col_sep: ",", row_sep: "\r\n") do |csv|
      @expected.each { |row| csv << FastCSV::Row.new(Array.new, row) }
    end

    test_shift
  end

  ### Test Read and Write Interface ###

  def test_filter
    assert_respond_to(FastCSV, :filter)

    expected = [[1, 2, 3], [4, 5]]
    FastCSV.filter( "1,2,3\n4,5\n", (result = String.new),
                in_col_sep: ",", out_col_sep: ",",
                converters: :all ) do |row|
      assert_equal(row, expected.shift)
      row.map! { |n| n * 2 }
      row << "Added\r"
    end
    assert_equal("2,4,6,\"Added\r\"\n8,10,\"Added\r\"\n", result)
  end

  def test_instance
    csv = String.new

    first = nil
    assert_nothing_raised(Exception) do
      first =  FastCSV.instance(csv, col_sep: ",")
      first << %w{a b c}
    end

    assert_equal("a,b,c\n", csv)

    second = nil
    assert_nothing_raised(Exception) do
      second =  FastCSV.instance(csv, col_sep: ",")
      second << [1, 2, 3]
    end

    assert_equal(first.object_id, second.object_id)
    assert_equal("a,b,c\n1,2,3\n", csv)

    # shortcuts
    assert_equal(STDOUT, FastCSV.instance.instance_eval { @io })
    assert_equal(STDOUT, FastCSV { |new_csv| new_csv.instance_eval { @io } })
  end

  def test_options_are_not_modified
    opt = {}.freeze
    assert_nothing_raised {  FastCSV.foreach(@path, opt)       }
    assert_nothing_raised {  FastCSV.open(@path, opt){}        }
    assert_nothing_raised {  FastCSV.parse("", opt)            }
    assert_nothing_raised {  FastCSV.parse_line("", opt)       }
    assert_nothing_raised {  FastCSV.read(@path, opt)          }
    assert_nothing_raised {  FastCSV.readlines(@path, opt)     }
    assert_nothing_raised {  FastCSV.table(@path, opt)         }
    assert_nothing_raised {  FastCSV.generate(opt){}           }
    assert_nothing_raised {  FastCSV.generate_line([], opt)    }
    assert_nothing_raised {  FastCSV.filter("", "", opt){}     }
    assert_nothing_raised {  FastCSV.instance("", opt)         }
  end
end
