#include <ruby.h>
#include <ruby/encoding.h>
// CSV specifications.
// http://tools.ietf.org/html/rfc4180
// http://w3c.github.io/csvw/syntax/#ebnf

// CSV implementation.
// https://github.com/ruby/ruby/blob/master/lib/csv.rb

// Ruby C extensions help.
// https://github.com/ruby/ruby/blob/trunk/README.EXT
// http://rxr.whitequark.org/mri/source

// Ragel help.
// https://www.mail-archive.com/ragel-users@complang.org/

# define ASSOCIATE_INDEX \
  if (internal_index) { \
    rb_enc_associate_index(field, internal_index); \
    field = rb_str_encode(field, rb_enc_from_encoding(external_encoding), 0, Qnil); \
  } \
  else { \
    rb_enc_associate_index(field, rb_enc_to_index(external_encoding)); \
  }

static VALUE mModule, rb_eParseError;
static ID s_read, s_to_str;

%%{
  machine fastcsv;

  action new_line {
    curline++;
  }

  action open_quote {
    unclosed_line = curline;
  }

  action close_quote {
    unclosed_line = 0;
  }

  action read_unquoted {
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_str_new(ts, p - ts);
      ASSOCIATE_INDEX;
    }
  }

  action read_quoted {
    if (p == ts) {
      field = rb_str_new2("");
      ASSOCIATE_INDEX;
    }
    // @note If we add an action on '""', we can skip some steps if no '""' is found.
    else if (p > ts) {
      // Operating on ts in-place produces odd behavior, FYI.
      char *copy = ALLOC_N(char, p - ts);
      memcpy(copy, ts, p - ts);

      char *reader = ts, *writer = copy;
      int escaped = 0;

      while (p > reader) {
        if (*reader == quote_char && !escaped) {
          // Skip the escaping character.
          escaped = 1;
        }
        else {
          escaped = 0;
          *writer++ = *reader;
        }
        reader++;
      }

      field = rb_str_new(copy, writer - copy);
      ASSOCIATE_INDEX;

      if (copy != NULL) {
        free(copy);
      }
    }
  }

  action new_field {
    rb_ary_push(row, field);
    field = Qnil;
  }

  action new_row {
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }

  action last_row {
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }

  EOF = 0 >last_row;
  quote_char = '"';
  col_sep = ',' >new_field;
  row_sep = ('\r' '\n'? | '\n') @new_line;
  unquoted = (any* -- quote_char -- col_sep -- row_sep - EOF) %read_unquoted;
  quoted = quote_char >open_quote (any - quote_char - EOF | quote_char quote_char | row_sep)* %read_quoted quote_char >close_quote;
  field = unquoted | quoted;
  # fields = (field col_sep)* field?;
  # file = (fields row_sep >new_row)* fields?;

  # @see Ragel Guide: 6.3 Scanners
  # Remember that an unquoted field can be zero-length.
  main := |*
    field col_sep EOF?;
    field row_sep >new_row EOF?;
    field EOF;
  *|;

  # Non-scanner version requires very large buffer.
  # main := file $/{
  #   if (!NIL_P(field) || RARRAY_LEN(row)) {
  #     rb_ary_push(row, field);
  #     rb_yield(row);
  #   }
  # };
}%%

%% write data;

#define BUFSIZE 16384

VALUE fastcsv(int argc, VALUE *argv, VALUE self) {
  int cs, act, have = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = 0, *eof = 0;

  VALUE port, opts;
  VALUE row = rb_ary_new(), field = Qnil, bufsize = Qnil;
  int done = 0, unclosed_line = 0, buffer_size = 0, taint = 0;
  int internal_index = 0, external_index = rb_enc_to_index(rb_default_external_encoding());
  rb_encoding *external_encoding = rb_default_external_encoding();

  VALUE option;
  char quote_char = '"'; //, *col_sep = ",", *row_sep = "\r\n";

  rb_scan_args(argc, argv, "11", &port, &opts);
  taint = OBJ_TAINTED(port);
  io = rb_respond_to(port, s_read);
  if (!io) {
    if (rb_respond_to(port, s_to_str)) {
      port = rb_funcall(port, s_to_str, 0);
      StringValue(port);
    }
    else {
      rb_raise(rb_eArgError, "data has to respond to #read or #to_str");
    }
  }

  if (NIL_P(opts)) {
    opts = rb_hash_new();
  }
  else if (TYPE(opts) != T_HASH) {
    rb_raise(rb_eArgError, "options has to be a Hash or nil");
  }

  // @note Add machines for common CSV dialects, or see if we can use "when"
  // from Chapter 6 to compare the character to the host program's variable.
  // option = rb_hash_aref(opts, ID2SYM(rb_intern("quote_char")));
  // if (TYPE(option) == T_STRING && RSTRING_LEN(option) == 1) {
  //   quote_char = *StringValueCStr(option);
  // }
  // else if (!NIL_P(option)) {
  //   rb_raise(rb_eArgError, ":quote_char has to be a single character String");
  // }

  // option = rb_hash_aref(opts, ID2SYM(rb_intern("col_sep")));
  // if (TYPE(option) == T_STRING) {
  //   col_sep = StringValueCStr(option);
  // }
  // else if (!NIL_P(option)) {
  //   rb_raise(rb_eArgError, ":col_sep has to be a String");
  // }

  // option = rb_hash_aref(opts, ID2SYM(rb_intern("row_sep")));
  // if (TYPE(option) == T_STRING) {
  //   row_sep = StringValueCStr(option);
  // }
  // else if (!NIL_P(option)) {
  //   rb_raise(rb_eArgError, ":row_sep has to be a String");
  // }

  option = rb_hash_aref(opts, ID2SYM(rb_intern("encoding")));
  if (TYPE(option) == T_STRING) {
    // @see parse_mode_enc in Ruby's io.c
    const char *string = StringValueCStr(option), *pointer;
    char internal_encoding_name[ENCODING_MAXNAMELEN + 1];

    pointer = strrchr(string, ':');
    if (pointer) {
      long len = (pointer++) - string;
      if (len == 0 || len > ENCODING_MAXNAMELEN) {
        internal_index = -1;
      }
      else {
        memcpy(internal_encoding_name, string, len);
        internal_encoding_name[len] = '\0';
        string = internal_encoding_name;
        internal_index = rb_enc_find_index(internal_encoding_name);
      }
    }
    else {
      internal_index = rb_enc_find_index(string);
    }

    if (internal_index < 0 && internal_index != -2) {
      unsupported_encoding(string);
    }

    if (pointer) {
      external_index = rb_enc_find_index(pointer);
      if (external_index >= 0) {
        external_encoding = rb_enc_from_index(external_index);
      }
      else {
        unsupported_encoding(pointer);
      }
    }
  }
  else if (!NIL_P(option)) {
    rb_raise(rb_eArgError, ":encoding has to be a String");
  }

  buffer_size = BUFSIZE;
  if (rb_ivar_defined(self, rb_intern("@buffer_size")) == Qtrue) {
    bufsize = rb_ivar_get(self, rb_intern("@buffer_size"));
    if (!NIL_P(bufsize)) {
      buffer_size = NUM2INT(bufsize);
    }
  }

  if (io) {
    buf = ALLOC_N(char, buffer_size);
  }

  %% write init;

  while (!done) {
    VALUE str;
    char *p, *pe;
    int len, space = buffer_size - have, tokstart_diff, tokend_diff;

    if (io) {
      if (space == 0) {
         tokstart_diff = ts - buf;
         tokend_diff = te - buf;

         buffer_size += BUFSIZE;
         REALLOC_N(buf, char, buffer_size);

         space = buffer_size - have;

         ts = buf + tokstart_diff;
         te = buf + tokend_diff;
      }
      p = buf + have;

      str = rb_funcall(port, s_read, 1, INT2FIX(space));
      if (NIL_P(str)) {
        // StringIO#read returns nil for empty string.
        len = 0;
      }
      else {
        len = RSTRING_LEN(str);
        memcpy(p, StringValuePtr(str), len);
      }

      if (len < space) {
        // EOF actions don't work in scanners, so we add a sentinel value.
        // @see http://www.complang.org/pipermail/ragel-users/2007-May/001516.html
        // @see https://github.com/leeonix/lua-csv-ragel/blob/master/src/csv.rl
        p[len++] = 0;
        done = 1;
      }
    }
    else {
      p = RSTRING_PTR(port);
      len = RSTRING_LEN(port);
      p[len++] = 0;
      done = 1;
    }

    pe = p + len;
    // if (done) {
    //   // This triggers the eof action in the non-scanner version.
    //   eof = pe;
    // }
    %% write exec;

    if (done && cs < fastcsv_first_final) {
      if (buf != NULL) {
        free(buf);
      }
      if (unclosed_line) {
        rb_raise(rb_eParseError, "Unclosed quoted field on line %d.", unclosed_line);
      }
      // Ruby raises different errors for illegal quoting, depending on whether
      // a quoted string is followed by a string ("Unclosed quoted field on line
      // %d.") or by a string ending in a quote ("Missing or stray quote in line
      // %d"). These precisions are kind of bogus, but we can try using $!.
      else {
        rb_raise(rb_eParseError, "Illegal quoting in line %d.", curline);
      }
    }

    if (ts == 0) {
      have = 0;
    }
    else if (io) {
      have = pe - ts;
      memmove(buf, ts, have);
      te = buf + (te - ts);
      ts = buf;
    }
  }

  if (buf != NULL) {
    free(buf);
  }

  return Qnil;
}

void Init_fastcsv() {
  s_read = rb_intern("read");
  s_to_str = rb_intern("to_str");

  mModule = rb_define_module("FastCSV");
  rb_define_attr(rb_singleton_class(mModule), "buffer_size", 1, 1);
  rb_define_singleton_method(mModule, "raw_parse", fastcsv, -1);
  rb_eParseError = rb_define_class_under(mModule, "ParseError", rb_eStandardError);
}
