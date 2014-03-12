 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/tray.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* TrayFind {{{ */
static VALUE
TrayFind(VALUE value,
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
        if(CHAR2SYM("all") == parsed)
          return subTraySingList(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Tray"))))
          return parsed;
    }

  return subSubtlextFindWindows("SUBTLE_TRAY_LIST", "Tray",
    buf, flags, first);
} /* }}} */

/* Singleton */

/* subTraySingFind {{{ */
/*
 * call-seq: find(value) -> Array
 *           [value]     -> Array
 *
 * Find Tray by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_TRAY_LIST</code> property list.
 * [String] Regexp match against both <code>WM_CLASS</code> values.
 * [Hash]   Instead of just match <code>WM_CLASS</code> match against
 *          following properties:
 *
 *          [:name]     Match against <code>WM_NAME</code>
 *          [:instance] Match against first value of <code>WM_NAME</code>
 *          [:class]    Match against second value of <code>WM_NAME</code>
 * [Symbol] Either <i>:all</i> for an array of all Trays or any string for
 *          an <b>exact</b> match.

 *  Subtlext::Tray.find(1)
 *  => [#<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray.find("subtle")
 *  => [#<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray[".*"]
 *  => [#<Subtlext::Tray:xxx>, #<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray["subtle"]
 *  => []
 *
 *  Subtlext::Tray[:terms]
 *  => [#<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray[name: "subtle"]
 *  => [#<Subtlext::Tray:xxx>]
 */

VALUE
subTraySingFind(VALUE self,
  VALUE value)
{
  return TrayFind(value, False);
} /* }}} */

/* subTraySingFirst {{{ */
/*
 * call-seq: find(value) -> Subtlext::Tray or nil
 *
 * Find first Tray by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_TRAY_LIST</code> property list.
 * [String] Regexp match against both <code>WM_CLASS</code> values.
 * [Hash]   Instead of just match <code>WM_CLASS</code> match against
 *          following properties:
 *
 *          [:name]     Match against <code>WM_NAME</code>
 *          [:instance] Match against first value of <code>WM_NAME</code>
 *          [:class]    Match against second value of <code>WM_NAME</code>
 * [Symbol] Either <i>:all</i> for an array of all Trays or any string for
 *          an <b>exact</b> match.

 *  Subtlext::Tray.first(1)
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray.first("subtle")
 *  => #<Subtlext::Tray:xxx>
 */

VALUE
subTraySingFirst(VALUE self,
  VALUE value)
{
  return TrayFind(value, True);
} /* }}} */

/* subTraySingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get an array of all Trays based on <code>SUBTLE_TRAY_LIST</code>
 * property list.
 *
 *  Subtlext::Tray.list
 *  => [#<Subtlext::Tray:xxx>, #<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray.list
 *  => []
 */

VALUE
subTraySingList(VALUE self)
{
  int i, ntrays = 0;
  Window *trays = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tray"));
  array = rb_ary_new();

  /* Check results */
  if((trays = subSubtlextWindowList("SUBTLE_TRAY_LIST", &ntrays)))
    {
      for(i = 0; i < ntrays; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, LONG2NUM(trays[i]));

          if(!NIL_P(t)) subTrayUpdate(t);

          rb_ary_push(array, t);
        }

      free(trays);
    }

  return array;
} /* }}} */

/* Helper */

/* subTrayInstantiate {{{ */
VALUE
subTrayInstantiate(Window win)
{
  VALUE klass = Qnil, tray = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tray"));
  tray  = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return tray;
} /* }}} */

/* Class */

/* subTrayInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tray
 *
 * Create a new Tray object locally <b>without</b> calling #save automatically.
 *
 * The Tray <b>won't</b> be visible until #save is called.
 *
 *  tag = Subtlext::Tray.new("subtle")
 *  => #<Subtlext::Tray:xxx>
 */

VALUE
subTrayInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win) && T_BIGNUM != rb_type(win))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(win));

  /* Init object */
  rb_iv_set(self, "@win",   win);
  rb_iv_set(self, "@name",  Qnil);
  rb_iv_set(self, "@klass", Qnil);
  rb_iv_set(self, "@title", Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subTrayUpdate {{{ */
/*
 * call-seq: update -> Subtlext::Tray
 *
 * Update Tray properties based on <b>required</b> Tray window id.
 *
 *  tray.update
 *  => #<Subtlext::Tray:xxx>
 */

VALUE
subTrayUpdate(VALUE self)
{
  Window win = None;

  /* Check ruby object */
  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get tray values */
  win = NUM2LONG(rb_iv_get(self, "@win"));

  /* Check values */
  if(0 <= win)
    {
      char *wmname = NULL, *wminstance = NULL, *wmclass = NULL;

      /* Get name, instance and class */
      subSharedPropertyClass(display, win, &wminstance, &wmclass);
      subSharedPropertyName(display, win, &wmname, wmclass);

      /* Set properties */
      rb_iv_set(self, "@name",     rb_str_new2(wmname));
      rb_iv_set(self, "@instance", rb_str_new2(wminstance));
      rb_iv_set(self, "@klass",    rb_str_new2(wmclass));

      free(wmname);
      free(wminstance);
      free(wmclass);
    }
  else rb_raise(rb_eStandardError, "Invalid tray id `%#lx`", win);

  return self;
} /* }}} */

/* subTrayToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Tray object to string.
 *
 *  puts tray
 *  => "subtle"
 */

VALUE
subTrayToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subTrayKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Send a close signal to Client and <b>freeze</b> this object.
 *
 *  tray.kill
 *  => nil
 */

VALUE
subTrayKill(VALUE self)
{
  VALUE win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = CurrentTime;
  data.l[1] = 2; ///< Claim to be a pager

  subSharedMessage(display, NUM2LONG(win),
    "_NET_CLOSE_WINDOW", data, 32, True);

  rb_obj_freeze(self);

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
