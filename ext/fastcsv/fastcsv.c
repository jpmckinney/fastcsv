
#line 1 "ext/fastcsv/fastcsv.rl"
#include <ruby.h>
#include <ruby/encoding.h>
// https://github.com/ruby/ruby/blob/trunk/README.EXT
// https://github.com/ruby/ruby/blob/trunk/encoding.c
// https://github.com/ruby/ruby/blob/master/lib/csv.rb
// http://tools.ietf.org/html/rfc4180
// http://w3c.github.io/csvw/syntax/#ebnf

static VALUE mModule, rb_eParseError;
static ID s_read, s_to_str;


#line 108 "ext/fastcsv/fastcsv.rl"



#line 20 "ext/fastcsv/fastcsv.c"
static const char _fastcsv_actions[] = {
	0, 1, 0, 1, 1, 1, 3, 1, 
	6, 1, 8, 2, 3, 0, 2, 4, 
	6, 2, 4, 8, 2, 5, 2, 2, 
	7, 0, 3, 3, 4, 6, 3, 3, 
	4, 8, 3, 3, 5, 2, 3, 4, 
	7, 0, 4, 0, 3, 4, 7, 4, 
	3, 4, 7, 0
};

static const char _fastcsv_key_offsets[] = {
	0, 0, 3, 6, 10, 14, 18
};

static const char _fastcsv_trans_keys[] = {
	10, 13, 34, 10, 13, 34, 10, 13, 
	34, 44, 10, 13, 34, 44, 10, 13, 
	34, 44, 10, 13, 34, 44, 0
};

static const char _fastcsv_single_lengths[] = {
	0, 3, 3, 4, 4, 4, 4
};

static const char _fastcsv_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0
};

static const char _fastcsv_index_offsets[] = {
	0, 0, 4, 8, 13, 18, 23
};

static const char _fastcsv_trans_targs[] = {
	2, 2, 6, 2, 2, 2, 6, 2, 
	3, 5, 1, 3, 4, 3, 5, 0, 
	3, 4, 3, 5, 1, 3, 4, 3, 
	5, 2, 3, 0, 0
};

static const char _fastcsv_trans_actions[] = {
	11, 11, 34, 5, 1, 1, 20, 0, 
	47, 47, 3, 26, 5, 38, 38, 0, 
	14, 0, 42, 47, 3, 26, 5, 23, 
	23, 0, 7, 0, 0
};

static const char _fastcsv_eof_actions[] = {
	0, 9, 9, 30, 17, 30, 9
};

static const int fastcsv_start = 3;
static const int fastcsv_first_final = 3;
static const int fastcsv_error = 0;

static const int fastcsv_en_main = 3;


#line 111 "ext/fastcsv/fastcsv.rl"

#define BUFSIZE 16384

VALUE fastcsv(int argc, VALUE *argv, VALUE self) {
  int cs, act, have = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = 0, *eof = 0;

  VALUE port, opts;
  VALUE row = rb_ary_new(), aval = Qnil, bufsize = Qnil;
  char *mark_aval = 0;
  int done = 0, unclosed_line = 0, buffer_size = 0, taint = 0;
  int encoding_index = rb_enc_to_index(rb_default_external_encoding());

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

  // @todo Add machines for other common CSV formats?
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
    encoding_index = rb_enc_find_index(StringValueCStr(option));
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

  
#line 161 "ext/fastcsv/fastcsv.c"
	{
	cs = fastcsv_start;
	}

#line 193 "ext/fastcsv/fastcsv.rl"

  while (!done) {
    VALUE str;
    char *p, *pe;
    int len, space = buffer_size - have, tokstart_diff, tokend_diff, mark_aval_diff;

    if (io) {
      if (space == 0) {
         tokstart_diff = ts - buf;
         tokend_diff = te - buf;
         mark_aval_diff = mark_aval - buf;

         buffer_size += BUFSIZE;
         REALLOC_N(buf, char, buffer_size);

         space = buffer_size - have;

         ts = buf + tokstart_diff;
         te = buf + tokend_diff;
         mark_aval = buf + mark_aval_diff;
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
      len = RSTRING_LEN(port) + 1;
      done = 1;
    }

    pe = p + len;
    if (done) {
      eof = pe;
    }
    
#line 215 "ext/fastcsv/fastcsv.c"
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
#line 15 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	break;
	case 1:
#line 19 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
	break;
	case 2:
#line 23 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
	break;
	case 3:
#line 27 "ext/fastcsv/fastcsv.rl"
	{
    mark_aval = p;
  }
	break;
	case 4:
#line 31 "ext/fastcsv/fastcsv.rl"
	{
    if (p == mark_aval) {
      // Unquoted empty fields are nil, not "", in Ruby.
      aval = Qnil;
    }
    else if (p > mark_aval) {
      aval = rb_str_new(mark_aval, p - mark_aval);
      rb_enc_associate_index(aval, encoding_index);
    }
  }
	break;
	case 5:
#line 42 "ext/fastcsv/fastcsv.rl"
	{
    if (p == mark_aval) {
      aval = rb_str_new2("");
      rb_enc_associate_index(aval, encoding_index);
    }
    else if (p > mark_aval) {
      // Operating on mark_aval in-place produces odd behavior.
      char *mark_copy = ALLOC_N(char, p - mark_aval);
      memcpy(mark_copy, mark_aval, p - mark_aval);

      char *reader = mark_aval, *writer = mark_copy;
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

      aval = rb_str_new(mark_copy, writer - mark_copy);
      rb_enc_associate_index(aval, encoding_index);

      if (mark_copy != NULL) {
        free(mark_copy);
      }
    }
  }
	break;
	case 6:
#line 76 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, aval);
    aval = Qnil;
  }
	break;
	case 7:
#line 81 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(aval) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, aval);
      aval = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	break;
#line 380 "ext/fastcsv/fastcsv.c"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _fastcsv_actions + _fastcsv_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 3:
#line 27 "ext/fastcsv/fastcsv.rl"
	{
    mark_aval = p;
  }
	break;
	case 4:
#line 31 "ext/fastcsv/fastcsv.rl"
	{
    if (p == mark_aval) {
      // Unquoted empty fields are nil, not "", in Ruby.
      aval = Qnil;
    }
    else if (p > mark_aval) {
      aval = rb_str_new(mark_aval, p - mark_aval);
      rb_enc_associate_index(aval, encoding_index);
    }
  }
	break;
	case 8:
#line 102 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(aval) || RARRAY_LEN(row)) {
      rb_ary_push(row, aval);
      rb_yield(row);
    }
  }
	break;
#line 424 "ext/fastcsv/fastcsv.c"
		}
	}
	}

	_out: {}
	}

#line 241 "ext/fastcsv/fastcsv.rl"

    if (cs < fastcsv_first_final) {
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
      else {
        rb_raise(rb_eParseError, "Illegal quoting in line %d.", curline);
      }
    }

    // @todo
    // EOF actions don't work in scanners.
    // @see http://www.complang.org/pipermail/ragel-users/2007-May/001516.html
    // if (done && (!NIL_P(aval) || RARRAY_LEN(row))) {
    //   rb_ary_push(row, aval);
    //   rb_yield(row);
    // }

    if (ts == 0) {
      have = 0;
    }
    else if (io) {
      have = pe - ts;
      memmove(buf, ts, have);
      if (mark_aval > ts) {
        mark_aval = buf + (mark_aval - ts);
      }
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
  rb_define_singleton_method(mModule, "scan", fastcsv, -1);
  rb_eParseError = rb_define_class_under(mModule, "ParseError", rb_eStandardError);
}
