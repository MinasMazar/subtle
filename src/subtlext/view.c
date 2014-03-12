 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/view.c,v 3216 2012/06/15 17:18:12 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* ViewSelect {{{ */
static VALUE
ViewSelect(VALUE self,
  int dir)
{
  int nnames = 0;
  char **names = NULL;
  VALUE id = Qnil, ret = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  if((names = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames)))
    {
      int vid = FIX2INT(id);

      /* Select view */
      if(SUB_VIEW_NEXT == dir && vid < (nnames - 1))
        {
          vid++;
        }
      else if(SUB_VIEW_PREV == dir && 0 < vid)
        {
          vid--;
        }

      /* Create view */
      ret = subViewInstantiate(names[vid]);
      subViewUpdate(ret);

      XFreeStringList(names);
    }

  return ret;
} /* }}} */

/* ViewFind {{{ */
static VALUE
ViewFind(VALUE value,
  int first)
{
  int flags = 0;
  VALUE parsed = Qnil;
  char buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      case T_SYMBOL:
        if(CHAR2SYM("visible") == parsed)
          return subViewSingVisible(Qnil);
        else if(CHAR2SYM("all") == parsed)
          return subViewSingList(Qnil);
        else if(CHAR2SYM("current") == parsed)
          return subViewSingCurrent(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("View"))))
          return parsed;
    }

  return subSubtlextFindObjects("_NET_DESKTOP_NAMES", "View",
    buf, flags, first);
} /* }}} */

/* Singleton */

/* subViewSingFind {{{ */
/*
 * call-seq: find(value) -> Array
 *           [value]     -> Array
 *
 * Find View by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>_NET_DESKTOP_NAMES</code> property list.
 * [String] Regexp match against name of Views, returns a View on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:current</i> for current View, <i>:all</i> for an
 *          array of all Views or any string for an <b>exact</b> match.
 *
 *  Subtlext::View.find(1)
 *  => [#<Subtlext::View:xxx>]
 *
 *  Subtlext::View.find("subtle")
 *  => [#<Subtlext::View:xxx>]
 *
 *  Subtlext::View[".*"]
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  Subtlext::View["subtle"]
 *  => []
 *
 *  Subtlext::View[:terms]
 *  => #<Subtlext::View:xxx>]
 */

VALUE
subViewSingFind(VALUE self,
  VALUE value)
{
  return ViewFind(value, False);
} /* }}} */

/* subViewSingFirst {{{ */
/*
 * call-seq: first(value) -> Subtlext::View or nil
 *
 * Find first View by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>_NET_DESKTOP_NAMES</code> property list.
 * [String] Regexp match against name of Views, returns a View on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:current</i> for current View, <i>:all</i> for an
 *          array of all Views or any string for an <b>exact</b> match.
 *
 *  Subtlext::View.first(1)
 *  => #<Subtlext::View:xxx>
 *
 *  Subtlext::View.first("subtle")
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSingFirst(VALUE self,
  VALUE value)
{
  return ViewFind(value, True);
} /* }}} */

/* subViewSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::View
 *
 * Get currently active View.
 *
 *  Subtlext::View.current
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSingCurrent(VALUE self)
{
  int nnames = 0;
  char **names = NULL;
  unsigned long *cur_view = NULL;
  VALUE view = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  names    = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  cur_view = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

  /* Check results */
  if(names && cur_view)
    {
      /* Create instance */
      view = subViewInstantiate(names[*cur_view]);
      rb_iv_set(view, "@id",  INT2FIX(*cur_view));
    }

  if(names)    XFreeStringList(names);
  if(cur_view) free(cur_view);

  return view;
} /* }}} */

/* subViewSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get an array of all <i>visible</i> Views on connected Screens.
 *
 *  Subtlext::View.visible
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  Subtlext::View.visible
 *  => []
 */

VALUE
subViewSingVisible(VALUE self)
{
  int i, nnames = 0, *tags = NULL;
  char **names = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, v = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("View"));
  array = rb_ary_new();
  names = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_VIEWS", False), NULL);
  tags  = (int *)subSharedPropertyGet(display, ROOT, XA_CARDINAL,
      XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(names && visible && tags)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Create view on match */
          if(*visible & (1L << (i + 1)) &&
              !NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
            {
              rb_iv_set(v, "@id",   INT2FIX(i));
              rb_iv_set(v, "@tags", INT2FIX(tags[i]));

              rb_ary_push(array, v);
            }
        }
    }

  if(names)   XFreeStringList(names);
  if(visible) free(visible);
  if(tags)    free(tags);

  return array;
} /* }}} */

/* subViewSingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get an array of all Views based on the <code>_NET_DESKTOP_NAMES</code>
 * property list.
 *
 *  Subtlext::View.list
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  Subtlext::View.list
 *  => []
 */

VALUE
subViewSingList(VALUE self)
{
  int i, nnames = 0;
  long *tags = NULL;
  char **names = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, v = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass = rb_const_get(mod, rb_intern("View"));
  meth  = rb_intern("new");
  array = rb_ary_new();
  names = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  tags  = (long *)subSharedPropertyGet(display, ROOT, XA_CARDINAL,
      XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(names && tags)
    {
      for(i = 0; i < nnames; i++)
        {
          if(!NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
            {
              rb_iv_set(v, "@id",   INT2FIX(i));
              rb_iv_set(v, "@tags", LONG2NUM(tags[i]));

              rb_ary_push(array, v);
            }
        }
    }

  if(names) XFreeStringList(names);
  if(tags)  free(tags);

  return array;
} /* }}} */

/* Helper */

/* subViewInstantiate {{{ */
VALUE
subViewInstantiate(char *name)
{
  VALUE klass = Qnil, view = Qnil;

  assert(name);

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("View"));
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return view;
} /* }}} */

/* Class */

/* subViewInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::View
 *
 * Create a new View object locally <b>without</b> calling #save automatically.
 *
 * The View <b>won't</b> be visible until #save is called.
 *
 *  view = Subtlext::View.new("subtle")
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(name));

  /* Init object */
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);
  rb_iv_set(self, "@tags", Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subViewUpdate {{{ */
/*
 * call-seq: update -> Subtlext::View
 *
 * Update View properties based on <b>required</b> View index.
 *
 *  view.update
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewUpdate(VALUE self)
{
  long *tags = NULL, ntags = 0;
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch tags */
  if((tags = (long *)subSharedPropertyGet(display, ROOT, XA_CARDINAL,
      XInternAtom(display, "SUBTLE_VIEW_TAGS", False), (unsigned long *)&ntags)))
    {
      int idx = FIX2INT(id);

      rb_iv_set(self, "@tags", LONG2NUM(idx < ntags ? tags[idx] : 0));

      free(tags);
    }

  return self;
} /* }}} */

/* subViewSave {{{ */
/*
 * call-seq: save -> Subtlext::View
 *
 * Save new View object.
 *
 *  view.save
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSave(VALUE self)
{
  int id = -1;
  VALUE name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Create view if needed */
  if(-1 == (id = subSubtlextFindString("_NET_DESKTOP_NAMES",
      RSTRING_PTR(name), NULL, SUB_MATCH_EXACT)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_VIEW_NEW", data, 8, True);

      id = subSubtlextFindString("_NET_DESKTOP_NAMES",
        RSTRING_PTR(name), NULL, SUB_MATCH_EXACT);
    }

  /* Guess view id */
  if(-1 == id)
    {
      int nnames = 0;
      char **names = NULL;

      /* Get names of views */
      if((names = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
          XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames)))
        {
          id = nnames; ///< New id should be last

          if(names) XFreeStringList(names);
        }
    }

  /* Set properties */
  rb_iv_set(self, "@id", INT2FIX(id));

  return self;
} /* }}} */

/* subViewClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get an array of visible Clients on this View.
 *
 *  view.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  view.clients
 *  => []
 */

VALUE
subViewClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE id = Qnil, klass = Qnil, meth = Qnil, array = Qnil, client = Qnil;
  unsigned long *view_tags = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass     = rb_const_get(mod, rb_intern("Client"));
  meth      = rb_intern("new");
  array     = rb_ary_new();
  clients   = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);
  view_tags = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(clients && view_tags)
    {
      for(i = 0; i < nclients; i++)
        {
          unsigned long *client_tags = NULL, *flags = NULL;

          /* Fetch window data */
          client_tags = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_CLIENT_TAGS", False), NULL);
          flags       = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_CLIENT_FLAGS", False), NULL);

          /* Check if there are common tags or window is stick */
          if((client_tags && view_tags[FIX2INT(id)] & *client_tags) ||
              (flags && *flags & SUB_EWMH_STICK))
            {
              if(RTEST(client = rb_funcall(klass, meth,
                  1, LONG2NUM(clients[i]))))
                {
                  subClientUpdate(client);

                  rb_ary_push(array, client);
                }
            }

          if(client_tags) free(client_tags);
          if(flags)       free(flags);
        }
    }

  if(clients)   free(clients);
  if(view_tags) free(view_tags);

  return array;
} /* }}} */

/* subViewJump {{{ */
/*
 * call-seq: jump -> Subtlext::View
 *
 * Set this View to the current active one
 *
 *  view.jump
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewJump(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = FIX2INT(id);
  data.l[2] = -1;

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_CURRENT_DESKTOP", data, 32, True);

  return self;
} /* }}} */

/* subViewSelectNext {{{ */
/*
 * call-seq: next -> Subtlext::View or nil
 *
 * Select next View, but <b>doesn't</b> cycle list.
 *
 *  view.next
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSelectNext(VALUE self)
{
  return ViewSelect(self, SUB_VIEW_NEXT);
} /* }}} */

/* subViewSelectPrev {{{ */
/*
 * call-seq: prev -> Subtlext::View or nil
 *
 * Select prev View, but <b>doesn't</b> cycle list.
 *
 *  view.prev
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSelectPrev(VALUE self)
{
  return ViewSelect(self, SUB_VIEW_PREV);
} /* }}} */

/* subViewAskCurrent {{{ */
/*
 * call-seq: current? -> true or false
 *
 * Check if this View is the current active View.
 *
 *  view.current?
 *  => true
 *
 *  view.current?
 *  => false
 */

VALUE
subViewAskCurrent(VALUE self)
{
  VALUE id = Qnil, ret = Qfalse;;
  unsigned long *cur_view = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check results */
  if((cur_view = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL)))
    {
      if(FIX2INT(id) == *cur_view) ret = Qtrue;

      free(cur_view);
    }

  return ret;
} /* }}} */

/* subViewIcon {{{ */
/*
 * call-seq: icon -> Subtlext::Icon or nil
 *
 * Get the Icon of the View.
 *
 *  view.icon
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subViewIcon(VALUE self)
{
  unsigned long nicons = 0;
  VALUE id = Qnil, ret = Qnil;
  unsigned long *icons = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check results */
  if((icons = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "SUBTLE_VIEW_ICONS", False), &nicons)))
    {
      int iid = FIX2INT(id);

      /* Check if id is in range and icon available */
      if(0 <= iid && iid < nicons && -1 != icons[iid])
        {
          /* Create new icon */
          ret = rb_funcall(rb_const_get(mod, rb_intern("Icon")),
            rb_intern("new"), 1, LONG2NUM(icons[iid]));
        }

      free(icons);
    }

  return ret;
} /* }}} */

/* subViewToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this View object to string.
 *
 *  puts view
 *  => "subtle"
 */

VALUE
subViewToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subViewKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Remove this View from subtle and <b>freeze</b> this object.
 *
 *  view.kill
 *  => nil
 */

VALUE
subViewKill(VALUE self)
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
    "SUBTLE_VIEW_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
