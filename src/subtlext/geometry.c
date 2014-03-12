 /**
  * @package subtlext
  *
  * @file Geometry functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/geometry.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* GeometryEqual {{{ */
VALUE
GeometryEqual(VALUE self,
  VALUE other)
{
  int ret = False;

  /* Check ruby object types */
  if(rb_obj_class(self) == rb_obj_class(other))
    {
      XRectangle r1 = { 0 }, r2 = { 0 };

      /* Get rectangles */
      subGeometryToRect(self,  &r1);
      subGeometryToRect(other, &r2);

      ret = (r1.x == r2.x && r1.y == r2.y &&
        r1.width == r2.width && r1.height == r2.height);
    }

  return ret ? Qtrue : Qfalse;
} /* }}} */

/* Helper */

/* subGeometryInstantiate {{{ */
VALUE
subGeometryInstantiate(int x,
  int y,
  int width,
  int height)
{
  VALUE klass = Qnil, geometry = Qnil;

  /* Create new instance */
  klass    = rb_const_get(mod, rb_intern("Geometry"));
  geometry = rb_funcall(klass, rb_intern("new"), 4,
    INT2FIX(x), INT2FIX(y), INT2FIX(width), INT2FIX(height));

  return geometry;
} /* }}} */

/* subGeometryToRect {{{ */
void
subGeometryToRect(VALUE self,
  XRectangle *r)
{
  /* Set values */
  r->x      = FIX2INT(rb_iv_get(self, "@x"));
  r->y      = FIX2INT(rb_iv_get(self, "@y"));
  r->width  = FIX2INT(rb_iv_get(self, "@width"));
  r->height = FIX2INT(rb_iv_get(self, "@height"));
} /* }}} */

/* Class */

/* subGeometryInit {{{ */
/*
 * call-seq: new(x, y, width, height) -> Subtlext::Geometry
 *           new(array)               -> Subtlext::Geometry
 *           new(hash)                -> Subtlext::Geometry
 *           new(string)              -> Subtlext::Geometry
 *           new(geometry)            -> Subtlext::Geometry
 *
 * Create a new Geometry object from givem <i>value</i> which can be of
 * following types:
 *
 * [Array]    Must be an array with values for x, y, width and height
 * [Hash]     Must be a hash with values for x, y, width and height
 * [String]   Must be a string of following format: XxY+WIDTH+HEIGHT
 * [Geometry] Copy geometry from a Geometry object
 *
 * Or just pass one argument for x, y, width and height.
 *
 * Creating a geometry with <b>zero</b> width or height will raise
 * a StandardError.
 *
 *  geom = Subtlext::Geometry.new(0, 0, 50, 50)
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new([ 0, 0, 50, 50 ])
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new({ :x => 0, :y => 0, :width => 50, :height => 50 })
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new("0x0+100+100")
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new(Subtlext::Geometry.new(0, 0, 50, 50))
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGeometryInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE value = Qnil, data[4] = { Qnil };

  rb_scan_args(argc, argv, "13", &data[0], &data[1], &data[2], &data[3]);
  value = data[0];

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: break;
      case T_ARRAY:
        if(4 == FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL)))
          {
            int i;

            for(i = 0; 4 > i; i++)
              data[i] = rb_ary_entry(value, i);
          }
        break;
      case T_HASH:
          {
            int i;
            const char *syms[] = { "x", "y", "width", "height" };

            for(i = 0; 4 > i; i++)
              data[i] = rb_hash_lookup(value, CHAR2SYM(syms[i]));
          }
        break;
      case T_STRING:
          {
            XRectangle geom = { 0 };

            sscanf(RSTRING_PTR(value), "%hdx%hd+%hu+%hu",
              &geom.x, &geom.y, &geom.width, &geom.height);

            /* Convert values */
            data[0] = INT2FIX(geom.x);
            data[1] = INT2FIX(geom.y);
            data[2] = INT2FIX(geom.width);
            data[3] = INT2FIX(geom.height);
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Geometry"));

            /* Check object instance */
            if(rb_obj_is_instance_of(value, klass))
              {
                data[0] = rb_iv_get(value, "@x");
                data[1] = rb_iv_get(value, "@y");
                data[2] = rb_iv_get(value, "@width");
                data[3] = rb_iv_get(value, "@height");
              }
          }
        break;
      default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(value));
    }

  /* Set values */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) && FIXNUM_P(data[2]) &&
      FIXNUM_P(data[3]) && 0 < FIX2INT(data[2]) && 0 < FIX2INT(data[3]))
    {
      rb_iv_set(self, "@x",      data[0]);
      rb_iv_set(self, "@y",      data[1]);
      rb_iv_set(self, "@width",  data[2]);
      rb_iv_set(self, "@height", data[3]);
    }
  else rb_raise(rb_eStandardError, "Invalid geometry");

  return self;
} /* }}} */

/* subGeometryToArray {{{ */
/*
 * call-seq: to_a -> Array
 *
 * Convert this Geometry object to an array with one fixnum for x, y, width
 * and height.
 *
 *  geom.to_a
 *  => [0, 0, 50, 50]
 */

VALUE
subGeometryToArray(VALUE self)
{
  VALUE ary = Qnil, x = Qnil, y = Qnil, width = Qnil, height = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@x",      x);
  GET_ATTR(self, "@y",      y);
  GET_ATTR(self, "@width",  width);
  GET_ATTR(self, "@height", height);

  /* Create new array */
  ary = rb_ary_new2(4);

  /* Set values */
  rb_ary_push(ary, x);
  rb_ary_push(ary, y);
  rb_ary_push(ary, width);
  rb_ary_push(ary, height);

  return ary;
} /* }}} */

/* subGeometryToHash {{{ */
/*
 * call-seq: to_hash -> Hash
 *
 * Convert this Geometry object to hash with one symbol/fixnum pair for
 * x, y, height and width.
 *
 *  geom.to_hash
 *  => { :x => 0, :y => 0, :width => 50, :height => 50 }
 */

VALUE
subGeometryToHash(VALUE self)
{
  VALUE klass = Qnil, hash = Qnil;
  VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@x",      x);
  GET_ATTR(self, "@y",      y);
  GET_ATTR(self, "@width",  width);
  GET_ATTR(self, "@height", height);

  /* Create new hash */
  klass = rb_const_get(rb_mKernel, rb_intern("Hash"));
  hash  = rb_funcall(klass, rb_intern("new"), 0, NULL);

  /* Set values */
  rb_hash_aset(hash, CHAR2SYM("x"),      x);
  rb_hash_aset(hash, CHAR2SYM("y"),      y);
  rb_hash_aset(hash, CHAR2SYM("width"),  width);
  rb_hash_aset(hash, CHAR2SYM("height"), height);

  return hash;
} /* }}} */

/* subGeometryToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Geometry object to string.
 *
 *  puts geom
 *  => "0x0+50+50"
 */

VALUE
subGeometryToString(VALUE self)
{
  char buf[256] = { 0 };
  VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@x",      x);
  GET_ATTR(self, "@y",      y);
  GET_ATTR(self, "@width",  width);
  GET_ATTR(self, "@height", height);

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", (int)FIX2INT(x),
    (int)FIX2INT(y), (int)FIX2INT(width), (int)FIX2INT(height));

  return rb_str_new2(buf);
} /* }}} */

/* subGeometryEqual {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects have the same values (based on geometry).
 *
 *  object1 == object2
 *  => true
 */

VALUE
subGeometryEqual(VALUE self,
  VALUE other)
{
  return GeometryEqual(self, other);
} /* }}} */

/* subGeometryEqualTyped {{{ */
/*
 * call-seq: eql?(other) -> True or False
 *
 * Whether both objects have the same values and types (based on geometry).
 *
 *  object1.eql? object2
 *  => true
 */

VALUE
subGeometryEqualTyped(VALUE self,
  VALUE other)
{
  return GeometryEqual(self, other);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
