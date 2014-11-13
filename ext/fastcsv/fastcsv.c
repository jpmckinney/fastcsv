
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
// http://rxr.whitequark.org/mri/source

// Ragel help.
// https://www.mail-archive.com/ragel-users@complang.org/

#define ENCODE \
if (enc2 != NULL) { \
  field = rb_str_encode(field, rb_enc_from_encoding(enc), 0, Qnil); \
}

static VALUE mModule, rb_eParseError;
static ID s_read, s_to_str, s_internal_encoding, s_external_encoding, s_string, s_encoding;


#line 125 "ext/fastcsv/fastcsv.rl"



#line 33 "ext/fastcsv/fastcsv.c"
static const int fastcsv_start = 4;
static const int fastcsv_first_final = 4;
static const int fastcsv_error = 0;

static const int fastcsv_en_main = 4;


#line 128 "ext/fastcsv/fastcsv.rl"

// 16 kB
#define BUFSIZE 16384

// @see http://rxr.whitequark.org/mri/source/io.c#4845
static void
rb_io_ext_int_to_encs(rb_encoding *ext, rb_encoding *intern, rb_encoding **enc, rb_encoding **enc2, int fmode)
{
  int default_ext = 0;

  if (ext == NULL) {
    ext = rb_default_external_encoding();
    default_ext = 1;
  }
  if (ext == rb_ascii8bit_encoding()) {
    /* If external is ASCII-8BIT, no transcoding */
    intern = NULL;
  }
  else if (intern == NULL) {
    intern = rb_default_internal_encoding();
  }
  if (intern == NULL || intern == (rb_encoding *)Qnil || intern == ext) {
    /* No internal encoding => use external + no transcoding */
    *enc = (default_ext && intern != ext) ? NULL : ext;
    *enc2 = NULL;
  }
  else {
    *enc = intern;
    *enc2 = ext;
  }
}

VALUE fastcsv(int argc, VALUE *argv, VALUE self) {
  int cs, act, have = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = 0, *eof = 0;

  VALUE port, opts;
  VALUE row = rb_ary_new(), field = Qnil, bufsize = Qnil;
  int done = 0, unclosed_line = 0, buffer_size = 0, taint = 0;
  rb_encoding *enc = NULL, *enc2 = NULL, *encoding = NULL;
  VALUE r_encoding;

  VALUE option;
  char quote_char = '"';

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

  // @see rb_io_extract_modeenc
  /* Set to defaults */
  rb_io_ext_int_to_encs(NULL, NULL, &enc, &enc2, 0);

  // "enc" (internal) or "enc2:enc" (external:internal) or "enc:-" (external).
  // We don't support binmode, which would force "ASCII-8BIT", or "BOM|UTF-*".
  // @see http://ruby-doc.org/core-2.1.1/IO.html#method-c-new-label-Open+Mode
  option = rb_hash_aref(opts, ID2SYM(rb_intern("encoding")));
  if (TYPE(option) == T_STRING) {
    // `parse_mode_enc` is not in header file.
    const char *estr = StringValueCStr(option), *ptr;
    char encname[ENCODING_MAXNAMELEN+1];
    int idx, idx2;
    rb_encoding *ext_enc, *int_enc;

    /* parse estr as "enc" or "enc2:enc" or "enc:-" */

    ptr = strrchr(estr, ':');
    if (ptr) {
      long len = (ptr++) - estr;
      if (len == 0 || len > ENCODING_MAXNAMELEN) { // ":enc"
        idx = -1;
      }
      else { // "enc2:enc" or "enc:-"
        memcpy(encname, estr, len);
        encname[len] = '\0';
        estr = encname;
        idx = rb_enc_find_index(encname);
      }
    }
    else { // "enc"
      idx = rb_enc_find_index(estr);
    }

    if (idx >= 0) {
      ext_enc = rb_enc_from_index(idx);
    }
    else {
      if (idx != -2) { // ":enc"
        // `unsupported_encoding` is not in header file.
        rb_warn("Unsupported encoding %s ignored", estr);
      }
      ext_enc = NULL;
    }

    int_enc = NULL;
    if (ptr) {
      if (*ptr == '-' && *(ptr+1) == '\0') { // "enc:-"
        /* Special case - "-" => no transcoding */
        int_enc = (rb_encoding *)Qnil;
      }
      else { // "enc2:enc"
        idx2 = rb_enc_find_index(ptr);
        if (idx2 < 0) {
          // `unsupported_encoding` is not in header file.
          rb_warn("Unsupported encoding %s ignored", ptr);
        }
        else if (idx2 == idx) {
          int_enc = (rb_encoding *)Qnil;
        }
        else {
          int_enc = rb_enc_from_index(idx2);
        }
      }
    }

    rb_io_ext_int_to_encs(ext_enc, int_enc, &enc, &enc2, 0);
  }
  else if (!NIL_P(option)) {
    rb_raise(rb_eArgError, ":encoding has to be a String");
  }

  // @see CSV#raw_encoding
  // @see https://github.com/ruby/ruby/blob/70510d026f8d86693dccaba07417488eed09b41d/lib/csv.rb#L1567
  if (rb_respond_to(port, s_internal_encoding)) {
    r_encoding = rb_funcall(port, s_internal_encoding, 0);
    if (NIL_P(r_encoding)) {
      r_encoding = rb_funcall(port, s_external_encoding, 0);
    }
  }
  else if (rb_respond_to(port, s_string)) {
    r_encoding = rb_funcall(rb_funcall(port, s_string, 0), s_encoding, 0);
  }
  else if (rb_respond_to(port, s_encoding)) {
    r_encoding = rb_funcall(port, s_encoding, 0);
  }
  else {
    r_encoding = rb_enc_from_encoding(rb_ascii8bit_encoding());
  }

  // @see CSV#initialize
  // @see https://github.com/ruby/ruby/blob/70510d026f8d86693dccaba07417488eed09b41d/lib/csv.rb#L2300
  if (NIL_P(r_encoding)) {
    r_encoding = rb_enc_from_encoding(rb_default_internal_encoding());
  }
  if (NIL_P(r_encoding)) {
    r_encoding = rb_enc_from_encoding(rb_default_external_encoding());
  }

  if (enc2 != NULL) {
    encoding = enc2;
  }
  else if (enc != NULL) {
    encoding = enc;
  }
  else if (!NIL_P(r_encoding)) {
    encoding = rb_enc_get(r_encoding);
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

  
#line 229 "ext/fastcsv/fastcsv.c"
	{
	cs = fastcsv_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 315 "ext/fastcsv/fastcsv.rl"

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
    
#line 286 "ext/fastcsv/fastcsv.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr0:
#line 1 "NONE"
	{	switch( act ) {
	case 0:
	{{goto st0;}}
	break;
	default:
	{{p = ((te))-1;}}
	break;
	}
	}
	goto st4;
tr10:
#line 101 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 123 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st4;
tr16:
#line 123 "ext/fastcsv/fastcsv.rl"
	{te = p;p--;}
	goto st4;
tr17:
#line 122 "ext/fastcsv/fastcsv.rl"
	{te = p;p--;}
	goto st4;
tr18:
#line 101 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 122 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st4;
tr20:
#line 121 "ext/fastcsv/fastcsv.rl"
	{te = p;p--;}
	goto st4;
tr21:
#line 101 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 121 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st4;
st4:
#line 1 "NONE"
	{ts = 0;}
#line 1 "NONE"
	{act = 0;}
	if ( ++p == pe )
		goto _test_eof4;
case 4:
#line 1 "NONE"
	{ts = p;}
#line 365 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 0: goto tr14;
		case 10: goto tr3;
		case 13: goto tr4;
		case 34: goto tr15;
		case 44: goto tr5;
	}
	goto st1;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	switch( (*p) ) {
		case 0: goto tr2;
		case 10: goto tr3;
		case 13: goto tr4;
		case 34: goto tr0;
		case 44: goto tr5;
	}
	goto st1;
tr2:
#line 1 "NONE"
	{te = p+1;}
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_enc_str_new(ts, p - ts, encoding);
      ENCODE;
    }
  }
#line 101 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 123 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st5;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
#line 416 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 0: goto tr2;
		case 10: goto tr3;
		case 13: goto tr4;
		case 34: goto tr16;
		case 44: goto tr5;
	}
	goto st1;
tr3:
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_enc_str_new(ts, p - ts, encoding);
      ENCODE;
    }
  }
#line 91 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st6;
tr19:
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st6;
tr11:
#line 91 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st6;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
#line 478 "ext/fastcsv/fastcsv.c"
	if ( (*p) == 0 )
		goto tr18;
	goto tr17;
tr4:
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_enc_str_new(ts, p - ts, encoding);
      ENCODE;
    }
  }
#line 91 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st7;
tr12:
#line 91 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st7;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
#line 529 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 0: goto tr18;
		case 10: goto tr19;
	}
	goto tr17;
tr5:
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_enc_str_new(ts, p - ts, encoding);
      ENCODE;
    }
  }
#line 86 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
	goto st8;
tr13:
#line 86 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
	goto st8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
#line 564 "ext/fastcsv/fastcsv.c"
	if ( (*p) == 0 )
		goto tr21;
	goto tr20;
tr14:
#line 1 "NONE"
	{te = p+1;}
#line 101 "ext/fastcsv/fastcsv.rl"
	{
    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }
    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 40 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      // Unquoted empty fields are nil, not "", in Ruby.
      field = Qnil;
    }
    else if (p > ts) {
      field = rb_enc_str_new(ts, p - ts, encoding);
      ENCODE;
    }
  }
#line 123 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 598 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 10: goto tr16;
		case 13: goto tr16;
		case 34: goto tr16;
		case 44: goto tr16;
	}
	goto st1;
tr8:
#line 28 "ext/fastcsv/fastcsv.rl"
	{
    curline++;
  }
	goto st2;
tr15:
#line 32 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
#line 622 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 0: goto st0;
		case 10: goto tr8;
		case 13: goto tr8;
		case 34: goto tr9;
	}
	goto st2;
st0:
cs = 0;
	goto _out;
tr9:
#line 51 "ext/fastcsv/fastcsv.rl"
	{
    if (p == ts) {
      field = rb_enc_str_new("", 0, encoding);
      ENCODE;
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

      field = rb_enc_str_new(copy, writer - copy, encoding);
      ENCODE;

      if (copy != NULL) {
        free(copy);
      }
    }
  }
#line 36 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 678 "ext/fastcsv/fastcsv.c"
	switch( (*p) ) {
		case 0: goto tr10;
		case 10: goto tr11;
		case 13: goto tr12;
		case 34: goto st2;
		case 44: goto tr13;
	}
	goto st0;
	}
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 1: goto tr0;
	case 5: goto tr16;
	case 6: goto tr17;
	case 7: goto tr17;
	case 8: goto tr20;
	case 9: goto tr16;
	}
	}

	_out: {}
	}

#line 363 "ext/fastcsv/fastcsv.rl"

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
  s_internal_encoding = rb_intern("internal_encoding");
  s_external_encoding = rb_intern("external_encoding");
  s_string = rb_intern("string");
  s_encoding = rb_intern("encoding");

  mModule = rb_define_module("FastCSV");
  rb_define_attr(rb_singleton_class(mModule), "buffer_size", 1, 1);
  rb_define_singleton_method(mModule, "raw_parse", fastcsv, -1);
  rb_eParseError = rb_define_class_under(mModule, "ParseError", rb_eStandardError);
}
