
#line 1 "ext/fastcsv/fastcsv.rl"
#include <ruby.h>
#include <ruby/encoding.h>
// CSV specifications.
// http://tools.ietf.org/html/rfc4180
// http://w3c.github.io/csvw/syntax/#ebnf

// CSV implementation.
// https://github.com/ruby/ruby/blob/master/lib/csv.rb

// Ruby C extensions help.
// https://github.com/ruby/ruby/blob/trunk/README.EXT
// https://github.com/ruby/ruby/blob/trunk/encoding.c

// Ragel help.
// https://www.mail-archive.com/ragel-users@complang.org/

# define ASSOCIATE_INDEX \
  if (internal_encoding) { \
    rb_enc_associate_index(field, rb_enc_to_index(internal_encoding)); \
    field = rb_str_encode(field, rb_enc_from_encoding(external_encoding), 0, Qnil); \
  } \
  else { \
    rb_enc_associate_index(field, rb_enc_to_index(external_encoding)); \
  }

static VALUE mModule, rb_eParseError;
static ID s_read, s_to_str;


#line 127 "ext/fastcsv/fastcsv.rl"



#line 37 "ext/fastcsv/fastcsv.c"
static const char _fastcsv_actions[] = {
	0, 1, 0, 1, 1, 1, 7, 1, 
	8, 1, 11, 2, 0, 10, 2, 4, 
	2, 2, 5, 9, 2, 6, 0, 3, 
	3, 5, 9, 3, 3, 6, 0, 3, 
	6, 0, 10, 4, 3, 6, 0, 10
	
};

static const char _fastcsv_key_offsets[] = {
	0, 0, 4, 7, 11, 15
};

static const char _fastcsv_trans_keys[] = {
	10, 13, 34, 44, 10, 13, 34, 10, 
	13, 34, 44, 10, 13, 34, 44, 10, 
	0
};

static const char _fastcsv_single_lengths[] = {
	0, 4, 3, 4, 4, 1
};

static const char _fastcsv_range_lengths[] = {
	0, 0, 0, 0, 0, 0
};

static const char _fastcsv_index_offsets[] = {
	0, 0, 5, 9, 14, 19
};

static const char _fastcsv_trans_targs[] = {
	4, 5, 0, 4, 1, 2, 2, 3, 
	2, 4, 5, 2, 4, 0, 4, 5, 
	2, 4, 1, 4, 4, 4, 0
};

static const char _fastcsv_trans_actions[] = {
	35, 27, 0, 23, 0, 1, 1, 14, 
	0, 31, 20, 0, 17, 0, 35, 27, 
	3, 23, 0, 11, 9, 9, 0
};

static const char _fastcsv_to_state_actions[] = {
	0, 0, 0, 0, 5, 0
};

static const char _fastcsv_from_state_actions[] = {
	0, 0, 0, 0, 7, 0
};

static const char _fastcsv_eof_trans[] = {
	0, 0, 0, 0, 0, 22
};

static const int fastcsv_start = 4;
static const int fastcsv_first_final = 4;
static const int fastcsv_error = 0;

static const int fastcsv_en_main = 4;


#line 130 "ext/fastcsv/fastcsv.rl"

#define BUFSIZE 16384

VALUE fastcsv(int argc, VALUE *argv, VALUE self) {
  int cs, act, have = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = 0, *eof = 0;

  VALUE port, opts;
  VALUE row = rb_ary_new(), field = Qnil, bufsize = Qnil;
  int done = 0, unclosed_line = 0, buffer_size = 0, taint = 0;
  int internal_index = 0, external_index = rb_enc_to_index(rb_default_external_encoding());
  rb_encoding *internal_encoding = NULL, *external_encoding = rb_default_external_encoding();

  VALUE option;
  char quote_char = '"', *col_sep = ",", *row_sep = "\r\n";

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

    if (internal_index >= 0) {
      internal_encoding = rb_enc_from_index(internal_index);
    }
    else if (internal_index != -2) {
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

  
#line 220 "ext/fastcsv/fastcsv.c"
	{
	cs = fastcsv_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 249 "ext/fastcsv/fastcsv.rl"

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
        done = 1;
      }
    }
    else {
      p = RSTRING_PTR(port);
      len = RSTRING_LEN(port);
      done = 1;
    }

    pe = p + len;
    if (done) {
      eof = pe;
    }
    
#line 275 "ext/fastcsv/fastcsv.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_acts = _fastcsv_actions + _fastcsv_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 8:
#line 1 "NONE"
	{ts = p;}
	break;
#line 296 "ext/fastcsv/fastcsv.c"
		}
	}

	_keys = _fastcsv_trans_keys + _fastcsv_key_offsets[cs];
	_trans = _fastcsv_index_offsets[cs];

	_klen = _fastcsv_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _fastcsv_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
_eof_trans:
	cs = _fastcsv_trans_targs[_trans];

	if ( _fastcsv_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _fastcsv_actions + _fastcsv_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 32 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	break;
	case 1:
#line 36 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
	break;
	case 2:
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
	break;
	case 3:
#line 44 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_str_new(ts, p - ts);
      ASSOCIATE_INDEX;
    }
  }
	break;
	case 4:
#line 55 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      field = rb_str_new2("");
      ASSOCIATE_INDEX;
    }
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
	break;
	case 5:
#line 89 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
	break;
	case 6:
#line 94 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	break;
	case 9:
#line 116 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	break;
	case 10:
#line 117 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	break;
	case 11:
#line 117 "ext/fastcsv/fastcsv.rl"
	{te = p;p--;}
	break;
#line 459 "ext/fastcsv/fastcsv.c"
		}
	}

_again:
	_acts = _fastcsv_actions + _fastcsv_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 7:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 472 "ext/fastcsv/fastcsv.c"
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _fastcsv_eof_trans[cs] > 0 ) {
		_trans = _fastcsv_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

#line 295 "ext/fastcsv/fastcsv.rl"

    // @todo Use \0 as a sentinel value as in Lua CSV Ragel?
    // EOF actions don't work in Scanners. We'd need to add a sentinel value.
    // @see http://www.complang.org/pipermail/ragel-users/2007-May/001516.html
    // if (done && (!NIL_P(field) || RARRAY_LEN(row))) {
    //   rb_ary_push(row, field);
    //   rb_yield(row);
    // }

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
      // %d"). These precisions are kind of bogus.
      // @todo Try using $!.
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
