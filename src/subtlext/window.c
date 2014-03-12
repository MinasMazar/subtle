 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/window.c,v 3204 2012/05/22 21:15:06 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* Flags {{{ */
#define WINDOW_INPUT_FUNC      (1L << 2)
#define WINDOW_FOREIGN_WIN     (1L << 3)
/* }}} */

/* Typedefs {{{ */
typedef struct subtlextwindow_t
{
  GC                 gc;
  int                flags, ntext;
  unsigned long      fg, bg;
  Window             win;
  VALUE              instance, expose, keyboard, pointer;
  SubFont            *font;
} SubtlextWindow;
/* }}} */

/* WindowMark {{{ */
static void
WindowMark(SubtlextWindow *w)
{
  if(w)
    {
      rb_gc_mark(w->instance);
      if(RTEST(w->expose))     rb_gc_mark(w->expose);
      if(RTEST(w->keyboard))   rb_gc_mark(w->keyboard);
      if(RTEST(w->pointer))    rb_gc_mark(w->pointer);
    }
} /* }}} */

/* WindowSweep {{{ */
static void
WindowSweep(SubtlextWindow *w)
{
  if(w)
    {
      /* Destroy window */
      if(!(w->flags & WINDOW_FOREIGN_WIN))
        XDestroyWindow(display, w->win);

      if(0 != w->gc) XFreeGC(display, w->gc);
      if(w->font) subSharedFontKill(display, w->font);

      free(w);
    }
} /* }}} */

/* WindowCall {{{ */
static VALUE
WindowCall(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  return rb_funcall(rargs[0], rargs[1], rargs[2], rargs[3], rargs[4]);
} /* }}} */

/* WindowExpose {{{ */
static void
WindowExpose(SubtlextWindow *w)
{
  if(w)
    {
      XClearWindow(display, w->win);

      /* Call expose proc if any */
      if(RTEST(w->expose))
        {
          int state = 0;
          VALUE rargs[5] = { Qnil };

          /* Wrap up data */
          rargs[0] = w->expose;
          rargs[1] = rb_intern("call");
          rargs[2] = 1;
          rargs[3] = w->instance;

          /* Carefully call listen proc */
          rb_protect(WindowCall, (VALUE)&rargs, &state);
          if(state) subSubtlextBacktrace();
        }

     XSync(display, False); ///< Sync with X
  }
} /* }}} */

/* WindowGrab {{{ */
static VALUE
WindowGrab(SubtlextWindow *w)
{
  XEvent ev;
  int loop = True, state = 0;
  char buf[32] = { 0 };
  unsigned long *focus = NULL, mask = 0;
  VALUE result = Qnil, rargs[5] = { Qnil }, sym = Qnil, ary = Qnil;
  KeySym keysym;

  /* Add grabs */
  if(RTEST(w->keyboard))
    {
      mask |= KeyPressMask;

      XGrabKeyboard(display, w->win, True, GrabModeAsync,
        GrabModeAsync, CurrentTime);
    }
  if(RTEST(w->pointer))
    {
      mask |= ButtonPressMask;

      XGrabPointer(display, w->win, True, ButtonPressMask, GrabModeAsync,
        GrabModeAsync, None, None, CurrentTime);
    }

  XMapRaised(display, w->win);
  XSelectInput(display, w->win, mask);
  XSetInputFocus(display, w->win, RevertToPointerRoot, CurrentTime);
  WindowExpose(w);
  XFlush(display);

  while(loop)
    {
      XMaskEvent(display, mask, &ev);
      switch(ev.type)
        {
          case KeyPress: /* {{{ */
            XLookupString(&ev.xkey, buf, sizeof(buf), &keysym, NULL);

            /* Skip modifier keys */
            if(IsModifierKey(keysym)) continue;

            /* Translate syms to something meaningful */
            switch(keysym)
              {
                /* Arrow keys */
                case XK_KP_Left:
                case XK_Left:      sym = CHAR2SYM("left");      break;
                case XK_KP_Right:
                case XK_Right:     sym = CHAR2SYM("right");     break;
                case XK_KP_Up:
                case XK_Up:        sym = CHAR2SYM("up");        break;
                case XK_KP_Down:
                case XK_Down:      sym = CHAR2SYM("down");      break;

                /* Input */
                case XK_KP_Enter:
                case XK_Return:    sym = CHAR2SYM("return");    break;
                case XK_Escape:    sym = CHAR2SYM("escape");    break;
                case XK_BackSpace: sym = CHAR2SYM("backspace"); break;
                case XK_Tab:       sym = CHAR2SYM("tab");       break;
                case XK_space:     sym = CHAR2SYM("space");     break;
                default:           sym = CHAR2SYM(buf);
              }

            /* Translate modifier keys */
            if(0 != ev.xkey.state)
              {
                ary = rb_ary_new();

                if(ev.xkey.state & ShiftMask)
                  rb_ary_push(ary, CHAR2SYM("shift"));
                if(ev.xkey.state & ControlMask)
                  rb_ary_push(ary, CHAR2SYM("control"));
                if(ev.xkey.state & Mod1Mask)
                  rb_ary_push(ary, CHAR2SYM("alt"));
                if(ev.xkey.state & Mod2Mask)
                  rb_ary_push(ary, CHAR2SYM("meta"));
                if(ev.xkey.state & Mod3Mask)
                  rb_ary_push(ary, CHAR2SYM("super"));
                if(ev.xkey.state & Mod4Mask)
                  rb_ary_push(ary, CHAR2SYM("altgr"));
              }

            /* Wrap up data */
            rargs[0] = w->keyboard;
            rargs[1] = rb_intern("call");
            rargs[2] = 2;
            rargs[3] = sym;
            rargs[4] = ary;

            /* Carefully call listen proc */
            result = rb_protect(WindowCall, (VALUE)&rargs, &state);
            if(state) subSubtlextBacktrace();

            /* End event loop? */
            if(Qtrue != result || state) loop = False;
            break; /* }}} */
          case ButtonPress: /* {{{ */
            /* Wrap up data */
            rargs[0] = w->pointer;
            rargs[1] = rb_intern("call");
            rargs[2] = 2;
            rargs[3] = INT2FIX(ev.xbutton.x);
            rargs[4] = INT2FIX(ev.xbutton.y);

            /* Carefully call listen proc */
            result = rb_protect(WindowCall, (VALUE)&rargs, &state);
            if(state) subSubtlextBacktrace();

            /* End event loop? */
            if(Qtrue != result || state) loop = False;
            break; /* }}} */
          default: break;
        }
    }

  /* Remove grabs */
  if(RTEST(w->keyboard))
    {
      XSelectInput(display, w->win, NoEventMask);
      XUngrabKeyboard(display, CurrentTime);
    }
  if(RTEST(w->pointer)) XUngrabPointer(display, CurrentTime);

  /* Restore logical focus */
  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      XSetInputFocus(display, *focus, RevertToPointerRoot, CurrentTime);

      free(focus);
    }

  return Qnil;
} /* }}} */

/* Singleton */

/* subWindowSingOnce {{{ */
/*
 * call-seq: once(geometry) -> Value
 *
 * Show window once as long as proc runs
 *
 *  Subtlext::Window.once(:x => 10, :y => 10, :widht => 100, :height => 100) do |w|
 *    "test"
 *  end
 *  => "test"
 **/

VALUE
subWindowSingOnce(VALUE self,
  VALUE geometry)
{
  VALUE win = Qnil, ret = Qnil;

  rb_need_block();

  /* Create new window */
  win = subWindowInstantiate(geometry);

  /* Yield block */
  ret = rb_yield_values(1, win);

  subWindowKill(win);

  return ret;
} /* }}} */

/* Helper */

/* subWindowInstantiate {{{ */
VALUE
subWindowInstantiate(VALUE geometry)
{
  VALUE klass = Qnil, win = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Window"));
  win   = rb_funcall(klass, rb_intern("new"), 1, geometry);

  return win;
} /* }}} */

/* Class */

/* subWindowAlloc {{{ */
/*
 * call-seq: new(geometry) -> Subtlext::Window
 *
 * Allocate space for new Window object.
 **/

VALUE
subWindowAlloc(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Create window */
  w = (SubtlextWindow *)subSharedMemoryAlloc(1, sizeof(SubtlextWindow));
  w->instance = Data_Wrap_Struct(self, WindowMark,
    WindowSweep, (void *)w);

  return w->instance;
} /* }}} */

/* subWindowInit {{{
 *
 * call-seq: new(geometry, &block) -> Subtlext::Window
 *
 * Initialize Window object.
 *
 *  win = Subtlext::Window.new(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowInit(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE geometry = Qnil;

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Check object type */
      switch(rb_type(value))
        {
          case T_HASH:
          case T_ARRAY:
              {
                XSetWindowAttributes sattrs;
                XRectangle r = { 0 };

                /* Create geometry */
                geometry = subGeometryInstantiate(0, 0, 1, 1);
                geometry = subGeometryInit(1, &value, geometry);

                subGeometryToRect(geometry, &r);

                /* Create window */
                sattrs.override_redirect = True;

                w->win = XCreateWindow(display, DefaultRootWindow(display),
                  r.x, r.y, r.width, r.height, 1, CopyFromParent,
                  CopyFromParent, CopyFromParent, CWOverrideRedirect, &sattrs);
              }
            break;
          case T_FIXNUM:
          case T_BIGNUM:
              {
                int x = 0, y = 0;
                unsigned int width = 0, height = 0, bw = 0, depth = 0;
                Window root = None;

                /* Update values */
                w->win    = FIX2LONG(value);
                w->flags |= WINDOW_FOREIGN_WIN;

                /* Get window geometry */
                if(XGetGeometry(display, w->win, &root,
                    &x, &y, &width, &height, &bw, &depth))
                  geometry = subGeometryInstantiate(x, y, width, height);
                else rb_raise(rb_eArgError, "Invalid window `%#lx'", w->win);
              }
            break;
          default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
            rb_obj_classname(value));
        }

      /* Store data */
      rb_iv_set(w->instance, "@win",      LONG2NUM(w->win));
      rb_iv_set(w->instance, "@geometry", geometry);
      rb_iv_set(w->instance, "@hidden",   Qtrue);

      /* Set window font */
      if(!w->font && !(w->font = subSharedFontNew(display, DEFFONT)))
        rb_raise(rb_eStandardError, "Invalid font `%s'", DEFFONT);

      /* Yield to block if given */
      if(rb_block_given_p())
        rb_yield_values(1, w->instance);

      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowSubwindow {{{
 *
 * call-seq: subwindow(geometry, &block) -> Subtlext::Window or nil
 *
 * Create a subwindow of Window with given Geometry.
 *
 *  win.subwindow(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowSubwindow(VALUE self,
  VALUE geometry)
{
  VALUE ret = Qnil;
  SubtlextWindow *w1 = NULL;

  Data_Get_Struct(self, SubtlextWindow, w1);
  if(w1)
    {
      SubtlextWindow *w2 = NULL;

      subSubtlextConnect(NULL); ///< Implicit open connection

      ret = subWindowInstantiate(geometry);

      Data_Get_Struct(ret, SubtlextWindow, w2);
      if(w2)
        {
          /* Yield to block if given */
          if(rb_block_given_p())
            rb_yield_values(1, w2->instance);

          XReparentWindow(display, w2->win, w1->win, 0, 0);
        }
    }

  return ret;
} /* }}} */

/* subWindowNameWriter {{{ */
/*
 * call-seq: name=(str) -> String
 *
 * Set the WM_NAME of a Window-
 *
 *  win.name = "sublet"
 *  => "sublet"
 */

VALUE
subWindowNameWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      Window win = None;
      XClassHint hint;
      XTextProperty text;
      char *name = NULL;

      /* Check object type */
      if(T_STRING == rb_type(value))
        {
          name = RSTRING_PTR(value);
          win  = NUM2LONG(rb_iv_get(self, "@win"));

          /* Set Window informations */
          hint.res_name  = name;
          hint.res_class = "Subtlext";

          XSetClassHint(display, win, &hint);
          XStringListToTextProperty(&name, 1, &text);
          XSetWMName(display, win, &text);

          free(text.value);
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return value;
} /* }}} */

/* subWindowFontWriter {{{ */
/*
 * call-seq: font=(string) -> String
 *
 * Set the font that is used for text inside of a W.indow
 *
 *  win.font = "-*-*-*-*-*-*-10-*-*-*-*-*-*-*"
 *  => "-*-*-*-*-*-*-10-*-*-*-*-*-*-*"
 */

VALUE
subWindowFontWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      /* Check object type */
      if(T_STRING == rb_type(value))
        {
          SubFont *f = NULL;
          char *font = RSTRING_PTR(value);

          /* Create window font */
          if((f = subSharedFontNew(display, font)))
            {
              /* Replace font */
              if(w->font) subSharedFontKill(display, w->font);

              w->font = f;
            }
          else rb_raise(rb_eStandardError, "Invalid font `%s'", font);
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return value;
} /* }}} */

/* subWindowFontYReader {{{ */
/*
 * call-seq: font_y -> Fixnum
 *
 * Get y offset of the selected Window font.
 *
 *  win.font_y
 *  => 10
 */

VALUE
subWindowFontYReader(VALUE self)
{
  VALUE ret = INT2FIX(0);
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && w->font) ret = INT2FIX(w->font->y);

  return ret;
} /* }}} */

/* subWindowFontHeightReader {{{ */
/*
 * call-seq: font_height -> Fixnum
 *
 * Get the height of selected Window font.
 *
 *  win.font_height
 *  => 10
 */

VALUE
subWindowFontHeightReader(VALUE self)
{
  VALUE ret = INT2FIX(0);
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && w->font) ret = INT2FIX(w->font->height);

  return ret;
} /* }}} */

/* subWindowFontWidth {{{ */
/*
 * call-seq: font_width(string) -> Fixnum
 *
 * Get width of string for selected Window font.
 *
 *  win.font_width("subtle")
 *  => 10
 */

VALUE
subWindowFontWidth(VALUE self,
  VALUE string)
{
  VALUE ret = INT2FIX(0);
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && w->font && T_STRING == rb_type(string))
    ret = INT2FIX(subSharedStringWidth(display, w->font,
      RSTRING_PTR(string), RSTRING_LEN(string), NULL, NULL, False));

  return ret;
} /* }}} */

/* subWindowForegroundWriter {{{ */
/*
 * call-seq: foreground=(string) -> String
 *           foreground=(array)  -> Array
 *           foreground=(hash)   -> Hash
 *           foreground=(fixnum) -> Fixnum
 *           foreground=(object) -> Subtlext::Color
 *
 * Set the foreground color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Object] Copy color from a Color object
 *
 *  win.foreground = "#000000"
 *  => "#000000"
 */

VALUE
subWindowForegroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) w->fg = subColorPixel(value, Qnil, Qnil, NULL);

  return value;
} /* }}} */

/* subWindowBackgroundWriter {{{ */
/*
 * call-seq: background=(string) -> String
 *           background=(array)  -> Array
 *           background=(hash)   -> Hash
 *           background=(fixnum) -> Fixnum
 *           background=(object) -> Subtlext::Color
 *
 * Set the background color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Object] Copy color from a Color object

 *  win.background = "#000000"
 *  => "#000000"
 */

VALUE
subWindowBackgroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      w->bg = subColorPixel(value, Qnil, Qnil, NULL);

      XSetWindowBackground(display, w->win, w->bg);
    }

  return value;
} /* }}} */

/* subWindowBorderColorWriter {{{ */
/*
 * call-seq: border=(string) -> String
 *           border=(array)  -> Array
 *           border=(hash)   -> Hash
 *           border=(fixnum) -> Fixnum
 *           border=(object) -> Subtlext::Color
 *
 * Set the border color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Object] Copy color from a Color object
 *
 *  win.border_color = "#000000"
 *  => "#000000"
 */

VALUE
subWindowBorderColorWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XSetWindowBorder(display, w->win,
        subColorPixel(value, Qnil, Qnil, NULL));
      XFlush(display);
    }

  return Qnil;
} /* }}} */

/* subWindowBorderSizeWriter {{{ */
/*
 * call-seq: border_size=(fixnum) -> Fixnum
 *
 * Set border size of this Window.
 *
 *  win.border_size = 3
 *  => 3
 */

VALUE
subWindowBorderSizeWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      int width = 3;

      /* Check object type */
      if(FIXNUM_P(value))
        {
          width = FIX2INT(value);

          XSetWindowBorderWidth(display, w->win, width);
          XFlush(display);
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return value;
} /* }}} */

/* subWindowGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get the Geometry of this Window.
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subWindowGeometryReader(VALUE self)
{
  VALUE geom = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@geometry", geom);

  return geom;
} /* }}} */

/* subWindowGeometryWriter {{{ */
/*
 * call-seq: geometry=(array)  -> Array
 *           geometry=(hash)   -> Hash
 *           geometry=(object) -> Subtlext::Geometry
 *
 * Set the geometry of this Window which can be of following
 * types:
 *
 * [Array]    Must be an array with values for x, y, width and height
 * [Hash]     Must be a hash with values for x, y, width and height
 * [Geometry] Copy geometry from a Geometry object
 *
 *  win.geometry = { :x => 0, :y => 0, :width => 50, :height => 50 }
 *  => { :x => 0, :y => 0, :width => 50, :height => 50 }
 */

VALUE
subWindowGeometryWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XRectangle r = { 0 };
      VALUE geom = Qnil;

      /* Create geometry */
      geom = subGeometryInstantiate(0, 0, 1, 1);
      geom = subGeometryInit(1, &value, geom);

      rb_iv_set(self, "@geometry", geom);
      subGeometryToRect(geom, &r);
      XMoveResizeWindow(display, w->win, r.x, r.y, r.width, r.height);
    }

  return value;
} /* }}} */

/* subWindowOn {{{ */
/*
 * call-seq: on(event, &block) -> Subtlext::Window
 *
 * Grab pointer button press events and pass them to the block until
 * the return value of the block isn't <b>true</b> or an error occured.
 *
 *  grab_mouse do |x, y, button|
 *    p "x=#{x}, y=#{y}, button=#{button}"
 *  end
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowOn(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE event = Qnil, value = Qnil;
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  rb_scan_args(argc, argv, "11", &event, &value);

  if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      /* Check value type */
      if(CHAR2SYM("draw") == event || CHAR2SYM("redraw") == event
          || CHAR2SYM("expose") == event)
        {
          w->expose = value;
        }
      else if(CHAR2SYM("key_down") == event)
        {
          w->keyboard = value;
        }
      else if(CHAR2SYM("mouse_down") == event)
        {
          w->pointer = value;
        }
      else rb_raise(rb_eArgError, "Unexpected value type for on");
    }

  return self;
} /* }}} */

/* subWindowDrawPoint {{{ */
/*
 * call-seq: draw_point(x, y, color) -> Subtlext::Window
 *
 * Draw a pixel on the window at given coordinates in given color.
 *
 *  win.draw_point(1, 1)
 *  => #<Subtlext::Window:xxx>
 *
 *  win.draw_point(1, 1, "#ff0000")
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowDrawPoint(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE x = Qnil, y = Qnil, color = Qnil;

  rb_scan_args(argc, argv, "21", &x, &y, &color);

  /* Check object types */
  if(FIXNUM_P(x) && FIXNUM_P(y))
    {
      SubtlextWindow *w = NULL;

      Data_Get_Struct(self, SubtlextWindow, w);
      if(w)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == w->gc)
            w->gc = XCreateGC(display, w->win, 0, NULL);

          /* Update GC */
          gvals.foreground = w->fg;
          gvals.background = w->bg;

          if(!NIL_P(color))
            gvals.foreground = subColorPixel(color, Qnil, Qnil, NULL);

          XChangeGC(display, w->gc, GCForeground|GCBackground, &gvals);

          XDrawPoint(display, w->win, w->gc, FIX2INT(x), FIX2INT(y));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subWindowDrawLine {{{ */
/*
 * call-seq: draw_line(x1, y1, x2, y2, color) -> Subtlext::Window
 *
 * Draw a line on the window starting at x1/y1 to x2/y2 in given color.
 *
 *  win.draw_line(1, 1, 10, 1)
 *  => #<Subtlext::Window:xxx>
 *
 *  win.draw_line(1, 1, 10, 1, "#ff0000", "#000000")
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowDrawLine(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE x1 = Qnil, x2 = Qnil, y1 = Qnil, y2 = Qnil, color = Qnil;

  rb_scan_args(argc, argv, "41", &x1, &y1, &x2, &y2, &color);

  /* Check object types */
  if(FIXNUM_P(x1) && FIXNUM_P(y1) &&
      FIXNUM_P(x2) && FIXNUM_P(x2))
    {
      SubtlextWindow *w = NULL;

      Data_Get_Struct(self, SubtlextWindow, w);
      if(w)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == w->gc)
            w->gc = XCreateGC(display, w->win, 0, NULL);

          /* Update GC */
          gvals.foreground = w->fg;
          gvals.background = w->bg;

          if(!NIL_P(color))
            gvals.foreground = subColorPixel(color, Qnil, Qnil, NULL);

          XChangeGC(display, w->gc, GCForeground|GCBackground, &gvals);

          XDrawLine(display, w->win, w->gc, FIX2INT(x1),
            FIX2INT(y1), FIX2INT(x2), FIX2INT(y2));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subWindowDrawRect {{{ */
/*
 * call-seq: draw_rect(x, y, width, height, color, fill) -> Subtlext::Window
 *
 * Draw a rect on the Window starting at x/y with given width, height
 * and colors.
 *
 *  win.draw_rect(1, 1, 10, 10)
 *  => #<Subtlext::Window:xxx>
 *
 *  win.draw_rect(1, 1, 10, 10, "#ff0000", true)
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowDrawRect(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil;
  VALUE color = Qnil, fill = Qnil;

  rb_scan_args(argc, argv, "42", &x, &y, &width, &height, &color, &fill);

  /* Check object types */
  if(FIXNUM_P(x) && FIXNUM_P(y) && FIXNUM_P(width) && FIXNUM_P(height))
    {
      SubtlextWindow *w = NULL;

      Data_Get_Struct(self, SubtlextWindow, w);
      if(w)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == w->gc)
            w->gc = XCreateGC(display, w->win, 0, NULL);

          /* Update GC */
          gvals.foreground = w->fg;
          gvals.background = w->bg;

          if(!NIL_P(color))
            gvals.foreground = subColorPixel(color, Qnil, Qnil, NULL);

          XChangeGC(display, w->gc, GCForeground|GCBackground, &gvals);

          /* Draw rect */
          if(Qtrue == fill)
            {
              XFillRectangle(display, w->win, w->gc, FIX2INT(x),
                FIX2INT(y), FIX2INT(width), FIX2INT(height));
            }
          else XDrawRectangle(display, w->win, w->gc, FIX2INT(x),
            FIX2INT(y), FIX2INT(width), FIX2INT(height));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subWindowDrawText {{{ */
/*
 * call-seq: draw_text(x, y, string, color) -> Subtlext::Window
 *
 * Draw a text on the Window starting at x/y with given width, height
 * and color <b>without</b> caching it.
 *
 *  win.draw_text(10, 10, "subtle")
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowDrawText(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextWindow *w = NULL;
  VALUE x = Qnil, y = Qnil, text = Qnil, color = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);

  rb_scan_args(argc, argv, "31", &x, &y, &text, &color);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && FIXNUM_P(x) && FIXNUM_P(y) && T_STRING == rb_type(text))
    {
      long lcolor = w->fg;

      /* Create on demand */
      if(0 == w->gc)
        w->gc = XCreateGC(display, w->win, 0, NULL);

      /* Parse colors */
      if(!NIL_P(color)) lcolor = subColorPixel(color, Qnil, Qnil, NULL);

      subSharedDrawString(display, w->gc, w->font, w->win, FIX2INT(x),
        FIX2INT(y), lcolor, w->bg, RSTRING_PTR(text), RSTRING_LEN(text));
    }

  return self;
} /* }}} */

/* subWindowDrawIcon {{{ */
/*
 * call-seq: draw_icon(x, y, icon, fg, bg) -> Subtlext::Window
 *
 * Draw a icon on the Window starting at x/y with given width, height
 * and color <b>without</b> caching it.
 *
 *  win.draw_icon(10, 10, Subtlext::Icon.new("foo.xbm"))
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowDrawIcon(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextWindow *w = NULL;
  VALUE x = Qnil, y = Qnil, icon = Qnil, fg = Qnil, bg = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);

  rb_scan_args(argc, argv, "32", &x, &y, &icon, &fg, &bg);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && FIXNUM_P(x) && FIXNUM_P(y) &&
      rb_obj_is_instance_of(icon, rb_const_get(mod, rb_intern("Icon"))))
    {
      int bitmap = False;
      long lfg = w->fg, lbg = w->bg;
      VALUE width = Qnil, height = Qnil, pixmap = Qnil;

      /* Create on demand */
      if(0 == w->gc)
        w->gc = XCreateGC(display, w->win, 0, NULL);

      /* Parse colors */
      if(!NIL_P(fg)) lfg = subColorPixel(fg, Qnil, Qnil, NULL);
      if(!NIL_P(bg)) lbg = subColorPixel(bg, Qnil, Qnil, NULL);

      /* Fetch icon values */
      width  = rb_iv_get(icon, "@width");
      height = rb_iv_get(icon, "@height");
      pixmap = rb_iv_get(icon, "@pixmap");
      bitmap = Qtrue == subIconAskBitmap(icon) ? True : False;

      subSharedDrawIcon(display, w->gc, w->win, FIX2INT(x),
        FIX2INT(y), FIX2INT(width), FIX2INT(height), lfg, lbg,
        NUM2LONG(pixmap), bitmap);
    }

  return self;
} /* }}} */

/* subWindowClear {{{ */
/*
 * call-seq: clear -> Subtlext::Window
 *
 * Clear this Window and remove all stored text.
 *
 *  win.clear
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowClear(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil;

      rb_scan_args(argc, argv, "04", &x, &y, &width, &height);

      /* Either clear area or whole window */
      if(FIXNUM_P(x) && FIXNUM_P(y) && FIXNUM_P(width) && FIXNUM_P(height))
        {
          XClearArea(display, w->win, FIX2INT(x), FIX2INT(y),
            FIX2INT(width), FIX2INT(height), False);
        }
      else XClearWindow(display, w->win);
    }

  return self;
} /* }}} */

/* subWindowRedraw {{{ */
/*
 * call-seq: redraw -> Subtlext::Window
 *
 * Redraw Window content.
 *
 *  win.redraw
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowRedraw(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) WindowExpose(w);

  return self;
} /* }}} */

/* subWindowRaise {{{ */
/*
 * call-seq: raise -> Subtlext::Window
 *
 * Raise this Window to the top of the window stack, when the window manager
 * supports that. (subtle does)
 *
 *  win.raise
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowRaise(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XRaiseWindow(display, w->win);
      WindowExpose(w);
    }

  return self;
} /* }}} */

/* subWindowLower {{{ */
/*
 * call-seq: lower -> Subtlext::Window
 *
 * Lower this Window to the bottom of the window stack, when the window manager
 * supports that. (subtle does)
 *
 *  win.lower
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowLower(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XLowerWindow(display, w->win);
      WindowExpose(w);
    }

  return self;
} /* }}} */

/* subWindowShow {{{ */
/*
 * call-seq: show() -> Subtlext::Window
 *
 * Show this Window on screen.
 *
 *  win.show
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowShow(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      rb_iv_set(self, "@hidden", Qfalse);

      if(RTEST(w->keyboard) || RTEST(w->pointer)) WindowGrab(w);
      else
        {
          XMapRaised(display, w->win);
          WindowExpose(w);
        }
    }

  return self;
} /* }}} */

/* subWindowHide {{{ */
/*
 * call-seq: hide() -> Subtlext::Window
 *
 * Hide this Window from screen.
 *
 *  win.hide
 *  => #<Subtlext::Window:xxx>
 */

VALUE
subWindowHide(VALUE self)
{
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  if(RTEST(win))
    {
      rb_iv_set(self, "@hidden", Qtrue);

      XUnmapWindow(display, NUM2LONG(win));
      XSync(display, False); ///< Sync with X
    }

  return self;
} /* }}} */

/* subWindowAskHidden {{{ */
/*
 * call-seq: hidden -> true or false
 *
 * Whether Window is hidden.
 *
 *  win.hidden?
 *  => true
 */

VALUE
subWindowAskHidden(VALUE self)
{
  VALUE hidden  = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@hidden", hidden);

  return hidden;
} /* }}} */

/* subWindowKill {{{ */
/*
 * call-seq: kill() -> nil
 *
 * Destroy this Window and <b>freeze</b> this object.
 *
 *  win.kill
 *  => nil
 */

VALUE
subWindowKill(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XUnmapWindow(display, w->win);

      rb_obj_freeze(self); ///< Freeze object
    }

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
