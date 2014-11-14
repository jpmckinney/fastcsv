require 'csv'

require 'fastcsv/fastcsv'

# @see https://github.com/ruby/ruby/blob/ab337e61ecb5f42384ba7d710c36faf96a454e5c/lib/csv.rb
class FastCSV < CSV
  def self.raw_parse(*args, &block)
    Parser.new.raw_parse(*args, &block)
  end

  def shift
    # COPY
    # handle headers not based on document content
    if header_row? and @return_headers and
       [Array, String].include? @use_headers.class
      if @unconverted_fields
        return add_unconverted_fields(parse_headers, Array.new)
      else
        return parse_headers
      end
    end
    # PASTE

    # The CSV library wraps File objects, whereas `FastCSV.raw_parse` accepts
    # IO-like objects that implement `#read(length)`.
    begin
      unless csv = fiber.resume # was unless parse = @io.gets(@row_sep)
        return nil
      end
    rescue FiberError
      return nil
    end

    # COPY
    if csv.empty?
      #
      # I believe a blank line should be an <tt>Array.new</tt>, not Ruby 1.8
      # CSV's <tt>[nil]</tt>
      #
      if parse.empty?
        @lineno += 1
        if @skip_blanks
          shift # was next
        elsif @unconverted_fields
          return add_unconverted_fields(Array.new, Array.new)
        elsif @use_headers
          return self.class::Row.new(Array.new, Array.new)
        else
          return Array.new
        end
      end
    end
    # PASTE

    shift if @skip_lines and @skip_lines.match row # was next if @skip_lines and @skip_lines.match parse

    # COPY
    @lineno += 1

    # save fields unconverted fields, if needed...
    unconverted = csv.dup if @unconverted_fields

    # convert fields, if needed...
    csv = convert_fields(csv) unless @use_headers or @converters.empty?
    # parse out header rows and handle CSV::Row conversions...
    csv = parse_headers(csv)  if     @use_headers

    # inject unconverted fields and accessor, if requested...
    if @unconverted_fields and not csv.respond_to? :unconverted_fields
      add_unconverted_fields(csv, unconverted)
    end
    # PASTE

    csv # was break csv
  end

  # CSV's delegated and overwritten IO methods move the pointer within the file,
  # but FastCSV doesn't notice, so we need to recreate the fiber. The old fiber
  # is garbage collected.

  def pos=(*args)
    super
    @fiber = nil
  end
  def reopen(*args)
    super
    @fiber = nil
  end
  def seek(*args)
    super
    @fiber = nil
  end
  def truncate(*args)
    super
    @fiber = nil
  end
  def rewind
    super
    @fiber = nil
  end

private

  def parser
    @parser ||= Parser.new
  end

  def fiber
    # @see http://www.ruby-doc.org/core-2.1.4/Fiber.html
    @fiber ||= Fiber.new do
      parser.raw_parse(@io) do |row|
        Fiber.yield(row)
      end
    end
  end

  def init_separators(options)
    super
    @col_sep = ','
    # FastCSV actually splits on "\r", "\n" or "\r\n", but we have no way to
    # report that as a simple string.
    @row_sep = "\r\n"
    @quote_char = '"'
  end
end
