 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/screen.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* ScreenList {{{ */
VALUE
ScreenList(void)
{
  unsigned long nworkareas = 0;
  VALUE method = Qnil, klass = Qnil, array = Qnil, screen = Qnil, geom = Qnil;
  long *workareas = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Screen"));
  array  = rb_ary_new();

  /* Get workarea list */
  if((workareas = (long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "_NET_WORKAREA", False), &nworkareas)))
    {
      int i;

      for(i = 0; i < nworkareas / 4; i++)
        {
          /* Create new screen */
          screen = rb_funcall(klass, method, 1, INT2FIX(i));
          geom   = subGeometryInstantiate(workareas[i * 4 + 0],
            workareas[i * 4 + 1], workareas[i * 4 + 2], workareas[i * 4 + 3]);

          rb_iv_set(screen, "@geometry", geom);
          rb_ary_push(array, screen);
        }

      free(workareas);
    }

  return array;
} /* }}} */

/* Singleton */

/* subScreenSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Screen or nil
 *           [value]     -> Subtlext::Screen or nil
 *
 * Find Screen by a given value which can be of following type:
 *
 * [fixnum]             Array id
 * [Subtlext::Geometry] Geometry
 *
 *  Subtlext::Screen.find(1)
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen[1]
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen.find(Subtlext::Geometry(10, 10, 100, 100)
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenSingFind(VALUE self,
  VALUE value)
{
  VALUE screen = Qnil;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM:
          {
            VALUE screens = ScreenList();

            screen = rb_ary_entry(screens, FIX2INT(value));
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Geometry"));

            /* Check object instance */
            if(rb_obj_is_instance_of(value, klass))
              {
                unsigned long nworkareas = 0;
                long *workareas = NULL;

                subSubtlextConnect(NULL); ///< Implicit open connection

                /* Get workarea list */
                if((workareas = (long *)subSharedPropertyGet(display,
                    DefaultRootWindow(display), XA_CARDINAL,
                    XInternAtom(display, "_NET_WORKAREA", False),
                    &nworkareas)))
                  {
                    int i;
                    XRectangle geom = { 0 };

                    subGeometryToRect(value, &geom);

                    for(i = 0; i < nworkareas / 4; i++)
                      {
                        /* Check if coordinates are in screen rects */
                        if(geom.x >= workareas[i * 4 + 0] && geom.x <
                            workareas[i * 4 + 0] + workareas[i * 4 + 2] &&
                            geom.y >= workareas[i * 4 + 1] && geom.y <
                            workareas[i * 4 + 1] + workareas[i * 4 + 3])
                          {
                            VALUE geometry = Qnil;

                            /* Create new screen */
                            screen   = subScreenInstantiate(i);
                            geometry = subGeometryInstantiate(
                              workareas[i * 4 + 0], workareas[i * 4 + 1],
                              workareas[i * 4 + 2], workareas[i * 4 + 3]);

                            rb_iv_set(screen, "@geometry", geometry);

                            break;
                          }
                      }

                    free(workareas);
                  }
              }
          }
        break;
      default: rb_raise(rb_eArgError, "Unexpected value type `%s'",
        rb_obj_classname(value));
    }

  return screen;
} /* }}} */

/* subScreenSingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get Array of all Screen
 *
 *  Subtlext::Screen.list
 *  => [#<Subtlext::Screen:xxx>, #<Subtlext::Screen:xxx>]
 *
 *  Subtlext::Screen.list
 *  => []
 */

VALUE
subScreenSingList(VALUE self)
{
  return ScreenList();
} /* }}} */

/* subScreenSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Screen
 *
 * Get current active Screen
 *
 *  Subtlext::Screen.current
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenSingCurrent(VALUE self)
{
  int rx = 0, ry = 0, x = 0, y = 0;
  unsigned int mask = 0;
  unsigned long nworkareas = 0, npanels = 0;
  long *workareas = NULL, *panels = NULL;
  VALUE screen = Qnil;
  Window root = None, win = None;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get current screen */
  XQueryPointer(display, DefaultRootWindow(display), &root,
    &win, &rx, &ry, &x, &y, &mask);

  /* Fetch data */
  workareas = (long *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_CARDINAL, XInternAtom(display, "_NET_WORKAREA", False), &nworkareas);
  panels    = (long *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_SCREEN_PANELS", False),
    &npanels);

  /* Get workarea list */
  if(workareas && panels)
    {
      int i;

      for(i = 0; i < nworkareas / 4; i++)
        {
          /* Check if coordinates are in screen rects including panel size */
          if(rx >= workareas[i * 4 + 0] &&
              rx < workareas[i * 4 + 0] + workareas[i * 4 + 2] &&
              ry >= (workareas[i * 4 + 1] - panels[i * 2 + 0]) &&
              ry < (workareas[i * 4 + 1] + workareas[i * 4 + 3] +
              panels[i * 2 + 1]))
            {
              VALUE geometry = Qnil;

              /* Create new screen */
              screen   = subScreenInstantiate(i);
              geometry = subGeometryInstantiate(workareas[i * 4 + 0],
                workareas[i * 4 + 1], workareas[i * 4 + 2],
                workareas[i * 4 + 3]);

              rb_iv_set(screen, "@geometry", geometry);
            }
        }
    }

  if(workareas) free(workareas);
  if(panels)    free(panels);

  return screen;
} /* }}} */

/* Helper */

/* subScreenInstantiate {{{ */
VALUE
subScreenInstantiate(int id)
{
  VALUE klass = Qnil, screen = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Screen"));
  screen = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

  return screen;
} /* }}} */

/* Class */

/* subScreenInit {{{ */
/*
 * call-seq: new(id) -> Subtlext::Screen
 *
 * Create a new Screen object
 *
 *  screen = Subtlext::Screen.new(0)
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenInit(VALUE self,
  VALUE id)
{
  if(!FIXNUM_P(id) || 0 > FIX2INT(id))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(id));

  /* Init object */
  rb_iv_set(self, "@id",       id);
  rb_iv_set(self, "@geometry", Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subScreenUpdate {{{ */
/*
 * call-seq: update -> Subtlext::Screen
 *
 * Update Screen properties
 *
 *  screen.update
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenUpdate(VALUE self)
{
  VALUE id = Qnil, screens = Qnil, screen = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@id", id);

  /* Find screen */
  if((screens = ScreenList()) &&
      RTEST(screen = rb_ary_entry(screens, FIX2INT(id))))
    {
      VALUE geometry = rb_iv_get(screen, "@geometry");

      rb_iv_set(self, "@geometry", geometry);
    }
  else rb_raise(rb_eStandardError, "Invalid screen id `%d'",
    (int)FIX2INT(id));

  return self;
} /* }}} */

/* subScreenJump {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Jump to this Screen
 *
 *  screen.jump
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenJump(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_SCREEN_JUMP", data, 32, True);

  return self;
} /* }}} */

/* subScreenViewReader {{{ */
/*
 * call-seq: view -> Subtlext::View
 *
 * Get active view for screen
 *
 *  screen.view
 *  => #<Subtlext::View:xxx>
 */

VALUE
subScreenViewReader(VALUE self)
{
  VALUE ret = Qnil;
  int nnames = 0;
  char **names = NULL;
  unsigned long *screens = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  names   = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  screens = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_SCREEN_VIEWS", False), NULL);

  /* Check results */
  if(names && screens)
    {
      int id = 0, vid = 0;

      if(0 <= (id = FIX2INT(rb_iv_get(self, "@id"))))
        {
          if(0 <= (vid = screens[id]) && vid < nnames)
            {
              ret = subViewInstantiate(names[vid]);

              if(!NIL_P(ret)) rb_iv_set(ret, "@id", INT2FIX(vid));
            }
        }
    }

  if(names)   XFreeStringList(names);
  if(screens) free(screens);

  return ret;
} /* }}} */

/* subScreenViewWriter {{{ */
/*
 * call-seq: view=(fixnum) -> Fixnum
 *           view=(symbol) -> Symbol
 *           view=(object) -> Subtlext::View
 *
 * Set active view for screen
 *
 *  screen.view = :www
 *  => nil
 *
 *  screen.view = Subtlext::View[0]
 *  => nil
 */

VALUE
subScreenViewWriter(VALUE self,
  VALUE value)
{
  VALUE vid = Qnil, view = Qnil, sid = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

 /* Check ruby object */
  GET_ATTR(self, "@id", sid);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check instance type */
  if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("View"))))
    view = value;
  else view = subViewSingFirst(Qnil, value);

  GET_ATTR(view, "@id", vid);

  /* Send message */
  data.l[0] = FIX2LONG(vid);
  data.l[1] = CurrentTime;
  data.l[2] = FIX2LONG(sid);

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_CURRENT_DESKTOP", data, 32, True);

  return value;
} /* }}} */

/* subScreenAskCurrent {{{ */
/*
 * call-seq: screen? -> true or false
 *
 * Check if this Screen is the current active Screen
 *
 *  screen.current?
 *  => true
 *
 *  screen.current?
 *  => false
 */

VALUE
subScreenAskCurrent(VALUE self)
{
  /* Check ruby object */
  rb_check_frozen(self);

  return rb_equal(self, subScreenSingCurrent(Qnil));
} /* }}} */

/* subScreenToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Screen object to String
 *
 *  puts screen
 *  => "0x0+800+600"
 */

VALUE
subScreenToString(VALUE self)
{
  VALUE geom = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@geometry", geom);

  return subGeometryToString(geom);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
