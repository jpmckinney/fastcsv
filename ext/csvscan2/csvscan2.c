
#line 1 "ext/csvscan2/csvscan2.rl"
#include <ruby.h>

static VALUE mCSVScan, rb_eCSVScanParseError;
static ID s_read, s_to_str;


#line 69 "ext/csvscan2/csvscan2.rl"



#line 14 "ext/csvscan2/csvscan2.c"
static const char _csvscan2_actions[] = {
	0, 1, 0, 1, 4, 1, 9, 1, 
	11, 1, 12, 1, 13, 1, 14, 2, 
	2, 3, 2, 5, 6, 2, 5, 7, 
	2, 5, 8, 3, 0, 1, 10, 3, 
	9, 0, 1
};

static const char _csvscan2_key_offsets[] = {
	0, 0, 2, 10, 14, 15
};

static const char _csvscan2_trans_keys[] = {
	10, 34, 9, 10, 13, 32, 34, 44, 
	11, 12, 10, 13, 34, 44, 10, 34, 
	0
};

static const char _csvscan2_single_lengths[] = {
	0, 2, 6, 4, 1, 1
};

static const char _csvscan2_range_lengths[] = {
	0, 0, 1, 0, 0, 0
};

static const char _csvscan2_index_offsets[] = {
	0, 0, 3, 11, 16, 18
};

static const char _csvscan2_trans_targs[] = {
	1, 5, 1, 2, 2, 4, 2, 1, 
	2, 3, 3, 2, 2, 2, 2, 3, 
	2, 2, 1, 2, 2, 2, 2, 2, 
	0
};

static const char _csvscan2_trans_actions[] = {
	1, 24, 0, 5, 31, 0, 5, 0, 
	7, 18, 21, 13, 13, 13, 13, 21, 
	27, 9, 0, 11, 13, 13, 9, 11, 
	0
};

static const char _csvscan2_to_state_actions[] = {
	0, 0, 15, 0, 0, 0
};

static const char _csvscan2_from_state_actions[] = {
	0, 0, 3, 0, 0, 0
};

static const char _csvscan2_eof_trans[] = {
	0, 22, 0, 22, 23, 24
};

static const int csvscan2_start = 2;
static const int csvscan2_error = 0;

static const int csvscan2_en_main = 2;


#line 72 "ext/csvscan2/csvscan2.rl"

#define BUFSIZE 16384

VALUE csvscan2(int argc, VALUE *argv, VALUE self)
{
  int cs, act, have = 0, nread = 0, curline = 1, io = 0;
  char *ts = 0, *te = 0, *buf = NULL, *eof = NULL;

  VALUE port;
  VALUE bufsize = Qnil;
  int done = 0, buffer_size = 0, taint = 0;

  rb_scan_args(argc, argv, "1", &port);
  taint = OBJ_TAINTED(port);
  io = rb_respond_to(port, s_read);
  if (!io)
  {
    if (rb_respond_to(port, s_to_str))
    {
      port = rb_funcall(port, s_to_str, 0);
      StringValue(port);
    }
    else
    {
      rb_raise(rb_eArgError, "a CSV table must be built from an input source (a String or IO object.)");
    }
  }

  buffer_size = BUFSIZE;
  if (rb_ivar_defined(self, rb_intern("@buffer_size")) == Qtrue) {
    bufsize = rb_ivar_get(self, rb_intern("@buffer_size"));
    if (!NIL_P(bufsize)) {
      buffer_size = NUM2INT(bufsize);
    }
  }

  if (io)
    buf = ALLOC_N(char, buffer_size);

  
#line 118 "ext/csvscan2/csvscan2.c"
	{
	cs = csvscan2_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 112 "ext/csvscan2/csvscan2.rl"

  VALUE row, coldata;
  row = rb_ary_new();
  coldata = Qnil;

  while (!done) {
    VALUE str;
    char *p, *pe;
    int len, space = buffer_size - have, tokstart_diff, tokend_diff;

    if (io)
    {
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
      len = RSTRING_LEN(str);
      memcpy(p, StringValuePtr(str), len);
    }
    else
    {
      p = RSTRING_PTR(port);
      len = RSTRING_LEN(port) + 1;
      done = 1;
    }

    nread += len;

    /* If this is the last buffer, tack on an EOF. */
    if (io && len < space) {
      p[len++] = 0;
      done = 1;
    }

    pe = p + len;
    
#line 174 "ext/csvscan2/csvscan2.c"
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
	_acts = _csvscan2_actions + _csvscan2_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 4:
#line 1 "NONE"
	{ts = p;}
	break;
#line 195 "ext/csvscan2/csvscan2.c"
		}
	}

	_keys = _csvscan2_trans_keys + _csvscan2_key_offsets[cs];
	_trans = _csvscan2_index_offsets[cs];

	_klen = _csvscan2_single_lengths[cs];
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

	_klen = _csvscan2_range_lengths[cs];
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
	cs = _csvscan2_trans_targs[_trans];

	if ( _csvscan2_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _csvscan2_actions + _csvscan2_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 9 "ext/csvscan2/csvscan2.rl"
	{
    curline += 1;
  }
	break;
	case 1:
#line 19 "ext/csvscan2/csvscan2.rl"
	{
      rb_ary_push(row, coldata);
      rb_yield(row);
      coldata = Qnil;
      row = rb_ary_new();
    }
	break;
	case 5:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 6:
#line 18 "ext/csvscan2/csvscan2.rl"
	{act = 1;}
	break;
	case 7:
#line 29 "ext/csvscan2/csvscan2.rl"
	{act = 4;}
	break;
	case 8:
#line 48 "ext/csvscan2/csvscan2.rl"
	{act = 5;}
	break;
	case 9:
#line 18 "ext/csvscan2/csvscan2.rl"
	{te = p+1;}
	break;
	case 10:
#line 24 "ext/csvscan2/csvscan2.rl"
	{te = p+1;}
	break;
	case 11:
#line 25 "ext/csvscan2/csvscan2.rl"
	{te = p+1;{
      rb_ary_push(row, coldata);
      coldata = Qnil;
    }}
	break;
	case 12:
#line 18 "ext/csvscan2/csvscan2.rl"
	{te = p;p--;}
	break;
	case 13:
#line 48 "ext/csvscan2/csvscan2.rl"
	{te = p;p--;{
      unsigned char ch, *start_p, *wptr, *rptr;
      int rest, datalen;
      start_p = wptr = ts;
      rptr = ts + 1;
      rest = te - ts - 2;
      datalen = 0;
      while(rest>0) {
        ch = *rptr++;
        if (ch=='"') {
        rptr++;
        rest--;
        }
        *wptr++ = ch;
        datalen++;
        rest--;
      }
      coldata = rb_str_new( start_p, datalen );
    }}
	break;
	case 14:
#line 1 "NONE"
	{	switch( act ) {
	case 0:
	{{cs = 0; goto _again;}}
	break;
	case 4:
	{{p = ((te))-1;}
      unsigned char ch, *endp;
      int datalen;
      datalen = te - ts;
      endp = te - 1;
      while(datalen>0) {
        ch = *endp--;
        if (ch==' ' || ch=='\t') {
          datalen--;
        } else {
          break;
        }
      }
      if (datalen==0) {
        coldata = Qnil;
      } else {
        coldata = rb_str_new(ts, datalen);
      }
    }
	break;
	case 5:
	{{p = ((te))-1;}
      unsigned char ch, *start_p, *wptr, *rptr;
      int rest, datalen;
      start_p = wptr = ts;
      rptr = ts + 1;
      rest = te - ts - 2;
      datalen = 0;
      while(rest>0) {
        ch = *rptr++;
        if (ch=='"') {
        rptr++;
        rest--;
        }
        *wptr++ = ch;
        datalen++;
        rest--;
      }
      coldata = rb_str_new( start_p, datalen );
    }
	break;
	default:
	{{p = ((te))-1;}}
	break;
	}
	}
	break;
#line 386 "ext/csvscan2/csvscan2.c"
		}
	}

_again:
	_acts = _csvscan2_actions + _csvscan2_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
#line 1 "NONE"
	{ts = 0;}
	break;
	case 3:
#line 1 "NONE"
	{act = 0;}
	break;
#line 403 "ext/csvscan2/csvscan2.c"
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _csvscan2_eof_trans[cs] > 0 ) {
		_trans = _csvscan2_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

#line 159 "ext/csvscan2/csvscan2.rl"

    if (cs == csvscan2_error) {
      if (buf != NULL)
        free(buf);
      rb_raise(rb_eCSVScanParseError, "parse error on line %d.", curline);
    }

    if (ts == 0)
    {
      have = 0;
    }
    else if (io)
    {
      have = pe - ts;
      memmove(buf, ts, have);
      te = buf + (te - ts);
      ts = buf;
    }
  }

  if (buf != NULL)
    free(buf);

  return Qnil;
}

void Init_csvscan2() {
  s_read = rb_intern("read");
  s_to_str = rb_intern("to_str");

  mCSVScan = rb_define_module("CSVScan");
  rb_define_attr(rb_singleton_class(mCSVScan), "buffer_size", 1, 1);
  rb_define_singleton_method(mCSVScan, "scan", csvscan2, -1);
  rb_eCSVScanParseError = rb_define_class_under(mCSVScan, "ParseError", rb_eStandardError);
}
