 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/gravity.c,v 3216 2012/06/15 17:18:12 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* GravityToRect {{{ */
void
GravityToRect(VALUE self,
  XRectangle *r)
{
  VALUE geometry = rb_iv_get(self, "@geometry");

  subGeometryToRect(geometry, r); ///< Get values
} /* }}} */

/* GravityFindId {{{ */
static int
GravityFindId(char *match,
  char **name,
  XRectangle *geometry)
{
  int ret = -1, ngravities = 0;
  char **gravities = NULL;

  assert(match);

  /* Find gravity id */
  if((gravities = subSharedPropertyGetStrings(display,
      DefaultRootWindow(display), XInternAtom(display,
      "SUBTLE_GRAVITY_LIST", False), &ngravities)))
    {
      int i;
      XRectangle geom = { 0 };
      char buf[30] = { 0 };

      for(i = 0; i < ngravities; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geom.x, &geom.y,
            &geom.width, &geom.height, buf);

          /* Check id and name */
          if((isdigit(match[0]) && atoi(match) == i) ||
              (!isdigit(match[0]) && 0 == strcmp(match, buf)))
            {
              if(geometry) *geometry = geom;
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(buf) + 1,
                    sizeof(char));
                  strncpy(*name, buf, strlen(buf));
                }

              ret = i;
              break;
           }
       }
    }

  if(gravities) XFreeStringList(gravities);

  return ret;
} /* }}} */

/* GravityFind {{{ */
static VALUE
GravityFind(VALUE value,
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
          return subGravitySingList(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod,
            rb_intern("Gravity"))))
          return parsed;
    }

  return subSubtlextFindObjectsGeometry("SUBTLE_GRAVITY_LIST",
    "Gravity", buf, flags, first);
} /* }}} */

/* Singleton */

/* subGravitySingFind {{{ */
/*
 * call-seq: find(value) -> Array
 *           [value]     -> Array
 *
 * Find Gravity by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_GRAVITY_LIST</code> property list.
 * [String] Regexp match against name of Gravities, returns a Gravity on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Views or any string for an
 *          <b>exact</b> match.
 *
 *  Subtlext::Gravity.find(1)
 *  => [#<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity.find("subtle")
 *  => [#<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity[".*"]
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity["subtle"]
 *  => []
 *
 *  Subtlext::Gravity[:center]
 *  => [#<Subtlext::Gravity:xxx>]

 */

VALUE
subGravitySingFind(VALUE self,
  VALUE value)
{
  return GravityFind(value, False);
} /* }}} */

/* subGravitySingFirst {{{ */
/*
 * call-seq: first(value) -> Subtlext::Gravity or nil
 *
 * Find first Gravity by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_GRAVITY_LIST</code> property list.
 * [String] Regexp match against name of Gravities, returns a Gravity on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Views or any string for an
 *          <b>exact</b> match.
 *
 *  Subtlext::Gravity.first(1)
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity.first("subtle")
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subGravitySingFirst(VALUE self,
  VALUE value)
{
  return GravityFind(value, True);
} /* }}} */

/* subGravitySingList {{{ */
/*
 * call-seq: list -> Array
 *
 * Get an array of all Gravities based on the <code>SUBTLE_GRAVITIY_LIST</code>
 * property list.
 *
 *  Subtlext::Gravity.list
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity.list
 *  => []
 */

VALUE
subGravitySingList(VALUE self)
{
  return subSubtlextFindObjectsGeometry("SUBTLE_GRAVITY_LIST",
    "Gravity", NULL, 0, False);
} /* }}} */

/* Helper */

/* subGravityInstantiate {{{ */
VALUE
subGravityInstantiate(char *name)
{
  VALUE klass = Qnil, gravity = Qnil;

  /* Create new instance */
  klass   = rb_const_get(mod, rb_intern("Gravity"));
  gravity = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return gravity;
} /* }}} */

/* Class */

/* subGravityInit {{{ */
/*
 * call-seq: new(name, x, y, width, height) -> Subtlext::Gravity
 *           new(name, array)               -> Subtlext::Gravity
 *           new(name, hash)                -> Subtlext::Gravity
 *           new(name, string)              -> Subtlext::Gravity
 *           new(name, geometry)            -> Subtlext::Gravity
 *
 * Create a new Gravity object locally <b>without</b> calling
 * #save automatically.
 *
 * The Gravity <b>won't</b> be useable until #save is called.
 *
 *  gravity = Subtlext::Gravity.new("center", 0, 0, 100, 100)
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subGravityInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[5] = { Qnil }, geom = Qnil;

  rb_scan_args(argc, argv, "14", &data[0], &data[1], &data[2],
    &data[3], &data[4]);

  /* Check gravity name */
  if(T_STRING != rb_type(data[0]))
    rb_raise(rb_eArgError, "Invalid value type");

  /* Delegate arguments */
  if(RTEST(data[1]))
    {
      VALUE klass = Qnil;

      klass = rb_const_get(mod, rb_intern("Geometry"));
      geom  = rb_funcall2(klass, rb_intern("new"), argc - 1, argv + 1);
    }

  /* Init object */
  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@name",     data[0]);
  rb_iv_set(self, "@geometry", geom);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subGravitySave {{{ */
/*
 * call-seq: save -> Subtlext::Gravity
 *
 * Save new Gravity object.
 *
 *  gravity.save
 *  => nil
 */

VALUE
subGravitySave(VALUE self)
{
  int id = -1;
  XRectangle geom = { 0 };
  char *name = NULL;
  VALUE match = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", match);

  /* Find gravity */
  if(-1 == (id = GravityFindId(RSTRING_PTR(match), &name, &geom)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE geometry = rb_iv_get(self, "@geometry");

      if(NIL_P(geometry = rb_iv_get(self, "@geometry")))
        rb_raise(rb_eStandardError, "No geometry given");

      subGeometryToRect(geometry, &geom); ///< Get values

      /* Create new gravity */
      snprintf(data.b, sizeof(data.b), "%hdx%hd+%hd+%hd#%s",
        geom.x, geom.y, geom.width, geom.height, RSTRING_PTR(match));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_GRAVITY_NEW", data, 8, True);

      id = GravityFindId(RSTRING_PTR(match), NULL, NULL);
    }
  else ///< Update gravity
    {
      VALUE geometry = Qnil;

      geometry = subGeometryInstantiate(geom.x, geom.y,
        geom.width, geom.height);

      rb_iv_set(self, "@name",    rb_str_new2(name));
      rb_iv_set(self, "@gravity", geometry);

      free(name);
    }

  /* Guess gravity id */
  if(-1 == id)
    {
      int ngravities = 0;
      char **gravities = NULL;

      gravities = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &ngravities);

      id = ngravities; ///< New id should be last

      XFreeStringList(gravities);
    }

  /* Set properties */
  rb_iv_set(self, "@id", INT2FIX(id));

  return self;
} /* }}} */

/* subGravityClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get an array of Clients that have this Gravity.
 *
 *  gravity.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  tag.clients
 *  => []
 */

VALUE
subGravityClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE id = Qnil, klass = Qnil, meth = Qnil, array = Qnil, c = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

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
          unsigned long *gravity = NULL;

          /* Get window gravity */
          gravity = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL, XInternAtom(display,
            "SUBTLE_CLIENT_GRAVITY", False), NULL);

          /* Check if there are common tags or window is stick */
          if(gravity && FIX2INT(id) == *gravity &&
              !NIL_P(c = rb_funcall(klass, meth, 1, INT2FIX(i))))
            {
              rb_iv_set(c, "@win", LONG2NUM(clients[i]));

              subClientUpdate(c);

              rb_ary_push(array, c);
            }

          if(gravity) free(gravity);
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subGravityGeometryFor {{{ */
/*
 * call-seq: geometry_for(screen) -> Subtlext::Geometry
 *
 * Get the Gravity Geometry for given Screen in pixel values.
 *
 *  gravity.geometry_for(screen)
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryFor(VALUE self,
  VALUE value)
{
  VALUE geom = Qnil;

  /* Check object instance */
  if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Screen"))))
    {
      XRectangle real = { 0 }, geom_grav = { 0 }, geom_screen = { 0 };

      GravityToRect(self,  &geom_grav);
      GravityToRect(value, &geom_screen);

      /* Calculate real values for screen */
      real.width  = geom_screen.width * geom_grav.width / 100;
      real.height = geom_screen.height * geom_grav.height / 100;
      real.x      = geom_screen.x +
        (geom_screen.width - real.width) * geom_grav.x / 100;
      real.y      = geom_screen.y +
        (geom_screen.height - real.height) * geom_grav.y / 100;

      geom = subGeometryInstantiate(real.x, real.y, real.width, real.height);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return geom;
} /* }}} */

/* subGravityGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get the Gravity Geometry
 *
 *  gravity.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryReader(VALUE self)
{
  VALUE geometry = Qnil, name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  /* Load on demand */
  if(NIL_P((geometry = rb_iv_get(self, "@geometry"))))
    {
      XRectangle geom = { 0 };

      GravityFindId(RSTRING_PTR(name), NULL, &geom);

      geometry = subGeometryInstantiate(geom.x, geom.y,
        geom.width, geom.height);
      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* subGravityGeometryWriter {{{ */
/*
 * call-seq: geometry=(x, y, width, height) -> Fixnum
 *           geometry=(array)               -> Array
 *           geometry=(hash)                -> Hash
 *           geometry=(string)              -> String
 *           geometry=(geometry)            -> Subtlext::Geometry
 *
 * Set the Gravity Geometry
 *
 *  gravity.geometry = 0, 0, 100, 100
 *  => 0
 *
 *  gravity.geometry = [ 0, 0, 100, 100 ]
 *  => [ 0, 0, 100, 100 ]
 *
 *  gravity.geometry = { x: 0, y: 0, width: 100, height: 100 }
 *  => { x: 0, y: 0, width: 100, height: 100 }
 *
 *  gravity.geometry = "0x0+100+100"
 *  => "0x0+100+100"
 *
 *  gravity.geometry = Subtlext::Geometry(0, 0, 100, 100)
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryWriter(int argc,
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
  if(RTEST(geom)) rb_iv_set(self, "@geometry", geom);

  return geom;
} /* }}} */

/* subGravityTilingWriter {{{ */
/*
 * call-seq: tiling=(value) -> Symbol or nil
 *
 * Set the tiling mode for gravity
 *
 *  gravity.tiling = :vert
 *  => :vert
 *
 *  gravity.tiling = :horz
 *  => :horz
 *
 *  gravity.tiling = nil
 *  => nil
 */

VALUE
subGravityTilingWriter(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check value type */
  switch(rb_type(value))
    {
      case T_SYMBOL:
        if(CHAR2SYM("horz")      == value) flags = SUB_EWMH_HORZ;
        else if(CHAR2SYM("vert") == value) flags = SUB_EWMH_VERT;
        break;
      case T_NIL: break;
      default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  /* Assemble message */
  data.l[0] = FIX2INT(id);
  data.l[1] = flags;

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_GRAVITY_FLAGS", data, 32, True);

  return value;
} /* }}} */

/* subGravityToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Gravity object to string.
 *
 *  puts gravity
 *  => "TopLeft"
 */

VALUE
subGravityToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subGravityToSym {{{ */
/*
 * call-seq: to_sym -> Symbol
 *
 * Convert this Gravity object to symbol.
 *
 *  puts gravity.to_sym
 *  => :center
 */

VALUE
subGravityToSym(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return CHAR2SYM(RSTRING_PTR(name));
} /* }}} */

/* subGravityKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Remove this Gravity from subtle and <b>freeze</b> this object.
 *
 *  gravity.kill
 *  => nil
 */

VALUE
subGravityKill(VALUE self)
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
    "SUBTLE_GRAVITY_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
