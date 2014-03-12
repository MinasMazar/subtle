 /**
  * @package subtlext
  *
  * @file Client functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/client.c,v 3216 2012/06/15 17:18:12 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* ClientRestack {{{ */
VALUE
ClientRestack(VALUE self,
  int detail)
{
  VALUE win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = 2; ///< Claim to be a pager
  data.l[1] = NUM2LONG(win);
  data.l[2] = detail;

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_RESTACK_WINDOW", data, 32, True);

  return self;
} /* }}} */

/* ClientFlagsGet {{{ */
static VALUE
ClientFlagsGet(VALUE self,
  int flag)
{
  VALUE flags = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@flags", flags)

  flags = rb_iv_get(self, "@flags");

  return (FIXNUM_P(flags) && FIX2INT(flags) & flag) ? Qtrue : Qfalse;
} /* }}} */

/* ClientFlagsSet {{{ */
static VALUE
ClientFlagsSet(VALUE self,
  int flag,
  int toggle)
{
  int iflags = flag;
  VALUE win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Toggle flags */
  if(toggle)
    {
      VALUE flags = Qnil;

      GET_ATTR(self, "@flags", flags);

      iflags = FIX2INT(flags);
      if(iflags & flag) iflags &= ~flag;
      else iflags |= flag;
    }

  /* Assemble message */
  data.l[0] = NUM2LONG(win);
  data.l[1] = iflags;

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_CLIENT_FLAGS", data, 32, True);

  rb_iv_set(self, "@flags", INT2FIX(iflags));

  return self;
} /* }}} */

/* ClientFlagsToggle {{{ */
static VALUE
ClientFlagsToggle(VALUE self,
  char *type,
  int flag)
{
  int iflags = 0;
  VALUE win = Qnil, flags = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win",   win);
  GET_ATTR(self, "@flags", flags);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Update flags */
  iflags = FIX2INT(flags);
  if(iflags & flag) iflags &= ~flag;
  else iflags |= flag;

  rb_iv_set(self, "@flags", INT2FIX(iflags));

  /* Send message */
  data.l[0] = XInternAtom(display, "_NET_WM_STATE_TOGGLE", False);
  data.l[1] = XInternAtom(display, type, False);

  subSharedMessage(display, NUM2LONG(win), "_NET_WM_STATE", data, 32, True);

  return self;
} /* }}} */

/* ClientGravity {{{ */
static int
ClientGravity(VALUE key,
  VALUE value,
  VALUE client)
{
  SubMessageData data = { .l = { -1, -1, -1, -1, -1 } };

  data.l[0] = NUM2LONG(rb_iv_get(client, "@win"));

  /* Find gravity */
  if(RTEST(value))
    {
      VALUE gravity = Qnil;

      if(RTEST((gravity = subGravitySingFirst(Qnil, value))))
        data.l[1] = FIX2INT(rb_iv_get(gravity, "@id"));
    }

  /* Find view id if any */
  if(RTEST(key))
    {
      VALUE view = Qnil;

      if(RTEST((view = subViewSingFirst(Qnil, key))))
        data.l[2] = FIX2INT(rb_iv_get(view, "@id"));
    }

  /* Finally send message */
  if(-1 != data.l[0] && -1 != data.l[1])
    {
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_CLIENT_GRAVITY", data, 32, True);
    }

  return ST_CONTINUE;
} /* }}} */

/* ClientFind {{{ */
static VALUE
ClientFind(VALUE value,
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
          return subClientSingVisible(Qnil);
        else if(CHAR2SYM("all") == parsed)
          return subClientSingList(Qnil);
        else if(CHAR2SYM("current") == parsed)
          return subClientSingCurrent(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Client"))))
          return parsed;
    }

  return subSubtlextFindWindows("_NET_CLIENT_LIST", "Client",
    buf, flags, first);
} /* }}} */

/* Singleton */

/* subClientSingSelect {{{ */
/*
 * call-seq: select -> Subtlext::Client or nil
 *
 * Click on a wondow and get the selected Client.
 *
 *  Subtlext::Client.select
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientSingSelect(VALUE self)
{
  VALUE win = subSubtleSingSelect(self);

  return None != NUM2LONG(win) ? subClientSingFind(self, win) : Qnil;
} /* }}} */

/* subClientSingFind {{{ */
/*
 * call-seq: find(value) -> Array
 *           [value]     -> Array
 *
 * Find Client by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>_NET_CLIENT_LIST</code> property list.
 * [String] Regexp match against both <code>WM_CLASS</code> values.
 * [Hash]   Instead of just match <code>WM_CLASS</code> match against
 *          following properties:
 *
 *          [:name]     Match against <code>WM_NAME</code>
 *          [:instance] Match against first value of <code>WM_NAME</code>
 *          [:class]    Match against second value of <code>WM_NAME</code>
 *          [:gravity]  Match against window Gravity
 *          [:role]     Match against <code>WM_ROLE</code>
 *          [:pid]      Match against window pid
 *
 *          With one of following keys: :title, :name, :class, :gravity
 * [Symbol] Either <i>:current</i> for current View, <i>:all</i> for an
 *          array of all Views or any string for an <b>exact</b> match.
 *
 *  Subtlext::Client.find(1)
 *  => [#<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client.find("subtle")
 *  => [#<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client[".*"]
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client["subtle"]
 *  => []
 *
 *  Subtlext::Client[:terms]
 *  => [#<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client[name: "subtle"]
 *  => [#<Subtlext::Client:xxx>]
 */

VALUE
subClientSingFind(VALUE self,
  VALUE value)
{
  return ClientFind(value, False);
} /* }}} */

/* subClientSingFirst {{{ */
/*
 * call-seq: first(value) -> Subtlext::Client or nil
 *
 * Find first Client by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>_NET_CLIENT_LIST</code> property list.
 * [String] Regexp match against both <code>WM_CLASS</code> values.
 * [Hash]   Instead of just match <code>WM_CLASS</code> match against
 *          following properties:
 *
 *          [:name]     Match against <code>WM_NAME</code>
 *          [:instance] Match against first value of <code>WM_NAME</code>
 *          [:class]    Match against second value of <code>WM_NAME</code>
 *          [:gravity]  Match against window Gravity
 *          [:role]     Match against <code>WM_ROLE</code>
 *          [:pid]      Match against window pid
 *
 *          With one of following keys: :title, :name, :class, :gravity
 * [Symbol] Either <i>:current</i> for current View, <i>:all</i> for an
 *          array of all Views or any string for an <b>exact</b> match.
 *
 *  Subtlext::Client.first(1)
 *  => #<Subtlext::Client:xxx>
 *
 *  Subtlext::Client.first("subtle")
 *  => #<Subtlext::Client:xxx>
 *
 *  Subtlext::Client.first(name: "subtle")
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientSingFirst(VALUE self,
  VALUE value)
{
  return ClientFind(value, True);
} /* }}} */

/* subClientSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Client
 *
 * Get current active Client.
 *
 *  Subtlext::Client.current
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientSingCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get current client */
  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      /* Update client values */
      if(RTEST(client = subClientInstantiate(*focus)))
        subClientUpdate(client);

      free(focus);
    }
  else rb_raise(rb_eStandardError, "Invalid current window");

  return client;
} /* }}} */

/* subClientSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get array of all visible Client on all Views.
 *
 *  Subtlext::Client.visible
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client.visible
 *  => []
 */

VALUE
subClientSingVisible(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_TAGS", False), NULL);

  /* Check results */
  if(clients && visible)
    {
      for(i = 0; i < nclients; i++)
        {
          unsigned long *tags = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL, XInternAtom(display,
            "SUBTLE_CLIENT_TAGS", False), NULL);

          /* Create client on match */
          if(tags && *tags && *visible & *tags &&
              RTEST(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
            {
              subClientUpdate(client);
              rb_ary_push(array, client);
            }

          if(tags) free(tags);
        }
    }

  if(clients) free(clients);
  if(visible) free(visible);

  return array;
} /* }}} */

/* subClientSingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get an array of all Clients based on <code>_NET_CLIENT_LIST</code>
 * property list.
 *
 *  Subtlext::Client.list
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client.list
 *  => []
 */

VALUE
subClientSingList(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          /* Create client */
          if(RTEST(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
            {
              subClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subClientSingRecent {{{ */
/*
 * call-seq: recent -> Array
 *
 * Get array of the last five active Clients.
 *
 *  Subtlext::Client.recent
 *  => [ #<Subtlext::Client:xxx> ]
 */

VALUE
subClientSingRecent(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_ACTIVE_WINDOW", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          /* Create client */
          if(!NIL_P(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
            {
              subClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* Helper */

/* subClientInstantiate {{{ */
VALUE
subClientInstantiate(Window win)
{
  VALUE klass = Qnil, client = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Client"));
  client = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return client;
} /* }}} */

/* Class */

/* subClientInit {{{ */
/*
 * call-seq: new(win) -> Subtlext::Client
 *
 * Create a new Client object locally <b>without</b> calling #save automatically.
 *
 * The Client <b>won't</b> be visible until #save is called.
 *
 *  client = Subtlext::Client.new(1)
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(win));

  /* Init object */
  rb_iv_set(self, "@win",      win);
  rb_iv_set(self, "@name",     Qnil);
  rb_iv_set(self, "@instance", Qnil);
  rb_iv_set(self, "@klass",    Qnil);
  rb_iv_set(self, "@role",     Qnil);
  rb_iv_set(self, "@geometry", Qnil);
  rb_iv_set(self, "@gravity",  Qnil);
  rb_iv_set(self, "@screen",   Qnil);
  rb_iv_set(self, "@flags",    Qnil);
  rb_iv_set(self, "@tags",     Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subClientUpdate {{{ */
/*
 * call-seq: update -> Subtlext::Client
 *
 * Update Client properties based on <b>required</b> Client window id.
 *
 *  client.update
 *  => nil
 */

VALUE
subClientUpdate(VALUE self)
{
  Window win = None;

  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check values */
  if(0 <= (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *tags = NULL, *flags = NULL;
      char *wmname = NULL, *wminstance = NULL, *wmclass = NULL, *role = NULL;

      /* Fetch name, instance and class */
      subSharedPropertyClass(display, win, &wminstance, &wmclass);
      subSharedPropertyName(display, win, &wmname, wmclass);

      /* Fetch tags, flags and role */
      tags  = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
        XInternAtom(display, "SUBTLE_CLIENT_TAGS", False), NULL);
      flags = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
        XInternAtom(display, "SUBTLE_CLIENT_FLAGS", False), NULL);
      role  = subSharedPropertyGet(display, win, XA_STRING,
        XInternAtom(display, "WM_WINDOW_ROLE", False), NULL);

      /* Set properties */
      rb_iv_set(self, "@tags",     tags  ? INT2FIX(*tags)  : INT2FIX(0));
      rb_iv_set(self, "@flags",    flags ? INT2FIX(*flags) : INT2FIX(0));
      rb_iv_set(self, "@name",     rb_str_new2(wmname));
      rb_iv_set(self, "@instance", rb_str_new2(wminstance));
      rb_iv_set(self, "@klass",    rb_str_new2(wmclass));
      rb_iv_set(self, "@role",     role ? rb_str_new2(role) : Qnil);

      /* Set to nil for on demand loading */
      rb_iv_set(self, "@geometry", Qnil);
      rb_iv_set(self, "@gravity",  Qnil);

      if(tags)  free(tags);
      if(flags) free(flags);
      if(role)  free(role);
      free(wmname);
      free(wminstance);
      free(wmclass);
    }
  else rb_raise(rb_eStandardError, "Invalid client id `%#lx'", win);

  return self;
} /* }}} */

/* subClientViewList {{{ */
/*
 * call-seq: views -> Array
 *
 * Get array of Views Client is visible.
 *
 *  client.views
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  client.views
 *  => nil
 */

VALUE
subClientViewList(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  VALUE win = Qnil, array = Qnil, method = Qnil, klass = Qnil;
  unsigned long *view_tags = NULL, *client_tags = NULL, *flags = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("View"));
  array   = rb_ary_new();
  names   = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  view_tags   = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VIEW_TAGS", False), NULL);
  client_tags = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_CLIENT_TAGS", False), NULL);
  flags       = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_CLIENT_FLAGS", False), NULL);

  /* Check results */
  if(names && view_tags && client_tags)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Check if there are common tags or window is stick */
          if((view_tags[i] & *client_tags) ||
              (flags && *flags & SUB_EWMH_STICK))
            {
              /* Create new view */
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id", INT2FIX(i));
              rb_ary_push(array, v);
            }
        }

    }

  if(names)       XFreeStringList(names);
  if(view_tags)   free(view_tags);
  if(client_tags) free(client_tags);
  if(flags)       free(flags);

  return array;
} /* }}} */

/* subClientFlagsAskFull {{{ */
/*
 * call-seq: is_full? -> true or false
 *
 * Check if Client is fullscreen.
 *
 *  client.is_full?
 *  => true
 *
 *  client.is_full?
 *  => false
 */

VALUE
subClientFlagsAskFull(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FULL);
} /* }}} */

/* subClientFlagsAskFloat {{{ */
/*
 * call-seq: is_float? -> true or false
 *
 * Check if Client is floating.
 *
 *  client.is_float?
 *  => true
 *
 *  client.is_float?
 *  => false
 */

VALUE
subClientFlagsAskFloat(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FLOAT);
} /* }}} */

/* subClientFlagsAskStick {{{ */
/*
 * call-seq: is_stick? -> true or false
 *
 * Check if Client is sticky.
 *
 *  client.is_stick?
 *  => true
 *
 *  client.is_stick?
 *  => false
 */

VALUE
subClientFlagsAskStick(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_STICK);
} /* }}} */

/* subClientFlagsAskResize {{{ */
/*
 * call-seq: is_resize? -> true or false
 *
 * Check if Client uses size hints.
 *
 *  client.is_resize?
 *  => true
 *
 *  client.is_resize?
 *  => false
 */

VALUE
subClientFlagsAskResize(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_RESIZE);
} /* }}} */

/* subClientFlagsAskUrgent {{{ */
/*
 * call-seq: is_urgent? -> true or false
 *
 * Check if Client is urgent.
 *
 *  client.is_urgent?
 *  => true
 *
 *  client.is_urgent?
 *  => false
 */

VALUE
subClientFlagsAskUrgent(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_URGENT);
} /* }}} */

/* subClientFlagsAskZaphod {{{ */
/*
 * call-seq: is_zaphod? -> true or false
 *
 * Check if Client is zaphod.
 *
 *  client.is_zaphod?
 *  => true
 *
 *  client.is_zaphod?
 *  => false
 */

VALUE
subClientFlagsAskZaphod(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_ZAPHOD);
} /* }}} */

/* subClientFlagsAskFixed {{{ */
/*
 * call-seq: is_fixed? -> true or false
 *
 * Check if Client is fixed.
 *
 *  client.is_fixed?
 *  => true
 *
 *  client.is_fixed?
 *  => false
 */

VALUE
subClientFlagsAskFixed(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FIXED);
} /* }}} */

/* subClientFlagsAskBorderless {{{ */
/*
 * call-seq: is_borderless? -> true or false
 *
 * Check if Client is borderless.
 *
 *  client.is_borderless?
 *  => true
 *
 *  client.is_borderless?
 *  => false
 */

VALUE
subClientFlagsAskBorderless(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_BORDERLESS);
} /* }}} */

/* subClientFlagsToggleFull {{{ */
/*
 * call-seq: toggle_full -> Subtlext::Client
 *
 * Toggle Client fullscreen state.
 *
 *  client.toggle_full
 *  => nil
 */

VALUE
subClientFlagsToggleFull(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_FULLSCREEN", SUB_EWMH_FULL);
} /* }}} */

/* subClientFlagsToggleFloat {{{ */
/*
 * call-seq: toggle_float -> Subtlext::Client
 *
 * Toggle Client floating state.
 *
 *  client.toggle_float
 *  => nil
 */

VALUE
subClientFlagsToggleFloat(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_ABOVE", SUB_EWMH_FLOAT);
} /* }}} */

/* subClientFlagsToggleStick {{{ */
/*
 * call-seq: toggle_stick -> Subtlext::Client
 *
 * Toggle Client sticky state.
 *
 *  client.toggle_stick
 *  => nil
 */

VALUE
subClientFlagsToggleStick(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_STICKY", SUB_EWMH_STICK);
} /* }}} */

/* subClientFlagsToggleResize {{{ */
/*
 * call-seq: toggle_stick -> Subtlext::Client
 *
 * Toggle Client resize state.
 *
 *  client.toggle_stick
 *  => nil
 */

VALUE
subClientFlagsToggleResize(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_RESIZE, True);
} /* }}} */

/* subClientFlagsToggleUrgent {{{ */
/*
 * call-seq: toggle_urgent -> Subtlext::Client
 *
 * Toggle Client urgent state.
 *
 *  client.toggle_urgent
 *  => nil
 */

VALUE
subClientFlagsToggleUrgent(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_URGENT, True);
} /* }}} */

/* subClientFlagsToggleZaphod {{{ */
/*
 * call-seq: toggle_zaphod -> Subtlext::Client
 *
 * Toggle Client zaphod state.
 *
 *  client.toggle_zaphod
 *  => nil
 */

VALUE
subClientFlagsToggleZaphod(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_ZAPHOD, True);
} /* }}} */

/* subClientFlagsToggleFixed {{{ */
/*
 * call-seq: toggle_fixed -> Subtlext::Client
 *
 * Toggle Client fixed state.
 *
 *  client.toggle_fixed
 *  => nil
 */

VALUE
subClientFlagsToggleFixed(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_FIXED, True);
} /* }}} */

/* subClientFlagsToggleBorderless {{{ */
/*
 * call-seq: toggle_borderless -> Subtlext::Client
 *
 * Toggle Client borderless state.
 *
 *  client.toggle_borderless
 *  => nil
 */

VALUE
subClientFlagsToggleBorderless(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_BORDERLESS, True);
} /* }}} */

/* subClientFlagsWriter {{{ */
/*
 * call-seq: flags=(array) -> Subtlext::Client
 *
 * Set multiple flags at once. Flags can be one or a combination of the
 * following:
 *
 * [:full]       Set fullscreen mode
 * [:float]      Set floating mode
 * [:stick]      Set sticky mode
 * [:resize]     Set resize mode
 * [:urgent]     Set urgent mode
 * [:zaphod]     Set zaphod mode
 * [:fixed]      Set fixed mode
 * [:borderless] Set borderless mode
 *
 *  client.flags = [ :float, :stick ]
 *  => nil
 */

VALUE
subClientFlagsWriter(VALUE self,
  VALUE value)
{
  /* Check object type */
  if(T_ARRAY == rb_type(value))
    {
      int i, flags = 0;
      VALUE entry = Qnil;

      /* Translate flags */
      for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); ++i)
        {
          if(CHAR2SYM("full")            == entry) flags |= SUB_EWMH_FULL;
          else if(CHAR2SYM("float")      == entry) flags |= SUB_EWMH_FLOAT;
          else if(CHAR2SYM("stick")      == entry) flags |= SUB_EWMH_STICK;
          else if(CHAR2SYM("resize")     == entry) flags |= SUB_EWMH_RESIZE;
          else if(CHAR2SYM("urgent")     == entry) flags |= SUB_EWMH_URGENT;
          else if(CHAR2SYM("zaphod")     == entry) flags |= SUB_EWMH_ZAPHOD;
          else if(CHAR2SYM("fixed")      == entry) flags |= SUB_EWMH_FIXED;
          else if(CHAR2SYM("borderless") == entry) flags |= SUB_EWMH_BORDERLESS;
        }

      ClientFlagsSet(self, flags, False);
    }

  return self;
} /* }}} */

/* subClientRestackRaise {{{ */
/*
 * call-seq: raise -> Subtlext::Client
 *
 * Move Client window to top of window stack.
 *
 *  client.raise
 *  => nil
 */

VALUE
subClientRestackRaise(VALUE self)
{
  return ClientRestack(self, Above);
} /* }}} */

/* subClientRestackLower {{{ */
/*
 * call-seq: lower -> Subtlext::Client
 *
 * Move Client window to bottom of window stack.
 *
 *  client.raise
 *  => nil
 */

VALUE
subClientRestackLower(VALUE self)
{
  return ClientRestack(self, Below);
} /* }}} */

/* subClientAskAlive {{{ */
/*
 * call-seq: alive? -> true or false
 *
 * Check if client is alive.
 *
 *  client.alive?
 *  => true
 *
 *  client.alive?
 *  => false
 */

VALUE
subClientAskAlive(VALUE self)
{
  VALUE ret = Qfalse, win = Qnil;
  XWindowAttributes attrs;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch client attributes */
  if(!XGetWindowAttributes(display, NUM2LONG(win), &attrs))
    rb_obj_freeze(self);
  else ret = Qtrue;

  return ret;
} /* }}} */

/* subClientGravityReader {{{ */
/*
 * call-seq: gravity -> Subtlext::Gravity
 *
 * Get Client Gravity.
 *
 *  client.gravity
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subClientGravityReader(VALUE self)
{
  VALUE win = Qnil, gravity = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Load on demand */
  if(NIL_P((gravity = rb_iv_get(self, "@gravity"))))
    {
      int *id = NULL;
      char buf[5] = { 0 };

      /* Get gravity */
      if((id = (int *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
          XInternAtom(display, "SUBTLE_CLIENT_GRAVITY", False), NULL)))
        {
          /* Create gravity */
          snprintf(buf, sizeof(buf), "%d", *id);
          gravity = subGravityInstantiate(buf);

          subGravitySave(gravity);

          rb_iv_set(self, "@gravity", gravity);

          free(id);
        }
    }

  return gravity;
} /* }}} */

/* subClientGravityWriter {{{ */
/*
 * call-seq: gravity=(fixnum) -> Fixnum
 *           gravity=(symbol) -> Symbol
 *           gravity=(object) -> Object
 *           gravity=(hash)   -> Hash
 *
 * Set Client Gravity either for current or for specific View.
 *
 *  # Set gravity for current view
 *  client.gravity = 0
 *  => #<Subtlext::Gravity:xxx>
 *
 *  client.gravity = :center
 *  => #<Subtlext::Gravity:xxx>
 *
 *  client.gravity = Subtlext::Gravity[0]
 *  => #<Subtlext::Gravity:xxx>
 *
 *  # Set gravity for specific view
 *  client.gravity = { :terms => :center }
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subClientGravityWriter(VALUE self,
  VALUE value)
{
  /* Check ruby object */
  rb_check_frozen(self);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check value type */
  switch(rb_type(value))
    {
      case T_FIXNUM:
      case T_SYMBOL:
      case T_STRING: ClientGravity(Qnil, value, self); break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value,
            rb_const_get(mod, rb_intern("Gravity"))))
          ClientGravity(Qnil, value, self);
        break;
      case T_HASH:
        rb_hash_foreach(value, ClientGravity, self);
        break;
      default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  /* Reset gravity */
  rb_iv_set(self, "@gravity", Qnil);

  return value;
} /* }}} */

/* subClientGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Client Geometry.
 *
 *  client.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subClientGeometryReader(VALUE self)
{
  VALUE win = Qnil, geom = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Load on demand */
  if(NIL_P((geom = rb_iv_get(self, "@geometry"))))
    {
      XRectangle geometry = { 0 };

      subSharedPropertyGeometry(display, NUM2LONG(win), &geometry);

      geom = subGeometryInstantiate(geometry.x, geometry.y,
        geometry.width, geometry.height);

      rb_iv_set(self, "@geometry", geom);
    }

  return geom;
} /* }}} */

/* subClientGeometryWriter {{{ */
/*
 * call-seq: geometry=(x, y, width, height) -> Fixnum
 *           geometry=(array)               -> Array
 *           geometry=(hash)                -> Hash
 *           geometry=(string)              -> String
 *           geometry=(geometry)            -> Subtlext::Geometry
 *
 * Set Client geometry.
 *
 *  client.geometry = 0, 0, 100, 100
 *  => 0
 *
 *  client.geometry = [ 0, 0, 100, 100 ]
 *  => [ 0, 0, 100, 100 ]
 *
 *  client.geometry = { x: 0, y: 0, width: 100, height: 100 }
 *  => { x: 0, y: 0, width: 100, height: 100 }
 *
 *  client.geometry = "0x0+100+100"
 *  => "0x0+100+100"
 *
 *  client.geometry = Subtlext::Geometry(0, 0, 100, 100)
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subClientGeometryWriter(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE klass = Qnil, geom = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Delegate arguments */
  klass = rb_const_get(mod, rb_intern("Geometry"));
  geom  = rb_funcall2(klass, rb_intern("new"), argc, argv);

  /* Update geometry */
  if(RTEST(geom))
    {
      VALUE win = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      GET_ATTR(self, "@win", win);

      data.l[1] = FIX2INT(rb_iv_get(geom,  "@x"));
      data.l[2] = FIX2INT(rb_iv_get(geom,  "@y"));
      data.l[3] = FIX2INT(rb_iv_get(geom,  "@width"));
      data.l[4] = FIX2INT(rb_iv_get(geom,  "@height"));

      subSharedMessage(display, NUM2LONG(win),
        "_NET_MOVERESIZE_WINDOW", data, 32, True);

      rb_iv_set(self, "@geometry", geom);
    }

  return geom;
} /* }}} */

/* subClientScreenReader {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get Client Screen.
 *
 *  client.screen
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subClientScreenReader(VALUE self)
{
  VALUE screen = Qnil, win = Qnil;
  int *id = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Get screen */
  if((id = (int *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
      XInternAtom(display, "SUBTLE_CLIENT_SCREEN", False), NULL)))
    {
      screen = subScreenSingFind(self, INT2FIX(*id));

      free(id);
    }

  return screen;
} /* }}} */

/* subClientToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Client object to string.
 *
 *  puts client
 *  => "subtle"
 */

VALUE
subClientToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subClientKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Send a close signal to Client and <b>freeze</b> this object.
 *
 *  client.kill
 *  => nil
 */

VALUE
subClientKill(VALUE self)
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
