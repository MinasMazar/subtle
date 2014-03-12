 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/tag.c,v 3216 2012/06/15 17:18:12 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* TagFind {{{ */
static VALUE
TagFind(VALUE value,
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
          return subTagSingVisible(Qnil);
        else if(CHAR2SYM("all") == parsed)
          return subTagSingList(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Tag"))))
          return parsed;
    }

  return subSubtlextFindObjects("SUBTLE_TAG_LIST", "Tag", buf, flags, first);
} /* }}} */

/* Singleton */

/* subTagSingFind {{{ */
/*
 * call-seq: find(value) -> Array
 *           [value]     -> Array
 *
 * Find Tag by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_TAG_LIST</code> property list.
 * [String] Regexp match against name of Tags, returns a Tag on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Tags or any string for
 *          an <b>exact</b> match.
 *
 *  Subtlext::Tag.find(1)
 *  => [#<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag.find("subtle")
 *  => [#<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag[".*"]
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag["subtle"]
 *  => []
 *
 *  Subtlext::Tag[:terms]
 *  => [#<Subtlext::Tag:xxx>]
 */

VALUE
subTagSingFind(VALUE self,
  VALUE value)
{
  return TagFind(value, False);
} /* }}} */

/* subTagSingFirst {{{ */
/*
 * call-seq: first(value) -> Subtlext::Tag or nil
 *
 * Find first Tag by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_TAG_LIST</code> property list.
 * [String] Regexp match against name of Tags, returns a Tag on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Tags or any string for
 *          an <b>exact</b> match.
 *
 *  Subtlext::Tag.first(1)
 *  => #<Subtlext::Tag:xxx>
 *
 *  Subtlext::Tag.first("subtle")
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subTagSingFirst(VALUE self,
  VALUE value)
{
  return TagFind(value, True);
} /* }}} */

/* subTagSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get an array of all <i>visible</i> Tags on all Views.
 *
 *  Subtlext::Tag.visible
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag.visible
 *  => []
 */

VALUE
subTagSingVisible(VALUE self)
{
  int i, ntags = 0;
  char **tags = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, t = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Tag"));
  array   = rb_ary_new();
  tags    = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_TAGS", False), NULL);

  /* Populate array */
  if(tags && visible)
    {
      for(i = 0; i < ntags; i++)
        {
          /* Create tag on match */
          if(*visible & (1L << (i + 1)) &&
              !NIL_P(t = rb_funcall(klass, meth, 1, rb_str_new2(tags[i]))))
            {
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

    }

  if(tags)    XFreeStringList(tags);
  if(visible) free(visible);

  return array;
} /* }}} */

/* subTagSingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get an array of all Tags based on the <code>SUBTLE_TAG_LIST</code>
 * property list.
 *
 *  Subtlext::Tag.list
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag.list
 *  => []
 */

VALUE
subTagSingList(VALUE self)
{
  int i, ntags = 0;
  char **tags = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tag"));
  array = rb_ary_new();

  /* Check results */
  if((tags = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags)))
    {
      for(i = 0; i < ntags; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, rb_str_new2(tags[i]));

          rb_iv_set(t, "@id", INT2FIX(i));
          rb_ary_push(array, t);
        }

      XFreeStringList(tags);
    }

  return array;
} /* }}} */

/* Helper */

/* subTagInstantiate {{{ */
VALUE
subTagInstantiate(char *name)
{
  VALUE klass = Qnil, tag = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tag"));
  tag   = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return tag;
} /* }}} */

/* Class */

/* subTagInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tag
 *
 * Create new Tag object locally <b>without</b> calling #save automatically.
 *
 * The Tag <b>won't</b> be useable until #save is called.
 *
 *  tag = Subtlext::Tag.new("subtle")
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subTagInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(name));

  /* Init object */
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subTagSave {{{ */
/*
 * call-seq: save -> Subtlext::Tag
 *
 * Save new Tag object.
 *
 *  tag.update
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subTagSave(VALUE self)
{
  int id = -1;
  VALUE name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Create tag if needed */
  if(-1 == (id = subSubtlextFindString("SUBTLE_TAG_LIST",
      RSTRING_PTR(name), NULL, SUB_MATCH_EXACT)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_TAG_NEW", data, 8, True);

      id = subSubtlextFindString("SUBTLE_TAG_LIST",
        RSTRING_PTR(name), NULL, SUB_MATCH_EXACT);
    }

  /* Guess tag id */
  if(-1 == id)
    {
      int ntags = 0;
      char **tags = NULL;

      /* Get names of tags */
      if((tags = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
          XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags)))
        {

          id = ntags; ///< New id should be last

          XFreeStringList(tags);
        }
    }

  /* Set properties */
  rb_iv_set(self, "@id", INT2FIX(id));

  return self;
} /* }}} */

/* subTagClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get an rray of Clients that are tagged with this Tag.
 *
 *  tag.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  tag.clients
 *  => []
 */

VALUE
subTagClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  unsigned long *tags = NULL;
  VALUE id = Qnil, array = Qnil, klass = Qnil, meth = Qnil, c = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Fetch data */
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new();
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          if((tags = (unsigned long *)subSharedPropertyGet(display,
              clients[i], XA_CARDINAL, XInternAtom(display,
              "SUBTLE_CLIENT_TAGS", False), NULL)))
            {
              /* Check if tag id matches */
              if(*tags & (1L << (FIX2INT(id) + 1)))
                {
                  /* Create new client */
                  if(!NIL_P(c = rb_funcall(klass, meth, 1,
                      LONG2NUM(clients[i]))))
                    {
                      subClientUpdate(c);

                      rb_ary_push(array, c);
                    }
                }
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subTagViews {{{ */
/*
 * call-seq: views -> Array
 *
 * Get an rray of Views that are tagged with this Tag.
 *
 *  tag.views
 *  => [#<Subtlext::Views:xxx>, #<Subtlext::Views:xxx>]
 *
 *  tag.views
 *  => []
 */

VALUE
subTagViews(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  unsigned long *tags = NULL;
  VALUE id = Qnil, array = Qnil, klass = Qnil, meth = Qnil, v = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass  = rb_const_get(mod, rb_intern("View"));
  meth   = rb_intern("new");
  array  = rb_ary_new();
  names  = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  tags   = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(names && tags)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Check if tag id matches */
          if(tags[i] & (1L << (FIX2INT(id) + 1)))
            {
              /* Create new view */
              if(!NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
                {
                  rb_iv_set(v, "@id",  INT2FIX(i));
                  rb_ary_push(array, v);
                }
            }
        }
    }

  if(names) XFreeStringList(names);
  if(tags)  free(tags);

  return array;
} /* }}} */

/* subTagToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Tag object to string.
 *
 *  puts tag
 *  => "subtle"
 */

VALUE
subTagToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subTagKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Remove this Tag from subtle and <b>freeze</b> this object.
 *
 *  tag.kill
 *  => nil
 */

VALUE
subTagKill(VALUE self)
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
    "SUBTLE_TAG_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
