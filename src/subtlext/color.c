 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/color.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

#define SCALE(i,div,mul) (unsigned long)(0 < i ? ((float)i / div) * mul : 0)

/* ColorEqual {{{ */
VALUE
ColorEqual(VALUE self,
  VALUE other,
  int check_type)
{
  int ret = False;
  VALUE pixel1 = Qnil, pixel2 = Qnil;

  /* Check ruby object */
  GET_ATTR(self,  "@pixel", pixel1);
  GET_ATTR(other, "@pixel", pixel2);

  /* Check ruby object types */
  if(check_type)
    {
      ret = (rb_obj_class(self) == rb_obj_class(other) && pixel1 == pixel2);
    }
  else ret = (pixel1 == pixel2);

  return ret ? Qtrue : Qfalse;
} /* }}} */

/* ColorPixelToRGB {{{ */
static void
ColorPixelToRGB(XColor *xcolor)
{
  XQueryColor(display, DefaultColormap(display,
    DefaultScreen(display)), xcolor);

  /* Scale 65535 to 255 */
  xcolor->red   = SCALE(xcolor->red,   65535, 255);
  xcolor->green = SCALE(xcolor->green, 65535, 255);
  xcolor->blue  = SCALE(xcolor->blue,  65535, 255);
} /* }}} */

/* ColorRGBToPixel {{{ */
static void
ColorRGBToPixel(XColor *xcolor)
{
  /* Scale 255 to 65535 */
  xcolor->red   = SCALE(xcolor->red,   255, 65535);
  xcolor->green = SCALE(xcolor->green, 255, 65535);
  xcolor->blue  = SCALE(xcolor->blue,  255, 65535);

  XAllocColor(display, DefaultColormap(display,
      DefaultScreen(display)), xcolor);

  /* Scale 65535 to 255 */
  xcolor->red   = SCALE(xcolor->red,   65535, 255);
  xcolor->green = SCALE(xcolor->green, 65535, 255);
  xcolor->blue  = SCALE(xcolor->blue,  65535, 255);
} /* }}} */

/* Helper */

/* subColorPixel {{{ */
unsigned long
subColorPixel(VALUE red,
  VALUE green,
  VALUE blue,
  XColor *xcolor)
{
  XColor xcol = { 0 };

  /* Check object type */
  switch(rb_type(red))
    {
      case T_FIXNUM:
      case T_BIGNUM:
        if(NIL_P(green) && NIL_P(blue))
          {
            xcol.pixel = NUM2LONG(red);

            ColorPixelToRGB(&xcol);
          }
        else
          {
            xcol.red   = NUM2INT(red);
            xcol.green = NUM2INT(green);
            xcol.blue  = NUM2INT(blue);

            ColorRGBToPixel(&xcol);
          }
        break;
      case T_STRING:
        xcol.pixel = subSharedParseColor(display, RSTRING_PTR(red));

        ColorPixelToRGB(&xcol);
        break;
      case T_ARRAY:
        if(3 == FIX2INT(rb_funcall(red, rb_intern("size"), 0, NULL)))
          {
            xcol.red   = NUM2INT(rb_ary_entry(red, 0));
            xcol.green = NUM2INT(rb_ary_entry(red, 1));
            xcol.blue  = NUM2INT(rb_ary_entry(red, 2));

            ColorRGBToPixel(&xcol);
          }
        break;
      case T_HASH:
          {
            xcol.red   = NUM2INT(rb_hash_lookup(red, CHAR2SYM("red")));
            xcol.green = NUM2INT(rb_hash_lookup(red, CHAR2SYM("green")));
            xcol.blue  = NUM2INT(rb_hash_lookup(red, CHAR2SYM("blue")));

            ColorRGBToPixel(&xcol);
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Color"));

            /* Check object instance */
            if(rb_obj_is_instance_of(red, klass))
              {
                xcol.red   = NUM2INT(rb_iv_get(red, "@red"));
                xcol.green = NUM2INT(rb_iv_get(red, "@green"));
                xcol.blue  = NUM2INT(rb_iv_get(red, "@blue"));
                xcol.pixel = NUM2LONG(rb_iv_get(red, "@pixel"));
              }
          }
        break;
      default:
        rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(red));
    }

  if(xcolor)
    {
      xcolor->red   = xcol.red;
      xcolor->green = xcol.green;
      xcolor->blue  = xcol.blue;
      xcolor->pixel = xcol.pixel;
    }


  return xcol.pixel;
} /* }}} */

/* subColorInstantiate {{{ */
VALUE
subColorInstantiate(unsigned long pixel)
{
  VALUE klass = Qnil, color = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Color"));
  color = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(pixel));

  return color;
} /* }}} */

/* Class */

/* subColorInit {{{ */
/*
 * call-seq: new(red, green, blue) -> Subtlext::Color
 *           new(string)           -> Subtlext::Color
 *           new(array)            -> Subtlext::Color
 *           new(hash)             -> Subtlext::Color
 *           new(fixnum)           -> Subtlext::Color
 *           new(color)            -> Subtlext::Color
 *
 * Create new Color object from given <i>value</i> which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object
 *
 * Or just pass one argument for red, green or blue.
 *
 *  color = Subtlext::Color.new(51, 102, 253)
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new("#336699")
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new("red")
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new([ 51, 102, 253 ])
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new({ :red => 51, :green => 102, :blue => 253 ])
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new(14253553)
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new(Subtlext::Color.new(51, 102, 253))
 *  => #<Subtlext::Color:xxx>
 */

VALUE
subColorInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[3] = { Qnil };
  XColor xcolor = { 0 };

  rb_scan_args(argc, argv, "12", &data[0], &data[1], &data[2]);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get color values */
  subColorPixel(data[0], data[1], data[2], &xcolor);

  /* Set values */
  rb_iv_set(self, "@red",   INT2FIX(xcolor.red));
  rb_iv_set(self, "@green", INT2FIX(xcolor.green));
  rb_iv_set(self, "@blue",  INT2FIX(xcolor.blue));
  rb_iv_set(self, "@pixel", LONG2NUM(xcolor.pixel));

  return self;
} /* }}} */

/* subColorToHex {{{ */
/*
 * call-seq: to_hex -> String
 *
 * Convert this Color object to <b>rrggbb</b> hex string.
 *
 *  puts color.to_hex
 *  => "#ff0000"
 */

VALUE
subColorToHex(VALUE self)
{
  char buf[8] = { 0 };
  VALUE red = Qnil, green = Qnil, blue = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@red",   red);
  GET_ATTR(self, "@green", green);
  GET_ATTR(self, "@blue",  blue);

  snprintf(buf, sizeof(buf), "#%02X%02X%02X",
    (int)FIX2INT(red), (int)FIX2INT(green), (int)FIX2INT(blue));

  return rb_str_new2(buf);
} /* }}} */

/* subColorToArray {{{ */
/*
 * call-seq: to_a -> Array
 *
 * Convert this Color object to an array with one fixnum for red, blue
 * and green.
 *
 *  color.to_a
 *  => [ 51, 102, 253 ]
 */

VALUE
subColorToArray(VALUE self)
{
  VALUE ary = Qnil, red = Qnil, green = Qnil, blue = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@red",   red);
  GET_ATTR(self, "@green", green);
  GET_ATTR(self, "@blue",  blue);

  /* Create new array */
  ary = rb_ary_new2(3);

  /* Set values */
  rb_ary_push(ary, red);
  rb_ary_push(ary, green);
  rb_ary_push(ary, blue);

  return ary;
} /* }}} */

/* subColorToHash {{{ */
/*
 * call-seq: to_hash -> Hash
 *
 * Convert this Color object to a hash with one symbol/fixnum pair for
 * red, green and blue.
 *
 *  color.to_hash
 *  => { :red => 51, :green => 102, :blue => 253 }
 */

VALUE
subColorToHash(VALUE self)
{
  VALUE klass = Qnil, hash = Qnil, red = Qnil, green = Qnil, blue = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@red",   red);
  GET_ATTR(self, "@green", green);
  GET_ATTR(self, "@blue",  blue);

  /* Create new hash */
  klass = rb_const_get(rb_mKernel, rb_intern("Hash"));
  hash  = rb_funcall(klass, rb_intern("new"), 0, NULL);

  /* Set values */
  rb_hash_aset(hash, CHAR2SYM("red"),   red);
  rb_hash_aset(hash, CHAR2SYM("green"), green);
  rb_hash_aset(hash, CHAR2SYM("blue"),  blue);

  return hash;
} /* }}} */

/* subColorToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Color object to string.
 *
 *  puts color
 *  => "<>123456789<>"
 */

VALUE
subColorToString(VALUE self)
{
  char buf[20] = { 0 };
  VALUE pixel = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@pixel", pixel);

  snprintf(buf, sizeof(buf), "%s#%ld%s",
    SEPARATOR, NUM2LONG(pixel), SEPARATOR);

  return rb_str_new2(buf);
} /* }}} */

/* subColorOperatorPlus {{{ */
/*
 * call-seq: +(string) -> String
 *
 * Convert this Color to string and concat given string.
 *
 *  color + "subtle"
 *  => "<>123456789<>subtle"
 */

VALUE
subColorOperatorPlus(VALUE self,
  VALUE value)
{
  return subSubtlextConcat(subColorToString(self), value);
} /* }}} */

/* subColorEqual {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects have the same values (based on pixel).
 *
 *  object1 == object2
 *  => true
 */

VALUE
subColorEqual(VALUE self,
  VALUE other)
{
  return ColorEqual(self, other, False);
} /* }}} */

/* subColorEqualTyped {{{ */
/*
 * call-seq: eql?(other) -> True or False
 *
 * Whether both objects have the same values and types (based on pixel).
 *
 *  object1.eql? object2
 *  => true
 */

VALUE
subColorEqualTyped(VALUE self,
  VALUE other)
{
  return ColorEqual(self, other, True);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
