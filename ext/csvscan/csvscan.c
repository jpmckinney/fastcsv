#line 1 "csvscan.rl"
#include <ruby.h>

static VALUE rb_eCSVParseError;
static ID s_read, s_to_str;

#line 70 "csvscan.rl"



#line 12 "csvscan.c"
static const int csv_scan_start = 2;

static const int csv_scan_error = 1;

#line 73 "csvscan.rl"

#define BUFSIZE 131072

VALUE csv_scan(VALUE self, VALUE port) {
    int cs, act, have = 0, nread = 0, curline = 1;
    unsigned char *tokstart = NULL, *tokend = NULL, *buf;
    VALUE row, coldata;
    VALUE bufsize = Qnil;
    int done=0, buffer_size;

    if ( !rb_respond_to( port, s_read ) ) {
        if ( rb_respond_to( port, s_to_str ) ) {
            port = rb_funcall( port, s_to_str, 0 );
            StringValue(port);
        } else {
            rb_raise( rb_eArgError, "bad argument, String or IO only please." );
        }
    }

    buffer_size = BUFSIZE;
    if (rb_ivar_defined(self, rb_intern("@buffer_size")) == Qtrue) {
        bufsize = rb_ivar_get(self, rb_intern("@buffer_size"));
        if (!NIL_P(bufsize)) {
            buffer_size = NUM2INT(bufsize);
        }
    }
    buf = ALLOC_N(unsigned char, buffer_size);

    
#line 47 "csvscan.c"
	{
	cs = csv_scan_start;
	tokstart = 0;
	tokend = 0;
	act = 0;
	}
#line 102 "csvscan.rl"

    row = rb_ary_new();
    coldata = Qnil;

    while( !done ) {
        VALUE str;
        unsigned char *p = buf + have, *pe;
        int len, space = buffer_size - have;

        if ( space == 0 ) {
            rb_raise(rb_eCSVParseError, "ran out of buffer on line %d.", curline);
        }

        if ( rb_respond_to( port, s_read ) ) {
            str = rb_funcall( port, s_read, 1, INT2FIX(space) );
        } else {
            str = rb_str_substr( port, nread, space );
        }

        StringValue(str);
        memcpy( p, RSTRING(str)->ptr, RSTRING(str)->len );
        len = RSTRING(str)->len;
        nread += len;

        /* If this is the last buffer, tack on an EOF. */
        if ( len < space ) {
            p[len++] = 0;
            done = 1;
        }

        pe = p + len;
        
#line 87 "csvscan.c"
	{
	if ( p == pe )
		goto _out;
	switch ( cs )
	{
tr0:
#line 19 "csvscan.rl"
	{tokend = p;{p = ((tokend))-1;}}
	goto st2;
tr1:
#line 10 "csvscan.rl"
	{
        curline += 1;
    }
#line 20 "csvscan.rl"
	{
          rb_ary_push(row, coldata);
          rb_yield(row);
          coldata = Qnil;
          row = rb_ary_new();
      }
#line 20 "csvscan.rl"
	{tokend = p+1;{p = ((tokend))-1;}}
	goto st2;
tr2:
#line 49 "csvscan.rl"
	{tokend = p;{
          unsigned char ch, *start_p, *wptr, *rptr;
          int rest, datalen;
          start_p = wptr = tokstart;
          rptr = tokstart + 1;
          rest = tokend - tokstart - 2;
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
      }{p = ((tokend))-1;}}
	goto st2;
tr5:
#line 1 "csvscan.rl"
	{	switch( act ) {
	case 0: tokend = tokstart; {goto st1;}
	case 4:
	{
          unsigned char ch, *endp;
          int datalen;
          datalen = tokend - tokstart;
          endp = tokend - 1;
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
              coldata = rb_str_new(tokstart, datalen);
          }
      }
	break;
	case 5:
	{
          unsigned char ch, *start_p, *wptr, *rptr;
          int rest, datalen;
          start_p = wptr = tokstart;
          rptr = tokstart + 1;
          rest = tokend - tokstart - 2;
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
	default: break;
	}
	{p = ((tokend))-1;}}
	goto st2;
tr6:
#line 19 "csvscan.rl"
	{tokend = p+1;{p = ((tokend))-1;}}
	goto st2;
tr7:
#line 19 "csvscan.rl"
	{tokend = p+1;{p = ((tokend))-1;}}
#line 10 "csvscan.rl"
	{
        curline += 1;
    }
#line 20 "csvscan.rl"
	{
          rb_ary_push(row, coldata);
          rb_yield(row);
          coldata = Qnil;
          row = rb_ary_new();
      }
	goto st2;
tr10:
#line 26 "csvscan.rl"
	{tokend = p+1;{
          rb_ary_push(row, coldata);
          coldata = Qnil;
      }{p = ((tokend))-1;}}
	goto st2;
st2:
#line 1 "csvscan.rl"
	{tokstart = 0;}
#line 1 "csvscan.rl"
	{act = 0;}
	if ( ++p == pe )
		goto _out2;
case 2:
#line 1 "csvscan.rl"
	{tokstart = p;}
#line 220 "csvscan.c"
	switch( (*p) ) {
		case 9u: goto tr6;
		case 10u: goto tr7;
		case 13u: goto st4;
		case 32u: goto tr6;
		case 34u: goto st0;
		case 44u: goto tr10;
	}
	if ( 11u <= (*p) && (*p) <= 12u )
		goto tr8;
	goto tr4;
tr4:
#line 1 "csvscan.rl"
	{tokend = p+1;}
#line 30 "csvscan.rl"
	{act = 4;}
	goto st3;
tr8:
#line 1 "csvscan.rl"
	{tokend = p+1;}
#line 19 "csvscan.rl"
	{act = 1;}
	goto st3;
st3:
	if ( ++p == pe )
		goto _out3;
case 3:
#line 248 "csvscan.c"
	switch( (*p) ) {
		case 10u: goto tr5;
		case 13u: goto tr5;
		case 34u: goto tr5;
		case 44u: goto tr5;
	}
	goto tr4;
st4:
	if ( ++p == pe )
		goto _out4;
case 4:
	if ( (*p) == 10u )
		goto tr1;
	goto tr0;
tr11:
#line 10 "csvscan.rl"
	{
        curline += 1;
    }
	goto st0;
st0:
	if ( ++p == pe )
		goto _out0;
case 0:
#line 273 "csvscan.c"
	switch( (*p) ) {
		case 10u: goto tr11;
		case 34u: goto tr12;
	}
	goto st0;
tr12:
#line 1 "csvscan.rl"
	{tokend = p+1;}
#line 49 "csvscan.rl"
	{act = 5;}
	goto st5;
st5:
	if ( ++p == pe )
		goto _out5;
case 5:
#line 289 "csvscan.c"
	if ( (*p) == 34u )
		goto st0;
	goto tr2;
st1:
	goto _out1;
	}
	_out2: cs = 2; goto _out; 
	_out3: cs = 3; goto _out; 
	_out4: cs = 4; goto _out; 
	_out0: cs = 0; goto _out; 
	_out5: cs = 5; goto _out; 
	_out1: cs = 1; goto _out; 

	_out: {}
	}
#line 134 "csvscan.rl"

        if ( cs == csv_scan_error ) {
            free(buf);
            rb_raise(rb_eCSVParseError, "parse error on line %d.", curline);
        }

        if ( tokstart == 0 ) {
            have = 0;
        } else {
            have = pe - tokstart;
            memmove( buf, tokstart, have );
            tokend = buf + (tokend - tokstart);
            tokstart = buf;
        }
    }
    free(buf);
    return Qnil;
}

void Init_csvscan() {
  VALUE mCSVScan = rb_define_module("CSVScan");
  rb_define_attr(rb_singleton_class(mCSVScan), "buffer_size", 1, 1);
  rb_define_singleton_method(mCSVScan, "scan", csv_scan, 1);
  rb_eCSVParseError = rb_define_class_under(mCSVScan, "ParseError", rb_eException);

  s_read = rb_intern("read");
  s_to_str = rb_intern("to_str");
}
