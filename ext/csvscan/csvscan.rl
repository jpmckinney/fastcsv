#include <ruby.h>

static VALUE rb_eCSVParseError;
static ID s_read, s_to_str;

%%{
    machine csv_scan;
    alphtype unsigned char;
    
    newline = ('\r\n' | '\n') @{
        curline += 1;
    };
    Separator = ',';
    UnQuotedValue = [^ \t",\r\n] [^",\r\n]*;
    QuotedChar = ( '""' | [^"] | newline );
    QuotedValue = '"' . QuotedChar* . '"';
    
    main := |*
      space;
      newline @{
          rb_ary_push(row, coldata);
          rb_yield(row);
          coldata = Qnil;
          row = rb_ary_new();
      };
      Separator {
          rb_ary_push(row, coldata);
          coldata = Qnil;
      };
      UnQuotedValue {
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
      };
      QuotedValue {
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
      };
    *|;

}%%

%% write data nofinal;

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

    %% write init;

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
        %% write exec;

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
