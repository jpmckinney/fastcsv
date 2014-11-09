#include <ruby.h>

static VALUE mCSVScan, rb_eCSVScanParseError;
static ID s_read, s_to_str;

%%{
  machine csvscan2;

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
    };
    QuotedValue {
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
    };
  *|;

}%%

%% write data nofinal;

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

  %% write init;

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
    %% write exec;

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
