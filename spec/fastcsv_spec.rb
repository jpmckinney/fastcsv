require 'spec_helper'

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

    # Single quotes.
    %('foo','bar','baz'),

    # Buffers.
    "0123456789," * 2_000,

    # Uneven rows.
    "1,2,3\n1,2",
    "1,2\n1,2,3",

    # Uneven data types.
    "2000-01-01,2,x\nx,2000-01-01,2",
  ].each do |csv|
    it "should parse: #{csv.inspect.gsub('\"', '"')}" do
      expect(parse(csv)).to eq(CSV.parse(csv))
    end
  end

  # This has caused segmentation faults in the StringIO context in the past, so
  # we separate it out so that it's easier to special case this spec. The fault
  # seems to occur less frequently when the spec is run in isolation. The
  # "TypeError: no implicit conversion from nil to integer" exception after a
  # fault is related to RSpec, not the fault.
  it "should parse long rows" do
    csv = "01234567890" * 2_000 # 20,000 > BUFSIZE
    expect(parse(csv)).to eq(CSV.parse(csv))
  end

  [
    # Whitespace.
    # @note Ruby's CSV library has inexplicably inconsistent error messages for
    #   the same class of error.
    #
    # * "Missing or stray quote in line %d" if quoted field matches /[^"]"[^"]/,
    #   for any quote char.
    # * "Unquoted fields do not allow \r or \n (line \d)" if unquoted field
    #   contains "\r" or "\n", e.g. if `:row_sep` is "\n" but file uses "\r"
    # * "Illegal quoting in line %d" if unquoted field contains quote char.
    # * "Unclosed quoted field on line %d" if reaches EOF without closing.
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
      expect{parse(csv)}.to raise_error(FastCSV::MalformedCSVError, fastcsv_error % 1)
    end

    it "should raise an error with the correct line number on: #{"\n#{csv}\n".inspect.gsub('\"', '"')}" do
      csv = "\n#{csv}\n"
      expect{CSV.parse(csv)}.to raise_error(CSV::MalformedCSVError, csv_error % 2)
      expect{parse(csv)}.to raise_error(FastCSV::MalformedCSVError, fastcsv_error % 2)
    end
  end

  it "should parse an encoded string" do
    csv = "ß"
    actual = parse(csv)
    expected = CSV.parse(csv)
    expect(actual[0][0].encoding).to eq(expected[0][0].encoding)
    expect(actual).to eq(expected)
  end

  it 'should raise an error on mixed row separators' do
    expect{CSV.parse("foo\rbar\nbaz\r\n")}.to raise_error(CSV::MalformedCSVError, 'Unquoted fields do not allow \r or \n (line 2).')
    skip
  end

  context 'when initializing' do
    it 'should raise an error if no block is given' do
      expect{parse_without_block('x')}.to raise_error(LocalJumpError, 'no block given')
    end

    it 'should not raise an error if no block and empty input' do
      expect{parse_without_block('')}.to_not raise_error
    end

    it 'should raise an error if the options are not a Hash or nil' do
      expect{parse('', '')}.to raise_error(ArgumentError, 'options has to be a Hash or nil')
    end
  end

  context 'when setting a buffer size' do
    def parse_with_buffer_size(csv, buffer_size)
      parser = FastCSV::Parser.new
      parser.buffer_size = buffer_size
      rows = parse(csv, nil, parser)
      parser.buffer_size = nil
      rows
    end

    it 'should allow nil' do
      expect(parse_with_buffer_size(simple, nil)).to eq(CSV.parse(simple))
    end

    # If buffer_size is actually set to 0, it can cause segmentation faults.
    it 'should allow zero' do
      expect(parse_with_buffer_size(simple, 0)).to eq(CSV.parse(simple))
    end
  end
end

RSpec.shared_examples 'with encoded strings' do
  def parse_with_encoding(basename, encoding)
    filename = File.expand_path(File.join('..', 'fixtures', basename), __FILE__)
    options = {encoding: encoding}
    File.open(filename) do |io|
      rows = []
      FastCSV.raw_parse(io, options){|row| rows << row}
      expected = CSV.read(filename, options)
      expect(rows[0][0].encoding).to eq(expected[0][0].encoding)
      expect(rows).to eq(expected)
    end
  end

  it 'should encode with internal encoding' do
    parse_with_encoding("iso-8859-1#{suffix}.csv", 'iso-8859-1')
  end

  it 'should encode with external encoding' do
    parse_with_encoding("iso-8859-1#{suffix}.csv", 'iso-8859-1:-')
  end

  it 'should transcode' do
    parse_with_encoding("iso-8859-1#{suffix}.csv", 'iso-8859-1:utf-8')
  end

  it 'should recover from blank external encoding' do
    parse_with_encoding("utf-8#{suffix}.csv", ':utf-8')
  end

  it 'should recover from invalid internal encoding' do
    parse_with_encoding("utf-8#{suffix}.csv", 'invalid')
    parse_with_encoding("utf-8#{suffix}.csv", 'utf-8:invalid')
  end

  it 'should recover from invalid external encoding' do
    parse_with_encoding("utf-8#{suffix}.csv", 'invalid:-')
    parse_with_encoding("utf-8#{suffix}.csv", 'invalid:utf-8')
  end

  it 'should recover from invalid internal and external encodings' do
    parse_with_encoding("utf-8#{suffix}.csv", 'invalid:invalid')
  end
end

RSpec.describe FastCSV do
  context "with String" do
    def parse(csv, options = nil, parser = FastCSV)
      rows = []
      parser.raw_parse(csv, options){|row| rows << row}
      rows
    end

    def parse_without_block(csv, options = nil)
      FastCSV.raw_parse(csv, options)
    end

    include_examples 'a CSV parser'

    it 'should not raise an error on negative buffer size' do
      parser = FastCSV::Parser.new
      parser.buffer_size = -1
      expect{parse(simple, nil, parser)}.to_not raise_error
      parser.buffer_size = nil
    end
  end

  context "with StringIO" do
    def parse(csv, options = nil, parser = FastCSV)
      rows = []
      parser.raw_parse(StringIO.new(csv), options){|row| rows << row}
      rows
    end

    def parse_without_block(csv, options = nil)
      FastCSV.raw_parse(StringIO.new(csv), options)
    end

    include_examples 'a CSV parser'

    it 'should raise an error on negative buffer size' do
      parser = FastCSV::Parser.new
      parser.buffer_size = -1
      expect{parse(simple, nil, parser)}.to raise_error(NoMemoryError)
      parser.buffer_size = nil
    end
  end

  context 'with encoded unquoted fields' do
    def suffix
      ''
    end

    include_examples 'with encoded strings'
  end

  context 'with encoded quoted fields' do
    def suffix
      '-quoted'
    end

    include_examples 'with encoded strings'
  end

  context 'when initializing' do
    it 'should raise an error if the input is not a String or IO' do
      expect{FastCSV.raw_parse(nil)}.to raise_error(ArgumentError, 'data has to respond to #read or #to_str')
    end
  end

  describe '#row' do
    [
      "",
      "\n",
      "\n\n",
      "a,b,",
      "a,b,\n",
      "a,b,\nx,y,\n",
      "a,b,c",
      "a,b,c\n",
      "a,b,c\nx,y,z\n",
    ].each do |csv|
      it "should return the current row for: #{csv.inspect.gsub('\"', '"')}" do
        parser = FastCSV::Parser.new
        rows = []
        parser.raw_parse(csv) do |row|
          rows << parser.row
        end
        expect(rows).to eq(CSV.parse(csv).map{|row| CSV.generate_line(row).chomp("\n")})
      end
    end
  end
end
