
#line 1 "ext/fastcsv/fastcsv.rl"
#include <ruby.h>
#include <ruby/encoding.h>
// CSV specifications.
// http://tools.ietf.org/html/rfc4180
// http://w3c.github.io/csvw/syntax/#ebnf

// CSV implementation.
// https://github.com/ruby/ruby/blob/trunk/lib/csv.rb

// Ruby C extensions help.
// https://github.com/ruby/ruby/blob/trunk/README.EXT
// http://rxr.whitequark.org/mri/source

// Ragel help.
// https://www.mail-archive.com/ragel-users@complang.org/

#define ENCODE \
if (enc2 != NULL) { \
  field = rb_str_encode(field, rb_enc_from_encoding(enc), 0, Qnil); \
}

#define FREE \
if (buf != NULL) { \
  free(buf); \
} \
if (row_sep != NULL) { \
  free(row_sep); \
}

static VALUE cClass, cParser, eError;
static ID s_read, s_row;

// @see https://github.com/nofxx/georuby_c/blob/b3b91fd90980d7c295ac8f6012d89878ea7cd569/ext/types.h#L22
typedef struct {
  char *start;
} Data;


#line 172 "ext/fastcsv/fastcsv.rl"



#line 46 "ext/fastcsv/fastcsv.c"
static const int raw_parse_start = 5;
static const int raw_parse_first_final = 5;
static const int raw_parse_error = 0;

static const int raw_parse_en_main = 5;


#line 175 "ext/fastcsv/fastcsv.rl"

// 16 kB
#define BUFSIZE 16384

// @see http://rxr.whitequark.org/mri/source/io.c#4845
static void rb_io_ext_int_to_encs(rb_encoding *ext, rb_encoding *intern, rb_encoding **enc, rb_encoding **enc2, int fmode) {
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

static VALUE raw_parse(int argc, VALUE *argv, VALUE self) {
  int cs, act, have = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = 0, *eof = 0, *mark_row_sep = 0, *row_sep = 0;

  VALUE port, opts, r_encoding;
  VALUE row = rb_ary_new(), field = Qnil, bufsize = Qnil;
  int done = 0, unclosed_line = 0, buffer_size = 0, taint = 0, len_row_sep = 0;
  rb_encoding *enc = NULL, *enc2 = NULL, *encoding = NULL;

  Data *d;
  Data_Get_Struct(self, Data, d);

  VALUE option;
  char quote_char = '"', col_sep = ',';

  rb_scan_args(argc, argv, "11", &port, &opts);
  taint = OBJ_TAINTED(port);
  io = rb_respond_to(port, s_read);
  if (!io) {
    if (rb_respond_to(port, rb_intern("to_str"))) {
      port = rb_funcall(port, rb_intern("to_str"), 0);
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

  option = rb_hash_aref(opts, ID2SYM(rb_intern("quote_char")));
  if (TYPE(option) == T_STRING && RSTRING_LEN(option) == 1) {
    quote_char = *StringValueCStr(option);
  }
  else if (!NIL_P(option)) {
    rb_raise(rb_eArgError, ":quote_char has to be a single character String");
  }

  option = rb_hash_aref(opts, ID2SYM(rb_intern("col_sep")));
  if (TYPE(option) == T_STRING && RSTRING_LEN(option) == 1) {
    col_sep = *StringValueCStr(option);
  }
  else if (!NIL_P(option)) {
    rb_raise(rb_eArgError, ":col_sep has to be a single character String");
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
  // @see https://github.com/ruby/ruby/blob/ab337e61ecb5f42384ba7d710c36faf96a454e5c/lib/csv.rb#L2290
  if (rb_respond_to(port, rb_intern("internal_encoding"))) {
    r_encoding = rb_funcall(port, rb_intern("internal_encoding"), 0);
    if (NIL_P(r_encoding)) {
      r_encoding = rb_funcall(port, rb_intern("external_encoding"), 0);
    }
  }
  else if (rb_respond_to(port, rb_intern("string"))) {
    r_encoding = rb_funcall(rb_funcall(port, rb_intern("string"), 0), rb_intern("encoding"), 0);
  }
  else if (rb_respond_to(port, rb_intern("encoding"))) {
    r_encoding = rb_funcall(port, rb_intern("encoding"), 0);
  }
  else {
    r_encoding = rb_enc_from_encoding(rb_ascii8bit_encoding());
  }

  // @see CSV#initialize
  // @see https://github.com/ruby/ruby/blob/ab337e61ecb5f42384ba7d710c36faf96a454e5c/lib/csv.rb#L1510
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

  // In case #raw_parse is called multiple times on the same parser. Note that
  // using IO methods on a re-used parser can cause segmentation faults.
  rb_ivar_set(self, s_row, Qnil);

  buffer_size = BUFSIZE;
  if (rb_ivar_defined(self, rb_intern("@buffer_size")) == Qtrue) {
    bufsize = rb_ivar_get(self, rb_intern("@buffer_size"));
    if (!NIL_P(bufsize)) {
      buffer_size = NUM2INT(bufsize);
      // buffer_size = 0 can cause segmentation faults.
      if (buffer_size == 0) {
        buffer_size = BUFSIZE;
      }
    }
  }

  if (io) {
    buf = ALLOC_N(char, buffer_size);
  }

  
#line 266 "ext/fastcsv/fastcsv.c"
	{
	cs = raw_parse_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 386 "ext/fastcsv/fastcsv.rl"

  while (!done) {
    VALUE str;
    char *p, *pe;
    int len, space = buffer_size - have, tokstart_diff, tokend_diff, start_diff, mark_row_sep_diff;

    if (io) {
      if (space == 0) {
        // Not moving d->start will cause intermittent segmentation faults.
        tokstart_diff = ts - buf;
        tokend_diff = te - buf;
        start_diff = d->start - buf;
        mark_row_sep_diff = mark_row_sep - buf;

        buffer_size += BUFSIZE;
        REALLOC_N(buf, char, buffer_size);

        space = buffer_size - have;

        ts = buf + tokstart_diff;
        te = buf + tokend_diff;
        d->start = buf + start_diff;
        mark_row_sep = buf + mark_row_sep_diff;
      }
      p = buf + have;

      // Reads "`length` bytes without any conversion (binary mode)."
      // "The resulted string is always ASCII-8BIT encoding."
      // @see http://www.ruby-doc.org/core-2.1.4/IO.html#method-i-read
      str = rb_funcall(port, s_read, 1, INT2FIX(space));
      if (NIL_P(str)) {
        // "`nil` means it met EOF at beginning," e.g. for `StringIO.new("")`.
        len = 0;
      }
      else {
        len = RSTRING_LEN(str);
        memcpy(p, StringValuePtr(str), len);
      }

      // "The 1 to `length`-1 bytes string means it met EOF after reading the result."
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

    if (d->start == 0) {
      d->start = p;
    }

    pe = p + len;
    
#line 336 "ext/fastcsv/fastcsv.c"
	{
	short _widec;
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
	goto st5;
tr5:
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st5;
tr6:
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st5;
tr7:
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st5;
tr13:
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st5;
tr19:
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st5;
tr20:
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st5;
tr42:
#line 170 "ext/fastcsv/fastcsv.rl"
	{te = p;p--;}
	goto st5;
tr43:
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
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
	goto st5;
tr50:
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
	goto st5;
tr56:
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{te = p+1;}
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st5;
st5:
#line 1 "NONE"
	{ts = 0;}
#line 1 "NONE"
	{act = 0;}
	if ( ++p == pe )
		goto _test_eof5;
case 5:
#line 1 "NONE"
	{ts = p;}
#line 630 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr33;
		case 1290: goto tr3;
		case 1293: goto tr4;
		case 1536: goto tr35;
		case 1546: goto tr36;
		case 1549: goto tr37;
		case 1792: goto tr7;
		case 1802: goto tr8;
		case 1805: goto tr9;
		case 2048: goto tr39;
		case 2058: goto tr40;
		case 2061: goto tr41;
	}
	if ( _widec < 1408 ) {
		if ( 1152 <= _widec && _widec <= 1407 )
			goto st1;
	} else if ( _widec > 1663 ) {
		if ( _widec > 1919 ) {
			if ( 1920 <= _widec && _widec <= 2175 )
				goto tr38;
		} else if ( _widec >= 1664 )
			goto tr6;
	} else
		goto tr34;
	goto st0;
st0:
cs = 0;
	goto _out;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr2;
		case 1290: goto tr3;
		case 1293: goto tr4;
		case 1536: goto tr5;
		case 1546: goto tr3;
		case 1549: goto tr4;
		case 1792: goto tr7;
		case 1802: goto tr8;
		case 1805: goto tr9;
		case 2048: goto tr7;
		case 2058: goto tr8;
		case 2061: goto tr9;
	}
	if ( _widec > 1407 ) {
		if ( 1664 <= _widec && _widec <= 2175 )
			goto tr6;
	} else if ( _widec >= 1152 )
		goto st1;
	goto tr0;
tr2:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st6;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
#line 738 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr2;
		case 1290: goto tr3;
		case 1293: goto tr4;
		case 1536: goto tr5;
		case 1546: goto tr3;
		case 1549: goto tr4;
		case 1792: goto tr7;
		case 1802: goto tr8;
		case 1805: goto tr9;
		case 2048: goto tr7;
		case 2058: goto tr8;
		case 2061: goto tr9;
	}
	if ( _widec > 1407 ) {
		if ( 1664 <= _widec && _widec <= 2175 )
			goto tr6;
	} else if ( _widec >= 1152 )
		goto st1;
	goto tr42;
tr3:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st7;
tr8:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st7;
tr14:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st7;
tr21:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st7;
tr44:
#line 1 "NONE"
	{te = p+1;}
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st7;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
#line 909 "ext/fastcsv/fastcsv.c"
	goto tr43;
tr4:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st8;
tr9:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st8;
tr15:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st8;
tr22:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
#line 1047 "ext/fastcsv/fastcsv.c"
	if ( (*p) == 10 )
		goto tr44;
	goto tr43;
tr33:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 1089 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	if ( _widec < 1291 ) {
		if ( 1152 <= _widec && _widec <= 1289 )
			goto st1;
	} else if ( _widec > 1292 ) {
		if ( 1294 <= _widec && _widec <= 1407 )
			goto st1;
	} else
		goto st1;
	goto tr42;
tr34:
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
	goto st2;
tr45:
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
#line 1138 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(128 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	switch( _widec ) {
		case 522: goto tr12;
		case 525: goto tr12;
	}
	if ( _widec < 257 ) {
		if ( 128 <= _widec && _widec <= 255 )
			goto st2;
	} else if ( _widec > 383 ) {
		if ( 384 <= _widec && _widec <= 639 )
			goto tr11;
	} else
		goto st2;
	goto tr0;
tr11:
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
	goto st3;
tr46:
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 1262 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr13;
		case 1290: goto tr14;
		case 1293: goto tr15;
		case 1536: goto tr16;
		case 1546: goto tr17;
		case 1549: goto tr18;
		case 1792: goto tr20;
		case 1802: goto tr21;
		case 1805: goto tr22;
		case 2048: goto tr24;
		case 2058: goto tr25;
		case 2061: goto tr26;
	}
	if ( _widec < 1664 ) {
		if ( 1408 <= _widec && _widec <= 1663 )
			goto st2;
	} else if ( _widec > 1919 ) {
		if ( 1920 <= _widec && _widec <= 2175 )
			goto tr23;
	} else
		goto tr19;
	goto tr0;
tr16:
#line 1 "NONE"
	{te = p+1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st10;
tr23:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
	goto st10;
tr24:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st10;
tr35:
#line 1 "NONE"
	{te = p+1;}
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st10;
tr38:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
	goto st10;
tr39:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st10;
tr55:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st10;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
#line 1497 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(128 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	switch( _widec ) {
		case 522: goto tr12;
		case 525: goto tr12;
	}
	if ( _widec < 257 ) {
		if ( 128 <= _widec && _widec <= 255 )
			goto st2;
	} else if ( _widec > 383 ) {
		if ( 384 <= _widec && _widec <= 639 )
			goto tr11;
	} else
		goto st2;
	goto tr0;
tr12:
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
	goto st4;
tr47:
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st4;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
#line 1621 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr13;
		case 1290: goto tr17;
		case 1293: goto tr18;
		case 1536: goto tr27;
		case 1546: goto tr28;
		case 1549: goto tr28;
		case 1792: goto tr20;
		case 1802: goto tr25;
		case 1805: goto tr26;
		case 2048: goto tr30;
		case 2058: goto tr31;
		case 2061: goto tr31;
	}
	if ( _widec < 1408 ) {
		if ( 1152 <= _widec && _widec <= 1407 )
			goto st2;
	} else if ( _widec > 1663 ) {
		if ( _widec > 1919 ) {
			if ( 1920 <= _widec && _widec <= 2175 )
				goto tr29;
		} else if ( _widec >= 1664 )
			goto tr23;
	} else
		goto tr12;
	goto tr0;
tr17:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st11;
tr25:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st11;
tr36:
#line 1 "NONE"
	{te = p+1;}
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st11;
tr40:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st11;
tr48:
#line 1 "NONE"
	{te = p+1;}
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st11;
tr51:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st11;
tr57:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st11;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
#line 1918 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(128 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	switch( _widec ) {
		case 256: goto tr43;
		case 522: goto tr47;
		case 525: goto tr47;
	}
	if ( _widec > 383 ) {
		if ( 384 <= _widec && _widec <= 639 )
			goto tr46;
	} else if ( _widec >= 128 )
		goto tr45;
	goto tr0;
tr18:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st12;
tr26:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st12;
tr37:
#line 1 "NONE"
	{te = p+1;}
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st12;
tr41:
#line 1 "NONE"
	{te = p+1;}
#line 49 "ext/fastcsv/fastcsv.rl"
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
#line 41 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = curline;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st12;
tr52:
#line 1 "NONE"
	{te = p+1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st12;
tr58:
#line 1 "NONE"
	{te = p+1;}
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st12;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
#line 2172 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(128 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	switch( _widec ) {
		case 256: goto tr43;
		case 266: goto tr48;
		case 522: goto tr49;
		case 525: goto tr47;
	}
	if ( _widec > 383 ) {
		if ( 384 <= _widec && _widec <= 639 )
			goto tr46;
	} else if ( _widec >= 128 )
		goto tr45;
	goto tr0;
tr28:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st13;
tr31:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
	goto st13;
tr49:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st13;
tr54:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 169 "ext/fastcsv/fastcsv.rl"
	{act = 2;}
	goto st13;
tr61:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 119 "ext/fastcsv/fastcsv.rl"
	{
    mark_row_sep = p;

    if (d->start == 0 || p == d->start) {
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) { // same as new_field
      rb_ary_push(row, field);
      field = Qnil;
    }

    rb_yield(row);
    row = rb_ary_new();
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st13;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
#line 2562 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr50;
		case 1290: goto tr51;
		case 1293: goto tr52;
		case 1536: goto tr53;
		case 1546: goto tr54;
		case 1549: goto tr54;
		case 1792: goto tr56;
		case 1802: goto tr57;
		case 1805: goto tr58;
		case 2048: goto tr60;
		case 2058: goto tr61;
		case 2061: goto tr61;
	}
	if ( _widec < 1408 ) {
		if ( 1152 <= _widec && _widec <= 1407 )
			goto tr45;
	} else if ( _widec > 1663 ) {
		if ( _widec > 1919 ) {
			if ( 1920 <= _widec && _widec <= 2175 )
				goto tr59;
		} else if ( _widec >= 1664 )
			goto tr55;
	} else
		goto tr47;
	goto tr0;
tr27:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st14;
tr29:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
	goto st14;
tr30:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st14;
tr53:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
#line 170 "ext/fastcsv/fastcsv.rl"
	{act = 3;}
	goto st14;
tr59:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
	goto st14;
tr60:
#line 1 "NONE"
	{te = p+1;}
#line 60 "ext/fastcsv/fastcsv.rl"
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
#line 45 "ext/fastcsv/fastcsv.rl"
	{
    unclosed_line = 0;
  }
#line 95 "ext/fastcsv/fastcsv.rl"
	{
    rb_ary_push(row, field);
    field = Qnil;
  }
#line 168 "ext/fastcsv/fastcsv.rl"
	{act = 1;}
#line 100 "ext/fastcsv/fastcsv.rl"
	{
    d->start = p;

    if (len_row_sep) {
      if (p - mark_row_sep != len_row_sep || row_sep[0] != *mark_row_sep || (len_row_sep == 2 && row_sep[1] != *(mark_row_sep + 1))) {
        FREE;

        rb_raise(eError, "Unquoted fields do not allow \\r or \\n (line %d).", curline);
      }
    }
    else {
      len_row_sep = p - mark_row_sep;
      row_sep = ALLOC_N(char, len_row_sep);
      memcpy(row_sep, mark_row_sep, len_row_sep);
    }

    curline++;
  }
#line 138 "ext/fastcsv/fastcsv.rl"
	{
    if (d->start == 0 || p == d->start) { // same as new_row
      rb_ivar_set(self, s_row, rb_str_new2(""));
    }
    else if (p > d->start) {
      rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
    }

    if (!NIL_P(field) || RARRAY_LEN(row)) {
      rb_ary_push(row, field);
    }

    if (RARRAY_LEN(row)) {
      rb_yield(row);
    }
  }
	goto st14;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
#line 3016 "ext/fastcsv/fastcsv.c"
	_widec = (*p);
	_widec = (short)(1152 + ((*p) - -128));
	if ( 
#line 156 "ext/fastcsv/fastcsv.rl"
 (*p) == quote_char  ) _widec += 256;
	if ( 
#line 157 "ext/fastcsv/fastcsv.rl"
 (*p) == col_sep  ) _widec += 512;
	switch( _widec ) {
		case 1280: goto tr13;
		case 1290: goto tr17;
		case 1293: goto tr18;
		case 1536: goto tr27;
		case 1546: goto tr28;
		case 1549: goto tr28;
		case 1792: goto tr20;
		case 1802: goto tr25;
		case 1805: goto tr26;
		case 2048: goto tr30;
		case 2058: goto tr31;
		case 2061: goto tr31;
	}
	if ( _widec < 1408 ) {
		if ( 1152 <= _widec && _widec <= 1407 )
			goto st2;
	} else if ( _widec > 1663 ) {
		if ( _widec > 1919 ) {
			if ( 1920 <= _widec && _widec <= 2175 )
				goto tr29;
		} else if ( _widec >= 1664 )
			goto tr23;
	} else
		goto tr12;
	goto tr0;
	}
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 1: goto tr0;
	case 6: goto tr42;
	case 7: goto tr43;
	case 8: goto tr43;
	case 9: goto tr42;
	case 2: goto tr0;
	case 3: goto tr0;
	case 10: goto tr0;
	case 4: goto tr0;
	case 11: goto tr43;
	case 12: goto tr43;
	case 13: goto tr43;
	case 14: goto tr0;
	}
	}

	_out: {}
	}

#line 447 "ext/fastcsv/fastcsv.rl"

    if (done && cs < raw_parse_first_final) {
      if (d->start == 0 || p == d->start) { // same as new_row
        rb_ivar_set(self, s_row, rb_str_new2(""));
      }
      else if (p > d->start) {
        rb_ivar_set(self, s_row, rb_str_new(d->start, p - d->start));
      }

      FREE;

      if (unclosed_line) {
        rb_raise(eError, "Unclosed quoted field on line %d.", unclosed_line);
      }
      else {
        rb_raise(eError, "Illegal quoting in line %d.", curline);
      }
    }

    if (ts == 0) {
      have = 0;
    }
    else if (io) {
      have = pe - ts;
      memmove(buf, ts, have);
      // @see https://github.com/hpricot/hpricot/blob/master/ext/hpricot_scan/hpricot_scan.rl#L92
      if (d->start > ts) {
        d->start = buf + (d->start - ts);
      }
      if (mark_row_sep > ts) {
        mark_row_sep = buf + (mark_row_sep - ts);
      }
      te = buf + (te - ts);
      ts = buf;
    }
  }

  FREE;

  return Qnil;
}

// @see https://github.com/ruby/ruby/blob/trunk/README.EXT#L616
static VALUE allocate(VALUE class) {
  // @see https://github.com/nofxx/georuby_c/blob/b3b91fd90980d7c295ac8f6012d89878ea7cd569/ext/line.c#L66
  Data *d = ALLOC(Data);
  d->start = 0;
  // @see https://github.com/nofxx/georuby_c/blob/b3b91fd90980d7c295ac8f6012d89878ea7cd569/ext/point.h#L26
  // rb_gc_mark(d->start) or rb_gc_mark(d) cause warning "passing argument 1 of ‘rb_gc_mark’ makes integer from pointer without a cast"
  // free(d->start) causes error "pointer being freed was not allocated"
  return Data_Wrap_Struct(class, NULL, free, d);
}

// @see http://tenderlovemaking.com/2009/12/18/writing-ruby-c-extensions-part-1.html
// @see http://tenderlovemaking.com/2010/12/11/writing-ruby-c-extensions-part-2.html
void Init_fastcsv() {
  s_read = rb_intern("read");
  s_row = rb_intern("@row");

  cClass = rb_define_class("FastCSV", rb_const_get(rb_cObject, rb_intern("CSV"))); // class FastCSV < CSV
  cParser = rb_define_class_under(cClass, "Parser", rb_cObject);                   //   class Parser
  rb_define_alloc_func(cParser, allocate);                                         //
  rb_define_method(cParser, "raw_parse", raw_parse, -1);                           //     def raw_parse(port, opts = nil); end
  rb_define_attr(cParser, "row", 1, 0);                                            //     attr_reader :row
  rb_define_attr(cParser, "buffer_size", 1, 1);                                    //     attr_accessor :buffer_size
                                                                                   //   end
  eError = rb_define_class_under(cClass, "MalformedCSVError", rb_eRuntimeError);   //   class MalformedCSVError < RuntimeError
                                                                                   // end
}
