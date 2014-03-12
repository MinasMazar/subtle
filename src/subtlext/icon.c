 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/icon.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <unistd.h>
#include "subtlext.h"

#ifdef HAVE_WORDEXP_H
  #include <wordexp.h>
#endif /* HAVE_WORDEXP_H */

#ifdef HAVE_X11_XPM_H
  #include <X11/xpm.h>
#endif /* HAVE_X11_XPM_H */

/* Flags {{{ */
#define ICON_BITMAP  (1L << 0)
#define ICON_PIXMAP  (1L << 1)
#define ICON_FOREIGN (1L << 2)
/* }}} */

/* Typedef {{{ */
typedef struct subtlexticon_t
{
  GC           gc;
  Pixmap       pixmap;
  int          flags;
  unsigned int width, height;
  VALUE        instance;
} SubtlextIcon;
/* }}} */

/* IconMark {{{ */
static void
IconMark(SubtlextIcon *i)
{
  if(i) rb_gc_mark(i->instance);
} /* }}} */

/* IconSweep {{{ */
static void
IconSweep(SubtlextIcon *i)
{
  if(i)
    {
      /* Check if we can kill the pixmap here */
      if(!(i->flags & ICON_FOREIGN) && i->pixmap)
        XFreePixmap(display, i->pixmap);

      if(0 != i->gc) XFreeGC(display, i->gc);

      free(i);
    }
} /* }}} */

/* IconEqual {{{ */
VALUE
IconEqual(VALUE self,
  VALUE other)
{
  int ret = False;

  /* Check ruby object types */
  if(rb_obj_class(self) == rb_obj_class(other))
    {
      SubtlextIcon *i1 = NULL, *i2 = NULL;

      /* Get icons */
      Data_Get_Struct(self,  SubtlextIcon, i1);
      Data_Get_Struct(other, SubtlextIcon, i2);

      ret = (i1 && i2 && i1->width == i2->width && i1->height == i2->height);
    }

  return ret ? Qtrue : Qfalse;
} /* }}} */

/* Class */

/* subIconAlloc {{{ */
/*
 * call-seq: new(path)          -> Subtlext::Icon
 *           new(width, height) -> Subtlext::Icon
 *
 * Allocate space for a new Icon object.
 */

VALUE
subIconAlloc(VALUE self)
{
  SubtlextIcon *i = NULL;

  /* Create icon */
  i = (SubtlextIcon *)subSharedMemoryAlloc(1, sizeof(SubtlextIcon));
  i->instance = Data_Wrap_Struct(self, IconMark, IconSweep, (void *)i);

  return i->instance;
} /* }}} */

/* subIconInit {{{ */
/*
 * call-seq: initialize(path)                  -> Subtlext::Icon
 *           initialize(width, height, bitmap) -> Subtlext::Icon
 *
 * Initialize Icon object.
 *
 *  icon = Subtlext::Icon.new("/path/to/icon")
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon = Subtlext::Icon.new(8, 8)
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconInit(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      VALUE data[3] = { Qnil };

      rb_scan_args(argc, argv, "12", &data[0], &data[1], &data[2]);

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Find or create icon */
      if(T_STRING == rb_type(data[0])) ///< Icon path
        {
          int hotx = 0, hoty = 0;
          char buf[100] = { 0 };

#ifdef HAVE_WORDEXP_H
          /* Expand tildes in path */
          wordexp_t we;

          if(0 == wordexp(RSTRING_PTR(data[0]), &we, 0))
            {
              snprintf(buf, sizeof(buf), "%s", we.we_wordv[0]);

              wordfree(&we);
            }
          else
#endif /* HAVE_WORDEXP_H */
          snprintf(buf, sizeof(buf), "%s", RSTRING_PTR(data[0]));

          /* Find file */
          if(-1 == access(buf, R_OK))
            {
              char *home = NULL;

              /* Combine paths */
              if((home = getenv("XDG_DATA_HOME")))
                {
                  snprintf(buf, sizeof(buf), "%s/subtle/icons/%s",
                    home, RSTRING_PTR(data[0]));
                }
              else
                {
                  snprintf(buf, sizeof(buf), "%s/.local/share/subtle/icons/%s",
                   getenv("HOME"), RSTRING_PTR(data[0]));
                }

              /* Check if icon exists */
              if(-1 == access(buf, R_OK))
                rb_raise(rb_eStandardError, "Invalid icon `%s'",
                  RSTRING_PTR(data[0]));
            }

          /* Reading bitmap or pixmap icon file */
          if(BitmapSuccess != XReadBitmapFile(display,
              DefaultRootWindow(display), buf, &i->width, &i->height,
              &i->pixmap, &hotx, &hoty))
            {
#ifdef HAVE_X11_XPM_H
              XpmAttributes attrs;

              /* We could define a color to use on transparent areas, but
               * this can be done on init only, so we just ignore it and
               * expect pixmaps to have no transparent areas at all */

              /* Init attributes */
              attrs.colormap  = DefaultColormap(display, DefaultScreen(display));
              attrs.depth     = DefaultDepth(display, DefaultScreen(display));
              attrs.visual    = DefaultVisual(display, DefaultScreen(display));
              attrs.valuemask = XpmColormap|XpmDepth|XpmVisual;

              if(XpmSuccess == XpmReadFileToPixmap(display,
                  DefaultRootWindow(display), buf, &i->pixmap, NULL, &attrs))
                {
                  i->flags  |= ICON_PIXMAP;
                  i->width   = attrs.width;
                  i->height  = attrs.height;
                }
              else
#endif /* HAVE_X11_XPM_H */
                {
                  rb_raise(rb_eStandardError, "Invalid icon data");

                  return Qnil;
               }
            }
          else i->flags |= ICON_BITMAP;
        }
      else if(FIXNUM_P(data[0]) && FIXNUM_P(data[1])) ///< Icon dimensions
        {
          int depth = 1;

          /* Create pixmap or bitmap */
          if(Qtrue == data[2])
            {
              i->flags |= ICON_PIXMAP;
              depth     = XDefaultDepth(display, DefaultScreen(display));
            }
          else i->flags |= ICON_BITMAP;

          /* Create empty pixmap */
          i->width  = FIX2INT(data[0]);
          i->height = FIX2INT(data[1]);
          i->pixmap = XCreatePixmap(display, DefaultRootWindow(display),
            i->width, i->height, depth);
        }
      else if(FIXNUM_P(data[0]))
        {
          XRectangle geom = { 0 };

          i->flags  |= (ICON_BITMAP|ICON_FOREIGN);
          i->pixmap  = NUM2LONG(data[0]);

          subSharedPropertyGeometry(display, i->pixmap, &geom);

          i->width  = geom.width;
          i->height = geom.height;
        }
      else rb_raise(rb_eArgError, "Unexpected value-types");

      /* Update object */
      rb_iv_set(i->instance, "@width",  INT2FIX(i->width));
      rb_iv_set(i->instance, "@height", INT2FIX(i->height));
      rb_iv_set(i->instance, "@pixmap", LONG2NUM(i->pixmap));

      XSync(display, False); ///< Sync all changes
    }

  return Qnil;
} /* }}} */

/* subIconDrawPoint {{{ */
/*
 * call-seq: draw_point(x, y, fg, bg) -> Subtlext::Icon
 *
 * Draw a pixel on the icon at given coordinates in given colors.
 *
 *  icon.draw_point(1, 1)
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon.draw_point(1, 1, "#ff0000", "#000000")
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconDrawPoint(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[4] = { Qnil };

  rb_scan_args(argc, argv, "22", &data[0], &data[1], &data[2], &data[3]);

  /* Check object types */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]))
    {
      SubtlextIcon *i = NULL;

      Data_Get_Struct(self, SubtlextIcon, i);
      if(i)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == i->gc)
            i->gc = XCreateGC(display, i->pixmap, 0, NULL);

          /* Update GC */
          gvals.foreground = 1;
          gvals.background = 0;

          if(i->flags & ICON_PIXMAP)
            {
              if(!NIL_P(data[2]))
                gvals.foreground = subColorPixel(data[2], Qnil, Qnil, NULL);
              if(!NIL_P(data[3]))
                gvals.background = subColorPixel(data[3], Qnil, Qnil, NULL);
            }

          XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

          XDrawPoint(display, i->pixmap, i->gc,
            FIX2INT(data[0]), FIX2INT(data[1]));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subIconDrawLine {{{ */
/*
 * call-seq: draw_line(x1, y1, x2, y2, fg, bg) -> Subtlext::Icon
 *
 * Draw a line on the Icon starting at x1/y1 to x2/y2 in given colors.
 *
 *  icon.draw_line(1, 1, 10, 1)
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon.draw_line(1, 1, 10, 1, "#ff0000", "#000000")
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconDrawLine(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[6] = { Qnil };

  rb_scan_args(argc, argv, "42", &data[0], &data[1], &data[2], &data[3],
    &data[4], &data[5]);

  /* Check object types */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) &&
      FIXNUM_P(data[2]) && FIXNUM_P(data[3]))
    {
      SubtlextIcon *i = NULL;

      Data_Get_Struct(self, SubtlextIcon, i);
      if(i)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == i->gc)
            i->gc = XCreateGC(display, i->pixmap, 0, NULL);

          /* Update GC */
          gvals.foreground = 1;
          gvals.background = 0;

          if(i->flags & ICON_PIXMAP)
            {
              if(!NIL_P(data[4]))
                gvals.foreground = subColorPixel(data[4], Qnil, Qnil, NULL);
              if(!NIL_P(data[5]))
                gvals.background = subColorPixel(data[5], Qnil, Qnil, NULL);
            }

          XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

          XDrawLine(display, i->pixmap, i->gc, FIX2INT(data[0]),
            FIX2INT(data[1]), FIX2INT(data[2]), FIX2INT(data[3]));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subIconDrawRect {{{ */
/*
 * call-seq: draw_rect(x, y, width, height, fill, fg, bg) -> Subtlext::Icon
 *
 * Draw a rect on the Icon starting at x/y with given width, height
 * and colors.
 *
 *  icon.draw_rect(1, 1, 10, 10, false)
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon.draw_rect(1, 1, 10, 10, false, "#ff0000", "#000000")
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconDrawRect(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[7] = { Qnil };

  rb_scan_args(argc, argv, "43", &data[0], &data[1], &data[2], &data[3],
    &data[4], &data[5], &data[6]);

  /* Check object types */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) &&
      FIXNUM_P(data[2]) && FIXNUM_P(data[3]))
    {
      SubtlextIcon *i = NULL;

      Data_Get_Struct(self, SubtlextIcon, i);
      if(i)
        {
          XGCValues gvals;

          /* Create on demand */
          if(0 == i->gc)
            i->gc = XCreateGC(display, i->pixmap, 0, NULL);

          /* Update GC */
          gvals.foreground = 1;
          gvals.background = 0;

          if(i->flags & ICON_PIXMAP)
            {
              if(!NIL_P(data[5]))
                gvals.foreground = subColorPixel(data[5], Qnil, Qnil, NULL);
              if(!NIL_P(data[6]))
                gvals.background = subColorPixel(data[6], Qnil, Qnil, NULL);
            }

          XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

          /* Draw rect */
          if(Qtrue == data[4])
            {
              XFillRectangle(display, i->pixmap, i->gc, FIX2INT(data[0]),
                FIX2INT(data[1]), FIX2INT(data[2]), FIX2INT(data[3]));
            }
          else XDrawRectangle(display, i->pixmap, i->gc, FIX2INT(data[0]),
            FIX2INT(data[1]), FIX2INT(data[2]), FIX2INT(data[3]));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subIconCopyArea {{{ */
/*
 * call-seq: copy_area(icon2, src_x, src_y, width, height, dest_x, dest_y) -> Subtlext::Icon
 *
 * Copy area of given width/height from one Icon to another.
 *
 *  icon.copy_area(icon, 0, 0, 10, 10, 0, 0)
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconCopyArea(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[7] = { Qnil };

  rb_scan_args(argc, argv, "16", &data[0], &data[1], &data[2], &data[3],
    &data[4], &data[5], &data[6]);

  /* Check object type */
  if(rb_obj_is_instance_of(data[0],
      rb_const_get(mod, rb_intern("Icon"))))
    {
      SubtlextIcon *src = NULL, *dest = NULL;

      Data_Get_Struct(data[0], SubtlextIcon, src);
      Data_Get_Struct(self,    SubtlextIcon, dest);

      if(src && dest)
        {
          int src_x = 0, src_y = 0, dest_x = 0, dest_y = 0;
          int iwidth = 0, iheight = 0, area_w = 0, area_h = 0;
          VALUE width = Qnil, height = Qnil;

          /* Get icon dimesions */
          GET_ATTR(self, "@width",  width);
          GET_ATTR(self, "@height", height);

          iwidth  = FIX2INT(width);
          iheight = FIX2INT(height);

          /* Check args */
          if(!NIL_P(data[1])) src_x  = FIX2INT(data[1]);
          if(!NIL_P(data[2])) src_y  = FIX2INT(data[2]);
          if(!NIL_P(data[3])) area_w = FIX2INT(data[3]);
          if(!NIL_P(data[4])) area_h = FIX2INT(data[4]);
          if(!NIL_P(data[5])) dest_x = FIX2INT(data[5]);
          if(!NIL_P(data[6])) dest_y = FIX2INT(data[6]);

          /* Sanitize args */
          if(0 == area_w) area_w = iwidth;
          if(0 == area_h) area_h = iheight;

          if(area_w > dest_x + iwidth)  area_w = iwidth  - dest_x;
          if(area_h > dest_y + iheight) area_h = iheight - dest_y;

          if(0 > src_x  || src_x  > iwidth)  src_x  = 0;
          if(0 > src_y  || src_y  > iheight) src_y  = 0;
          if(0 > dest_x || dest_x > iwidth)  dest_x = 0;
          if(0 > dest_y || dest_y > iheight) dest_y = 0;

          /* Create on demand */
          if(0 == dest->gc)
            dest->gc = XCreateGC(display, dest->pixmap, 0, NULL);

          /* Copy area */
          if(src->flags & ICON_PIXMAP && dest->flags & ICON_PIXMAP)
            {
              XCopyPlane(display, src->pixmap, dest->pixmap, dest->gc,
                src_x, src_y, area_w, area_h, dest_x, dest_y, 1);
            }
          else XCopyArea(display, src->pixmap, dest->pixmap, dest->gc,
            src_x, src_y, area_w, area_h, dest_x, dest_y);

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return self;
} /* }}} */

/* subIconClear {{{ */
/*
 * call-seq: clear         -> Subtlext::Icon
 *           clear(fg, bg) -> Subtlext::Icon
 *
 * Clear the icon and optionally set a fore-/background color.
 *
 *  icon.clear
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon.clear("#ff0000", "#000000")
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconClear(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      XGCValues gvals;

      if(0 == i->gc) ///< Create on demand
        i->gc = XCreateGC(display, i->pixmap, 0, NULL);

      /* Update GC */
      gvals.foreground = 0;
      gvals.background = 1;

      if(i->flags & ICON_PIXMAP)
        {
          VALUE colors[2] = { Qnil };

          rb_scan_args(argc, argv, "02", &colors[0], &colors[1]);

          if(!NIL_P(colors[0]))
            gvals.foreground = subColorPixel(colors[0], Qnil, Qnil, NULL);
          if(!NIL_P(colors[1]))
            gvals.background = subColorPixel(colors[1], Qnil, Qnil, NULL);
        }

      XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

      XFillRectangle(display, i->pixmap, i->gc, 0, 0, i->width, i->height);

      XFlush(display);
    }

  return self;
} /* }}} */

/* subIconAskBitmap {{{ */
/*
 * call-seq: bitmap? -> true or false
 *
 * Whether icon is a bitmap.
 *
 *  icon.bitmap?
 *  => true
 *
 *  icon.bitmap?
 *  => false
 */

VALUE
subIconAskBitmap(VALUE self)
{
  VALUE ret = Qfalse;
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i) ret = (i->flags & ICON_BITMAP) ? Qtrue : Qfalse;

  return ret;
} /* }}} */

/* subIconToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Icon object to string.
 *
 *  puts icon
 *  => "<>!4<>"
 */

VALUE
subIconToString(VALUE self)
{
  VALUE ret = Qnil;
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%s%c%ld%s", SEPARATOR,
        i->flags & ICON_PIXMAP ? '&' : '!', i->pixmap, SEPARATOR);
      ret = rb_str_new2(buf);
    }

  return ret;
} /* }}} */

/* subIconOperatorPlus {{{ */
/*
* call-seq: +(string) -> String
*
* Convert this Icon to string and concat given string.
*
*  icon + "subtle"
*  => "<>!4<>subtle"
*/

VALUE
subIconOperatorPlus(VALUE self,
  VALUE value)
{
  return subSubtlextConcat(subIconToString(self), value);
} /* }}} */

/* subIconOperatorMult {{{ */
/*
* call-seq: *(value) -> String
*
* Convert this Icon to string and concat it multiple times.
*
*  icon * 2
*  => "<>!4<><>!4<>"
*/

VALUE
subIconOperatorMult(VALUE self,
  VALUE value)
{
  VALUE ret = Qnil;

  /* Check id and name */
  if(FIXNUM_P(value))
    {
      /* Passthru to string class */
      ret = rb_funcall(subIconToString(self), rb_intern("*"), 1, value);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return ret;
} /* }}} */

/* subIconEqual {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects have the same values (based on geometry).
 *
 *  object1 == object2
 *  => true
 */

VALUE
subIconEqual(VALUE self,
  VALUE other)
{
  return IconEqual(self, other);
} /* }}} */

/* subIconEqualTyped {{{ */
/*
 * call-seq: eql?(other) -> True or False
 *
 * Whether both objects have the same values and types (based on geometry).
 *
 *  object1.eql? object2
 *  => true
 */

VALUE
subIconEqualTyped(VALUE self,
  VALUE other)
{
  return IconEqual(self, other);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
