require 'spec_helper'

require 'csv'

RSpec.shared_examples 'a CSV parser' do
  let :simple do
    "foo\nbar\nbaz"
  end

  [
    # Single tokens.
    "",
    "x",
    %(""),
    %("x"),
    ",",
    "\n",

    # Last tokens.
    "x,y",
    %(x,"y"),
    "x,",
    "x\n",

    # Line endings.
    "\n\n\n",
    "\r\r\r",
    "\r\n\r\n\r\n",
    "foo\rbar\rbaz\r",
    "foo\nbar\nbaz\n",
    "foo\r\nbar\r\nbaz\r\n",

    # Repetition.
    "x,x,x",
    "x\nx\nx",
    %("x","x","x"),
    %("x"\n"x"\n"x"),
    ",,,",
    ",\n,\n,",

    # Blank.
    %(,""),
    %("",),
    "\n\n\nfoo\n\n\n",

    # Whitespace.
    "   x",
    "x   ",
    "   x   ",
    # Tab.
    " x",
    "x  ",
    "  x  ",

    # Quoting.
    %(foo,"bar,baz",bzz),
    %(foo,"bar\nbaz",bzz),
    %(foo,"""bar""baz""bzz""",zzz),

    # Buffers.
    "01234567890" * 2_000, # 20,000 > BUFSIZE
    "0123456789," * 2_000,

    # Uneven rows.
    "1,2,3\n1,2",
    "1,2\n1,2,3",

    # Uneven data types.
    "2000-01-01,2,x\nx,2000-01-01,2",
  ].each do |csv|
    it "should parse: #{csv}" do
      expect(parse(csv)).to eq(CSV.parse(csv))
    end
  end

  [
    # Whitespace.
    # @note Ruby's CSV library has inexplicably inconsistent error messages for
    #   the same class of error.
    [%(   "x"), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],
    [%("x"   ), 'Unclosed quoted field on line %d.', 'Illegal quoting in line %d.'],
    [%(   "x"   ), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],
    # Tab.
    [%(	"x"), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],
    [%("x"	), 'Unclosed quoted field on line %d.', 'Illegal quoting in line %d.'],
    [%(	"x"	), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],

    # Quoted next to unquoted.
    [%("x"x), 'Unclosed quoted field on line %d.', 'Illegal quoting in line %d.'],
    [%(x"x"), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],
    [%(x"x"x), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],
    [%("x"x"x"), 'Missing or stray quote in line %d', 'Illegal quoting in line %d.'],

    # Unclosed quote.
    [%("x), 'Unclosed quoted field on line %d.', 'Unclosed quoted field on line %d.'],

    # Quote in unquoted field.
    [%(x"x), 'Illegal quoting in line %d.', 'Illegal quoting in line %d.'],

    # Unescaped quote in quoted field.
    [%("x"x"), 'Unclosed quoted field on line %d.', 'Illegal quoting in line %d.'],
  ].each do |csv,csv_error,fastcsv_error|
    it "should raise an error on: #{csv.inspect.gsub('\"', '"')}" do
      expect{CSV.parse(csv)}.to raise_error(CSV::MalformedCSVError, csv_error % 1)
      expect{parse(csv)}.to raise_error(FastCSV::ParseError, fastcsv_error % 1)
    end

    it "should raise an error with the correct line number on: #{"\n#{csv}\n".inspect.gsub('\"', '"')}" do
      csv = "\n#{csv}\n"
      expect{CSV.parse(csv)}.to raise_error(CSV::MalformedCSVError, csv_error % 2)
      expect{parse(csv)}.to raise_error(FastCSV::ParseError, fastcsv_error % 2)
    end
  end

  it 'should raise an error on mixed row separators are' do
    csv = "foo\rbar\nbaz\r\n"
    expect{CSV.parse(csv)}.to raise_error(CSV::MalformedCSVError, 'Unquoted fields do not allow \r or \n (line 2).')
    skip
  end

  it 'should raise an error if no block is given' do
    expect{parse_without_block('x')}.to raise_error(LocalJumpError, 'no block given')
  end

  it 'should not raise an error if no block and empty input' do
    expect{parse_without_block('')}.to_not raise_error
  end

  it 'should raise an error if the options are not a Hash or nil' do
    expect{parse('', '')}.to raise_error(ArgumentError, 'options has to be a Hash or nil')
  end

  it 'should allow nil buffer size' do
    FastCSV.buffer_size = nil
    expect(parse(simple)).to eq(CSV.parse(simple))
    FastCSV.buffer_size = nil
  end

  it 'should recover from a zero buffer size' do
    FastCSV.buffer_size = 0
    expect(parse(simple)).to eq(CSV.parse(simple))
    FastCSV.buffer_size = nil
  end
end

RSpec.describe FastCSV do
  context "with String" do
    def parse(csv, options = nil)
      rows = []
      FastCSV.raw_parse(csv, options){|row| rows << row}
      rows
    end

    def parse_without_block(csv, options = nil)
      FastCSV.raw_parse(csv, options)
    end

    include_examples 'a CSV parser'

    it 'should not raise an error on negative buffer size' do
      FastCSV.buffer_size = -1
      expect{parse(simple)}.to_not raise_error
      FastCSV.buffer_size = nil
    end
  end

  context "with StringIO" do
    def parse(csv, options = nil)
      rows = []
      FastCSV.raw_parse(StringIO.new(csv), options){|row| rows << row}
      rows
    end

    def parse_without_block(csv, options = nil)
      FastCSV.raw_parse(StringIO.new(csv), options)
    end

    include_examples 'a CSV parser'

    it 'should raise an error on negative buffer size' do
      FastCSV.buffer_size = -1
      expect{parse(simple)}.to raise_error(NoMemoryError)
      FastCSV.buffer_size = nil
    end
  end

  def parse_with_encoding(basename, encoding)
    filename = File.expand_path(File.join('..', 'fixtures', basename), __FILE__)
    options = {encoding: encoding}
    File.open(filename) do |io|
      rows = []
      FastCSV.raw_parse(io, options){|row| rows << row}
      expected = CSV.read(filename, options)
      expect(rows).to eq(expected)
      expect(rows[0][0].encoding).to eq(expected[0][0].encoding)
    end
  end

  it 'should encode the input' do
    parse_with_encoding('iso-8859-1.csv', 'iso-8859-1')
  end

  it 'should encode the input with a blank internal encoding' do
    parse_with_encoding('utf-8.csv', ':utf-8')
  end

  it 'should transcode the input' do
    parse_with_encoding('iso-8859-1.csv', 'iso-8859-1:utf-8')
  end

  it 'should invalid encoding' do
    parse_with_encoding('utf-8.csv', 'invalid')
  end

  it 'should raise an error if the input is not a String or IO' do
    expect{FastCSV.raw_parse(nil)}.to raise_error(ArgumentError, 'data has to respond to #read or #to_str')
  end
end
