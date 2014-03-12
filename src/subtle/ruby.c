
 /**
  * @package subtle
  *
  * @file Ruby functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/ruby.c,v 3223 2012/06/20 11:29:44 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <ctype.h>
#include <ruby.h>
#include <ruby/encoding.h>
#include <X11/Xresource.h>
#include "subtle.h"

#ifdef HAVE_WORDEXP_H
  #include <wordexp.h>
#endif /* HAVE_WORDEXP_H */

/* Macros {{{ */
#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))
/* }}} */

/* Globals {{{ */
static VALUE shelter = Qnil, mod = Qnil, config_sublets = Qnil;
static VALUE config_instance = Qnil, config_methods = Qnil;
/* }}} */

/* Typedef {{{ */
typedef struct rubysymbol_t
{
  VALUE sym;
  int   flags;
} RubySymbols;

typedef struct rubymethods_t
{
  VALUE sym, real;
  int   flags, arity;
} RubyMethods;
/* }}} */

/* RubyBacktrace {{{ */
static void
RubyBacktrace(void)
{
  VALUE lasterr = Qnil;

  /* Get last error */
  if(!NIL_P(lasterr = rb_gv_get("$!")))
    {
      int i;
      VALUE message = Qnil, klass = Qnil, backtrace = Qnil, entry = Qnil;

      /* Fetching backtrace data */
      message   = rb_obj_as_string(lasterr);
      klass     = rb_class_path(CLASS_OF(lasterr));
      backtrace = rb_funcall(lasterr, rb_intern("backtrace"), 0, NULL);

      /* Print error and backtrace */
      subSubtleLogWarn("%s: %s\n", RSTRING_PTR(klass), RSTRING_PTR(message));
      for(i = 0; Qnil != (entry = rb_ary_entry(backtrace, i)); ++i)
        printf("\tfrom %s\n", RSTRING_PTR(entry));
    }
} /* }}} */

/* RubyFilter {{{ */
static inline int
#ifdef IS_OPENBSD
RubyFilter(struct dirent *entry)
#else
RubyFilter(const struct dirent *entry)
#endif
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

/* RubyReceiver {{{ */
static int
RubyReceiver(unsigned long instance,
  unsigned long meth)
{
  VALUE receiver = Qnil;

  /* Check object instance */
  if(rb_obj_is_instance_of(meth, rb_cMethod))
    receiver = rb_funcall(meth, rb_intern("receiver"), 0, NULL);

  return receiver == instance;
} /* }}} */

/* RubyFont {{{ */
static SubFont *
RubyFont(const char *fontname)
{
  SubFont *f = NULL;

  /* Load font */
  if(!(f = subSharedFontNew(subtle->dpy, fontname)))
    {
      subSubtleLogWarn("Cannot load font `%s'\n", fontname);

      /* Load fallback font */
      if(!(f = subSharedFontNew(subtle->dpy, DEFFONT)))
        {
          subSubtleLogError("Cannot load fallback font `%s'\n", DEFFONT);

          subSubtleFinish();

          exit(-1); ///< Should never happen
        }
    }

  return f;
} /* }}} */

/* Type converter */

/* RubySubtleToSubtlext {{{ */
static VALUE
RubySubtleToSubtlext(void *data)
{
  SubClient *c = NULL;
  VALUE object = Qnil;

  /* Convert subtle object to subtlext */
  if((c = CLIENT(data)))
    {
      int id = 0;
      VALUE subtlext = Qnil, klass = Qnil;

      XSync(subtle->dpy, False); ///< Sync before going on

      subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

      if(c->flags & SUB_TYPE_CLIENT) /* {{{ */
        {
          int flags = 0;
          VALUE value = Qnil;

          /* Create client instance */
          id     = subArrayIndex(subtle->clients, (void *)c);
          klass  = rb_const_get(subtlext, rb_intern("Client"));
          object = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

          /* Translate flags */
          subEwmhTranslateClientMode(c->flags, &flags);

          /* Set properties */
          rb_iv_set(object, "@win",      LONG2NUM(c->win));
          rb_iv_set(object, "@flags",    INT2FIX(flags));
          rb_iv_set(object, "@tags",     INT2FIX(c->tags));
          rb_iv_set(object, "@name",     rb_str_new2(c->name));
          rb_iv_set(object, "@instance", rb_str_new2(c->instance));
          rb_iv_set(object, "@klass",    rb_str_new2(c->klass));
          rb_iv_set(object, "@role",     c->role ? rb_str_new2(c->role) :
            Qnil);

          /* Create and set geometry */
          klass = rb_const_get(subtlext, rb_intern("Geometry"));
          value = rb_funcall(klass, rb_intern("new"), 4, INT2FIX(c->geom.x),
            INT2FIX(c->geom.y), INT2FIX(c->geom.width),
            INT2FIX(c->geom.height));

          rb_iv_set(object, "@geometry", value);

          /* Create and set gravity if any */
          if(-1 != c->gravityid)
            {
              SubGravity *g = GRAVITY(subArrayGet(subtle->gravities,
                c->gravityid));

              klass = rb_const_get(subtlext, rb_intern("Gravity"));
              value = rb_funcall(klass, rb_intern("new"), 1,
                rb_str_new2(XrmQuarkToString(g->quark)));

              rb_funcall(value, rb_intern("geometry="), 4, INT2FIX(g->geom.x),
                INT2FIX(g->geom.y), INT2FIX(g->geom.width),
                INT2FIX(g->geom.height));
            }
          else value = Qnil;

          rb_iv_set(object, "@gravity", value);
        } /* }}} */
      else if(c->flags & SUB_TYPE_SCREEN) /* {{{ */
        {
          SubScreen *s = SCREEN(c);
          VALUE klass_geom, geom = Qnil;

          /* Create tag instance */
          id         = subArrayIndex(subtle->screens, (void *)s);
          klass      = rb_const_get(subtlext, rb_intern("Screen"));
          klass_geom = rb_const_get(subtlext, rb_intern("Geometry"));
          object     = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));
          geom       = rb_funcall(klass_geom, rb_intern("new"), 4,
            INT2FIX(s->geom.x), INT2FIX(s->geom.y),
            INT2FIX(s->geom.width), INT2FIX(s->geom.height));

          /* Set properties */
          rb_iv_set(object, "@geometry", geom);
        } /* }}} */
      else if(c->flags & SUB_TYPE_TAG) /* {{{ */
        {
          SubTag *t = TAG(c);

          /* Create tag instance */
          id     = subArrayIndex(subtle->tags, (void *)t);
          klass  = rb_const_get(subtlext, rb_intern("Tag"));
          object = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(t->name));

          /* Set properties */
          rb_iv_set(object, "@id", INT2FIX(id));
        } /* }}} */
      else if(c->flags & SUB_TYPE_VIEW) /* {{{ */
        {
          SubView *v = VIEW(c);

          /* Create view instance */
          id     = subArrayIndex(subtle->views, (void *)v);
          klass  = rb_const_get(subtlext, rb_intern("View"));
          object = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(v->name));

          /* Set properties */
          rb_iv_set(object, "@id",   INT2FIX(id));
          rb_iv_set(object, "@tags", INT2FIX(v->tags));
        } /* }}} */
    }

  return object;
} /* }}} */

/* RubyIconToIcon {{{ */
static void
RubyIconToIcon(VALUE icon,
  SubIcon *i)
{
  VALUE width = Qnil, height = Qnil, pixmap = Qnil, bitmap = Qnil;

  assert(i);

  /* Get properties */
  width  = rb_iv_get(icon, "@width");
  height = rb_iv_get(icon, "@height");
  pixmap = rb_iv_get(icon, "@pixmap");
  bitmap = rb_funcall(icon, rb_intern("bitmap?"), 0, NULL);

  /* Update panel */
  i->pixmap = NUM2LONG(pixmap);
  i->width  = FIX2INT(width);
  i->height = FIX2INT(height);
  i->bitmap = (Qtrue == bitmap) ? True : False;
} /* }}} */

/* RubySymbolToFlag {{{ */
static void
RubySymbolToFlag(VALUE sym,
  int *flags)
{
  /* Translate symbols to flags */
  if(CHAR2SYM("name")          == sym) (*flags) |= SUB_TAG_MATCH_NAME;
  else if(CHAR2SYM("instance") == sym) (*flags) |= SUB_TAG_MATCH_INSTANCE;
  else if(CHAR2SYM("class")    == sym) (*flags) |= SUB_TAG_MATCH_CLASS;
  else if(CHAR2SYM("role")     == sym) (*flags) |= SUB_TAG_MATCH_ROLE;
  else if(CHAR2SYM("type")     == sym) (*flags) |= SUB_TAG_MATCH_TYPE;
  else if(CHAR2SYM("normal")   == sym) (*flags) |= SUB_CLIENT_TYPE_NORMAL;
  else if(CHAR2SYM("desktop")  == sym) (*flags) |= SUB_CLIENT_TYPE_DESKTOP;
  else if(CHAR2SYM("dock")     == sym) (*flags) |= SUB_CLIENT_TYPE_DOCK;
  else if(CHAR2SYM("toolbar")  == sym) (*flags) |= SUB_CLIENT_TYPE_TOOLBAR;
  else if(CHAR2SYM("splash")   == sym) (*flags) |= SUB_CLIENT_TYPE_SPLASH;
  else if(CHAR2SYM("dialog")   == sym) (*flags) |= SUB_CLIENT_TYPE_DIALOG;
} /* }}} */

/* RubyArrayToArray {{{ */
static void
RubyArrayToArray(VALUE ary,
  int *values,
  int len)
{
  /* Check value type */
  if(T_ARRAY == rb_type(ary))
    {
      int i;
      VALUE value = Qnil;

      for(i = 0; i < len; i++)
        {
          /* Check and convert value type */
          switch(rb_type(value = rb_ary_entry(ary, i)))
            {
              case T_FIXNUM: values[i] = FIX2INT(value);            break;
              case T_FLOAT:  values[i] = FIX2INT(rb_to_int(value)); break;
              default:       values[i] = 0;                         break;
            }
        }
    }
} /* }}} */

/* RubyArrayToGeometry {{{ */
static void
RubyArrayToGeometry(VALUE ary,
  XRectangle *geometry)
{
  int values[4] = { 0 };

  RubyArrayToArray(ary, values, 4);

  /* Assign data to geometry */
  geometry->x      = values[0];
  geometry->y      = values[1];
  geometry->width  = values[2];
  geometry->height = values[3];
} /* }}} */

/* RubyArrayToSides {{{ */
static void
RubyArrayToSides(VALUE ary,
  SubSides *sides)
{
  int values[4] = { 0 };

  RubyArrayToArray(ary, values, 4);

  /* Assign data to sides CSS-alike:
   * 2 values => 1. top/bottom, 2. left/right
   * 3 values => 1. top,        2. left/right, 3. bottom
   * 4 values => 1. top,        2. right,      3. bottom, 4. left
   **/

  switch((int)RARRAY_LEN(ary))
    {
      case 2:
        sides->top    = sides->bottom = values[0];
        sides->right  = sides->left   = values[1];
        break;
      case 3:
        sides->top    = values[0];
        sides->left   = sides->right  = values[1];
        sides->bottom = values[2];
        break;
      case 4:
        sides->top    = values[0];
        sides->right  = values[1];
        sides->bottom = values[2];
        sides->left   = values[3];
        break;
      default:
        rb_raise(rb_eArgError, "Too many array values");
    }
} /* }}} */

/* RubyValueToGravity {{{ */
static int
RubyValueToGravity(VALUE value)
{
  int gravity = -1;

  /* Check value type */
  switch(rb_type(value))
    {
      case T_FIXNUM: gravity = FIX2INT(value);                     break;
      case T_SYMBOL: gravity = subGravityFind(SYM2CHAR(value), 0); break;
    }

  return 0 <= gravity && gravity < subtle->gravities->ndata ? gravity : -1;
} /* }}} */

/* RubyValueToGravityString {{{ */
static void
RubyValueToGravityString(VALUE value,
  char **string)
{
  /* Check value type */
  if(T_ARRAY == rb_type(value))
    {
      int i = 0, j = 0, id = -1, size = 0;
      VALUE entry = Qnil;

      /* Create gravity string */
      size    = RARRAY_LEN(value);
      *string = (char *)subSharedMemoryAlloc(size + 1, sizeof(char));

      /* Add gravities */
      for(i = 0, j = 0; Qnil != (entry = rb_ary_entry(value, i)); i++)
        {
          /* We store gravity ids in a string to save a bit of memory */
          if(-1 != (id = RubyValueToGravity(entry)))
            (*string)[j++] = id+ GRAVITYSTRLIMIT;
          else subSubtleLogWarn("Cannot find gravity `%s'\n", SYM2CHAR(entry));
        }
    }
} /* }}} */

/* RubyValueToIcon {{{ */
static VALUE
RubyValueToIcon(VALUE value)
{
  VALUE icon = Qnil, subtlext = Qnil, klass = Qnil;

  /* Fetch data */
  subtlext = rb_const_get(rb_cObject, rb_intern("Subtlext"));
  klass    = rb_const_get(subtlext, rb_intern("Icon"));

  /* Check icon */
  switch(rb_type(value))
    {
      case T_DATA:
        /* Check object instance */
        if(rb_obj_is_instance_of(value, klass))
          {
            icon = value; ///< Lazy eval

            rb_ary_push(shelter, icon); ///< Protect from GC
          }
        break;
      case T_STRING:
        /* Create new text icon */
        icon  = rb_funcall(klass, rb_intern("new"), 1, value);

        rb_ary_push(shelter, icon); ///< Protect from GC
        break;
      default: break;
    }

  return icon;
} /* }}} */

/* RubyValueToHash {{{ */
static VALUE
RubyValueToHash(VALUE value)
{
  VALUE hash = Qnil;

  /* Check value type */
  switch(rb_type(value))
    {
      case T_HASH: hash = value; break;
      case T_NIL:                break; ///< Ignore this case
      default:
        /* Convert to hash */
        hash = rb_hash_new();

        rb_hash_aset(hash, Qnil, value);
    }

  return hash;
} /* }}} */

/* RubyHashToColor {{{ */
static void
RubyHashToColor(VALUE hash,
  const char *key,
  long *col)
{
  VALUE value = Qnil;

  /* Parse and set color if key found */
  if(T_STRING == rb_type(value = rb_hash_lookup(hash, CHAR2SYM(key))))
    *col = (long)subSharedParseColor(subtle->dpy, RSTRING_PTR(value));
} /* }}} */

/* RubyHashToInt {{{ */
static void
RubyHashToInt(VALUE hash,
  const char *key,
  int *val)
{
  VALUE value = Qnil;

  /* Parse and set color if key found */
  if(FIXNUM_P(value = rb_hash_lookup(hash, CHAR2SYM(key))))
    *val = FIX2INT(value);
} /* }}} */

/* RubyHashToBorder {{{ */
static void
RubyHashToBorder(VALUE hash,
  const char *key,
  long *col,
  int *bw)
{
  VALUE value = Qnil;

  /* Parse and set color if key found */
  switch(rb_type(value = rb_hash_lookup(hash, CHAR2SYM(key))))
    {
      case T_ARRAY:
          {
            VALUE val = Qnil;

            if(T_STRING == rb_type(val = rb_ary_entry(value, 0)))
              *col = (long)subSharedParseColor(subtle->dpy, RSTRING_PTR(val));

            if(FIXNUM_P(val  = rb_ary_entry(value, 1)))
              *bw = FIX2INT(val);
          }
        break;
      case T_STRING:
        *col = (long)subSharedParseColor(subtle->dpy, RSTRING_PTR(value));
        break;
    }
} /* }}} */

/* RubyHashToSides {{{ */
static void
RubyHashToSides(VALUE params,
  const char *name,
  SubSides *s)
{
  VALUE value = Qnil;

  /* Get sides value */
  if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
      CHAR2SYM(name)))) ///< Two or more values
    RubyArrayToSides(value, s);
  else if(FIXNUM_P(value)) ///< Single value
    {
      s->top = s->right = s->bottom = s->left = FIX2INT(value);
    }
  else
    {
      char buf[15] = { 0 };

      /* Painful string concat.. */
      snprintf(buf, sizeof(buf), "%s_top", name);
      RubyHashToInt(params, buf, &s->top);

      snprintf(buf, sizeof(buf), "%s_right", name);
      RubyHashToInt(params, buf, &s->right);

      snprintf(buf, sizeof(buf), "%s_bottom", name);
      RubyHashToInt(params, buf, &s->bottom);

      snprintf(buf, sizeof(buf), "%s_left", name);
      RubyHashToInt(params, buf, &s->left);
    }
} /* }}} */

/* Eval */

/* RubyEvalHook {{{ */
static void
RubyEvalHook(VALUE event,
  VALUE proc)
{
  int i;
  SubHook *h = NULL;

  RubySymbols hooks[] =
  {
    { CHAR2SYM("start"),          SUB_HOOK_START                                 },
    { CHAR2SYM("exit"),           SUB_HOOK_EXIT                                  },
    { CHAR2SYM("tile"),           SUB_HOOK_TILE                                  },
    { CHAR2SYM("reload"),         SUB_HOOK_RELOAD                                },
    { CHAR2SYM("client_create"),  (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_CREATE)  },
    { CHAR2SYM("client_mode"),    (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_MODE)    },
    { CHAR2SYM("client_gravity"), (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_GRAVITY) },
    { CHAR2SYM("client_focus"),   (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_FOCUS)   },
    { CHAR2SYM("client_kill"),    (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_KILL)    },
    { CHAR2SYM("tag_create"),     (SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_CREATE)     },
    { CHAR2SYM("tag_kill"),       (SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_KILL)       },
    { CHAR2SYM("view_create"),    (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_CREATE)    },
    { CHAR2SYM("view_focus"),     (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS)     },
    { CHAR2SYM("view_kill"),      (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_KILL)      }
  };

  if(subtle->flags & SUB_SUBTLE_CHECK) return; ///< Skip on check

  /* Generic hooks */
  for(i = 0; LENGTH(hooks) > i; i++)
    {
      if(hooks[i].sym == event)
        {
          /* Create new hook */
          if((h = subHookNew(hooks[i].flags, proc)))
            {
              subArrayPush(subtle->hooks, (void *)h);
              rb_ary_push(shelter, proc); ///< Protect from GC
            }

          break;
        }
    }
} /* }}} */

/* RubyEvalGrab {{{ */
static void
RubyEvalGrab(VALUE keys,
  VALUE value)
{
  int type = -1;
  SubData data = { None };

  /* Check value type */
  switch(rb_type(value))
    {
      case T_SYMBOL: /* {{{ */
          {
            /* Find symbol and add flag */
            if(CHAR2SYM("ViewNext") == value)
              {
                type = SUB_GRAB_VIEW_SELECT;
                data = DATA((unsigned long)SUB_VIEW_NEXT);
              }
            else if(CHAR2SYM("ViewPrev") == value)
              {
                type = SUB_GRAB_VIEW_SELECT;
                data = DATA((unsigned long)SUB_VIEW_PREV);
              }
            else if(CHAR2SYM("SubtleReload") == value)
              {
                type = SUB_GRAB_SUBTLE_RELOAD;
              }
            else if(CHAR2SYM("SubtleRestart") == value)
              {
                type = SUB_GRAB_SUBTLE_RESTART;
              }
            else if(CHAR2SYM("SubtleQuit") == value)
              {
                type = SUB_GRAB_SUBTLE_QUIT;
              }
            else if(CHAR2SYM("WindowMove") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
              }
            else if(CHAR2SYM("WindowMoveUp") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_UP);
              }
            else if(CHAR2SYM("WindowMoveRight") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_RIGHT);
              }
            else if(CHAR2SYM("WindowMoveDown") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_DOWN);
              }
            else if(CHAR2SYM("WindowMoveLeft") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_LEFT);
              }
            else if(CHAR2SYM("WindowResize") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
              }
            else if(CHAR2SYM("WindowResizeUp") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_UP);
              }
            else if(CHAR2SYM("WindowResizeRight") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_RIGHT);
              }
            else if(CHAR2SYM("WindowResizeDown") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_DOWN);
              }
            else if(CHAR2SYM("WindowResizeLeft") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_LEFT);
              }
            else if(CHAR2SYM("WindowFloat") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_FLOAT);
              }
            else if(CHAR2SYM("WindowFull") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_FULL);
              }
            else if(CHAR2SYM("WindowStick") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_STICK);
              }
            else if(CHAR2SYM("WindowZaphod") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_ZAPHOD);
              }
            else if(CHAR2SYM("WindowRaise") == value)
              {
                type = SUB_GRAB_WINDOW_STACK;
                data = DATA((unsigned long)SUB_CLIENT_RESTACK_UP);
              }
            else if(CHAR2SYM("WindowLower") == value)
              {
                type = SUB_GRAB_WINDOW_STACK;
                data = DATA((unsigned long)SUB_CLIENT_RESTACK_DOWN);
              }
            else if(CHAR2SYM("WindowLeft") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_LEFT);
              }
            else if(CHAR2SYM("WindowDown") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_DOWN);
              }
            else if(CHAR2SYM("WindowUp") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_UP);
              }
            else if(CHAR2SYM("WindowRight") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_GRAB_DIRECTION_RIGHT);
              }
            else if(CHAR2SYM("WindowKill") == value)
              {
                type = SUB_GRAB_WINDOW_KILL;
              }

            /* Symbols with parameters */
            if(-1 == type)
              {
                const char *name = SYM2CHAR(value);

                /* Numbered grabs */
                if(!strncmp(name, "ViewJump", 8))
                  {
                    if((name = (char *)name + 8))
                      {
                        type = SUB_GRAB_VIEW_FOCUS;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "ViewSwitch", 10))
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_VIEW_SWAP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "ScreenJump", 10))
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_SCREEN_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else
                  {
                    /* Sublet grabs? */
                    type = SUB_RUBY_DATA;
                    data = DATA((unsigned long)value);
                  }
              }
          }
        break; /* }}} */
      case T_ARRAY: /* {{{ */
        type = (SUB_GRAB_WINDOW_GRAVITY|SUB_RUBY_DATA);
        data = DATA((unsigned long)value);

        rb_ary_push(shelter, value); ///< Protect from GC
        break; /* }}} */
      case T_STRING: /* {{{ */
        type = SUB_GRAB_SPAWN;
        data = DATA(strdup(RSTRING_PTR(value)));
        break; /* }}} */
      case T_DATA: /* {{{ */
        type = SUB_GRAB_PROC;
        data = DATA(value);

        rb_ary_push(shelter, value); ///< Protect from GC
        break; /* }}} */
      default:
        subSubtleLogWarn("Unexpected value type for grab `%s'\n",
          rb_obj_classname(value));

        return;
    }

  /* Check value type */
  if(T_STRING == rb_type(keys))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          int duplicate = False, ntok = 0;
          char *tokens = NULL, *tok = NULL, *save = NULL;
          SubGrab *prev = NULL, *g = NULL;

          /* Parse keys */
          tokens = strdup(RSTRING_PTR(keys));
          tok    = strtok_r(tokens, " ", &save);

          while(tok)
            {
              /* Find or create grab */
              if((g = subGrabNew(tok, &duplicate)))
                {
                  if(!duplicate)
                    {
                      subArrayPush(subtle->grabs, (void *)g);
                      subArraySort(subtle->grabs, subGrabCompare);
                    }

                  /* Assemble chain */
                  if(0 < ntok)
                    {
                      /* Init chain array */
                      if(NULL == prev->keys)
                        {
                          /* Mark first grab as chain start */
                          if(1 == ntok) prev->flags |= SUB_GRAB_CHAIN_START;

                          prev->keys = subArrayNew();
                        }

                      /* Mark grabs w/o action as chain link */
                      if(0 == (g->flags & ~(SUB_TYPE_GRAB|
                          SUB_GRAB_KEY|SUB_GRAB_MOUSE|SUB_GRAB_CHAIN_LINK)))
                        g->flags |= SUB_GRAB_CHAIN_LINK;

                      subArrayPush(prev->keys, (void *)g);
                    }

                  prev = g;
                }

              tok = strtok_r(NULL, " ", &save);
              ntok++;
            }

          /* Add type/action to new grabs and mark as chain end */
          if(g && !duplicate)
            {
              /* Update flags */
              if(g->flags & SUB_GRAB_CHAIN_LINK)
                g->flags &= ~SUB_GRAB_CHAIN_LINK;
              if(1 < ntok) g->flags |= SUB_GRAB_CHAIN_END;

              g->flags |= type;
              g->data   = data;
            }
          else if(type & SUB_GRAB_SPAWN)
            free(data.string);

          free(tokens);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for grab");
} /* }}} */

/* RubyEvalPanel {{{ */
void
RubyEvalPanel(VALUE ary,
  int position,
  SubScreen *s)
{
  /* Check value type */
  if(T_ARRAY == rb_type(ary))
    {
      int i, j, flags = 0;
      Window panel = s->panel1;
      VALUE entry = Qnil, tray = Qnil, spacer = Qnil, separator = Qnil;
      VALUE sublets = Qnil, views = Qnil, title = Qnil, keychain;
      VALUE center = Qnil, subtlext = Qnil;
      SubPanel *p = NULL, *last = NULL;;

      /* Get syms */
      tray      = CHAR2SYM("tray");
      spacer    = CHAR2SYM("spacer");
      center    = CHAR2SYM("center");
      separator = CHAR2SYM("separator");
      sublets   = CHAR2SYM("sublets");
      views     = CHAR2SYM("views");
      title     = CHAR2SYM("title");
      keychain  = CHAR2SYM("keychain");

      /* Set position of panel */
      if(SUB_SCREEN_PANEL2 == position) panel  = s->panel2;

      /* Parse array and assemble panel */
      for(i = 0; Qnil != (entry = rb_ary_entry(ary, i)); ++i)
        {
          p = NULL;

          /* Parse array and assemble panel */
          if(entry == spacer || entry == center || entry == separator)
            {
              /* Add spacer after panel item */
              if(last && flags & SUB_PANEL_SPACER1)
                {
                  last->flags |= SUB_PANEL_SPACER2;
                  flags       &= ~SUB_PANEL_SPACER1;
                }

              /* Add separator after panel item */
              if(last && flags & SUB_PANEL_SEPARATOR1)
                {
                  last->flags |= SUB_PANEL_SEPARATOR2;
                  flags       &= ~SUB_PANEL_SEPARATOR1;
                }

              /* Add flags */
              if(entry == spacer)         flags |= SUB_PANEL_SPACER1;
              else if(entry == center)    flags |= SUB_PANEL_CENTER;
              else if(entry == separator) flags |= SUB_PANEL_SEPARATOR1;
            }
          else if(entry == sublets)
            {
              if(0 < subtle->sublets->ndata)
                {
                  /* Add dummy panel as entry point for sublets */
                  flags |= SUB_PANEL_SUBLETS;
                  p      = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
                }
            }
          else if(entry == tray)
            {
              /* Prevent multiple use */
              if(!subtle->panels.tray.screen)
                {
                  subtle->flags |= SUB_SUBTLE_TRAY;
                  flags         |= (SUB_TYPE_PANEL|SUB_PANEL_TRAY);
                  p              = &subtle->panels.tray;

                  XReparentWindow(subtle->dpy, subtle->windows.tray,
                    panel, 0, 0);
                }
            }
          else if(entry == views)
            {
              /* Create new panel views */
              p = subPanelNew(SUB_PANEL_VIEWS);
            }
          else if(entry == title)
            {
              /* Create new panel title */
              p = subPanelNew(SUB_PANEL_TITLE);
            }
          else if(entry == keychain)
            {
              /* Prevent multiple use */
              if(!subtle->panels.keychain.screen)
                {
                  flags       |= (SUB_TYPE_PANEL|SUB_PANEL_KEYCHAIN);
                  p            = &subtle->panels.keychain;
                  p->keychain  = KEYCHAIN(subSharedMemoryAlloc(1,
                    sizeof(SubKeychain)));
                }
            }
          else if(T_DATA == rb_type(entry))
            {
              if(NIL_P(subtlext))
                subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

              if(rb_obj_is_instance_of(entry,
                  rb_const_get(subtlext, rb_intern("Icon"))))
                {
                  /* Create new panel icon */
                  p = subPanelNew(SUB_PANEL_ICON);

                  RubyIconToIcon(entry, p->icon);

                  rb_ary_push(shelter, entry); ///< Protect from GC
                }
            }
          else
            {
              /* Check for sublets */
              for(j = 0; j < subtle->sublets->ndata; j++)
                {
                  SubPanel *p2 = PANEL(subtle->sublets->data[j]);

                  if(entry == CHAR2SYM(p2->sublet->name))
                    {
                      /* Check if sublet is already on a panel */
                      if(p2->screen)
                        {
                          /* Clone sublet */
                          p = subPanelNew(SUB_PANEL_COPY);

                          p->flags  |= (p2->flags & (SUB_PANEL_SUBLET|
                            SUB_PANEL_DOWN|SUB_PANEL_OVER|SUB_PANEL_OUT));
                          p->sublet  = p2->sublet;

                          printf("Cloned sublet (%s)\n", p->sublet->name);
                        }
                      else p = p2;

                      break;
                    }
                }
            }

          /* Finally add to panel */
          if(p)
            {
              p->flags  |= SUB_SCREEN_PANEL2 == position ?
                (flags|SUB_PANEL_BOTTOM) : flags; ///< Mark for bottom panel
              p->screen  = s;
              s->flags  |= position; ///< Enable this panel
              flags      = 0;
              last       = p;

              subArrayPush(s->panels, (void *)p);
            }
        }

      /* Add stuff to last item */
      if(last && flags & SUB_PANEL_SPACER1)    last->flags |= SUB_PANEL_SPACER2;
      if(last && flags & SUB_PANEL_SEPARATOR1) last->flags |= SUB_PANEL_SEPARATOR2;

      subRubyRelease(ary);
    }
} /* }}} */

/* RubyEvalConfig {{{ */
static void
RubyEvalConfig(void)
{
  int i;

  /* Update style values */
  subStyleUpdate();

  /* Check and update grabs */
  if(0 == subtle->grabs->ndata)
    {
      subSubtleLogWarn("Cannot find any grabs\n");
      subSubtleFinish();

      exit(-1);
    }
  else
    {
      subArraySort(subtle->grabs, subGrabCompare);

      /* Update grabs and gravites */
      for(i = 0; i < subtle->grabs->ndata; i++)
        {
          SubGrab *g = GRAB(subtle->grabs->data[i]);

          /* Update gravity grab */
          if(g->flags & SUB_GRAB_WINDOW_GRAVITY)
            {
              char *string = NULL;

              /* Create gravity string */
              RubyValueToGravityString(g->data.num, &string);
              subRubyRelease(g->data.num);
              g->data.string  = string;
              g->flags       &= ~SUB_RUBY_DATA;
            }
        }
    }

  /* Check gravities */
  if(0 == subtle->gravities->ndata)
    {
      subSubtleLogError("Cannot find any gravities\n");
      subSubtleFinish();

      exit(-1);
    }

  /* Get default gravity or use default (-1) */
  subtle->gravity = RubyValueToGravity(subtle->gravity);

  subGravityPublish();

  /* Check and update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Check if vid exists */
      if(0 > s->viewid || s->viewid >= subtle->views->ndata)
        s->viewid = 0;

      s->flags &= ~SUB_RUBY_DATA;
    }

  /* Check and update tags */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      /* Update gravities */
      if(t->flags & SUB_TAG_GRAVITY)
        {
          /* Disable tag gravity when gravity can't be found */
          if(-1 == (t->gravityid = RubyValueToGravity(t->gravityid)))
            t->flags &= ~SUB_TAG_GRAVITY;
        }

      /* Update screens */
      if(t->flags & SUB_CLIENT_MODE_STICK)
        {
          if(-1 != t->screenid &&
              (0 > t->screenid || subtle->screens->ndata <= t->screenid))
            t->flags &= ~SUB_CLIENT_MODE_STICK;
        }
    }

  subTagPublish();

  /* Check tag count */
  if(1 == subtle->tags->ndata)
    {
      subSubtleLogWarn("Cannot find any tags\n");
    }
  else if(32 < subtle->tags->ndata)
    subSubtleLogWarn("Cannot handle more than 32 tags\n");

  /* Check and update views */
  if(0 == subtle->views->ndata) ///< Create default view
    {
      SubView *v = subViewNew("subtle", "default");

      subArrayPush(subtle->views, (void *)v);
      subSubtleLogError("Cannot find any views\n");
    }
  else ///< Check default tag
    {
      SubView *v = NULL;

      /* Check for view with default tag */
      for(i = subtle->views->ndata - 1; 0 <= i; i--)
        if((v = VIEW(subtle->views->data[i])) && v->tags & DEFAULTTAG)
          {
            subSubtleLogDebugRuby("EvalConfig: default view=%s\n", v->name);
            break;
          }

      v->tags |= DEFAULTTAG; ///< Set default tag
    }

  subViewPublish();
  subDisplayPublish();
} /* }}} */

/* RubyEvalStyle {{{ */
static void
RubyEvalStyle(VALUE name,
  SubStyle *s,
  VALUE params)
{
  int bw = -1;
  long border = -1;
  VALUE value = Qnil;

  /* Special cases */
  if(CHAR2SYM("subtle") == name)
    {
      /* Get colors */
      RubyHashToColor(params, "background",   &s->bg);
      RubyHashToColor(params, "panel_top",    &s->top);
      RubyHashToColor(params, "panel_bottom", &s->bottom);
      RubyHashToColor(params, "stipple",      &s->fg);

      /* Set strut */
      RubyHashToSides(params, "strut",   &s->padding);
      RubyHashToSides(params, "padding", &s->padding);

      /* Set both panel colors */
      if(!NIL_P(value = rb_hash_lookup(params, CHAR2SYM("panel"))))
        {
          RubyHashToColor(params, "panel", &s->top);
          s->bottom = s->top;
        }
    }
  else if(CHAR2SYM("clients") == name)
    {
      /* We misuse some style values here:
       * border-top   <-> client border
       * border-right <-> title length
       * margin       <-> client gap */

      /* Set border color and width */
      RubyHashToBorder(params, "active",   &s->fg, &s->border.top);
      RubyHashToBorder(params, "inactive", &s->bg, &s->border.top);

      /* Get client margin */
      RubyHashToSides(params, "margin", &s->margin);

      /* FIXME: Set title width, but that should be a title style, right? */
      if(FIXNUM_P(value = rb_hash_lookup(params, CHAR2SYM("width"))))
        s->right = FIX2INT(value);
      else s->right = 50;
    }
  else
    {
      /* Get colors */
      RubyHashToColor(params, "foreground", &s->fg);
      RubyHashToColor(params, "background", &s->bg);
      RubyHashToColor(params, "icon",       &s->icon);

      /* Set all borders */
      RubyHashToBorder(params, "border", &border, &bw);

      /* Get borders */
      RubyHashToBorder(params, "border_top",    &s->top,    &s->border.top);
      RubyHashToBorder(params, "border_right",  &s->right,  &s->border.right);
      RubyHashToBorder(params, "border_bottom", &s->bottom, &s->border.bottom);
      RubyHashToBorder(params, "border_left",   &s->left,   &s->border.left);

      /* Apply catchall values */
      if(-1 != border) s->top = s->right = s->bottom = s->left = border;
      if(-1 != bw)
        {
          s->border.top = s->border.right =
            s->border.bottom = s->border.left = bw;
        }

      /* Get padding/margin */
      RubyHashToSides(params, "padding", &s->padding);
      RubyHashToSides(params, "margin",  &s->margin);

      /* Get min width */
      RubyHashToInt(params, "min_width", &s->min);
      s->min = MAX(0, s->min);

      /* Style font */
      if(T_STRING == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("font"))) && !s->font)
        {
          s->flags |= SUB_STYLE_FONT;
          s->font   = RubyFont(RSTRING_PTR(value));

          /* EWMH: Font */
          if(CHAR2SYM("all") == name)
            subEwmhSetString(ROOT, SUB_EWMH_SUBTLE_FONT, RSTRING_PTR(value));
      }

      /* Style separator */
      if(T_STRING == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("separator"))) && !s->separator)
        {
          s->flags |= SUB_STYLE_SEPARATOR;

          /* Create new separator */
          s->separator = (SubSeparator *)subSharedMemoryAlloc(1,
            sizeof(SubSeparator));
          s->separator->string  = strdup(RSTRING_PTR(value));
       }
   }
} /* }}} */

/* Foreach */

/* RubyForeachMatcher {{{ */
static int
RubyForeachMatcher(VALUE key,
  VALUE value,
  VALUE data)
{
  int type = 0;
  VALUE regex = Qnil, *rargs = (VALUE *)data;

  if(key == Qundef) return ST_CONTINUE;

  /* Check key value type */
  switch(rb_type(key))
    {
      case T_NIL:
        type = SUB_TAG_MATCH_INSTANCE|SUB_TAG_MATCH_CLASS; ///< Defaults
        break;
      case T_SYMBOL: RubySymbolToFlag(key, &type); break;
      case T_ARRAY:
          {
            int i;
            VALUE entry = Qnil;

            /* Check flags in array */
            for(i = 0; Qnil != (entry = rb_ary_entry(key, i)); i++)
              RubySymbolToFlag(entry, &type);
          }
        break;
      default: rb_raise(rb_eArgError, "Unknown value type");
    }

  /* Check value type */
  switch(rb_type(value))
    {
      case T_REGEXP:
        regex = rb_funcall(value, rb_intern("source"), 0, NULL);
        break;
      case T_SYMBOL: RubySymbolToFlag(value, &type); break;
      case T_ARRAY:
          {
            int i;
            VALUE entry = Qnil;

            /* Check flags in array */
            for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); i++)
              RubySymbolToFlag(entry, &type);
          }
        break;
      case T_STRING: regex = value; break;
      default: rb_raise(rb_eArgError, "Unknown value type");
    }

  /* Finally create regex if there is any and append additional flags */
  if(0 < type)
    {
      subTagMatcherAdd(TAG(rargs[0]), type,
        NIL_P(regex) ? NULL : RSTRING_PTR(regex), 0 < rargs[1]++);
    }

  return ST_CONTINUE;
} /* }}} */

/* RubyForeachStyle {{{ */
static int
RubyForeachStyle(VALUE key,
  VALUE value,
  VALUE data)
{
  SubStyle *s = (SubStyle *)data, *style = NULL;

  if(key == Qundef || NIL_P(value)) return ST_CONTINUE;

  /* Create new state */
  style = subStyleNew();
  style->name = strdup(SYM2CHAR(key));
  RubyEvalStyle(key, style, rb_iv_get(value, "@params"));

  /* Ease access to sub-styles */
  if(&subtle->styles.views == s)
    {
      if(CHAR2SYM("urgent")          == key) subtle->styles.urgent   = style;
      else if(CHAR2SYM("occupied")   == key) subtle->styles.occupied = style;
      else if(CHAR2SYM("focus")      == key) subtle->styles.focus    = style;
      else if(CHAR2SYM("visible")    == key) subtle->styles.visible  = style;
      else if(CHAR2SYM("separator")  == key) subtle->styles.viewsep  = style;
      else if(CHAR2SYM("unoccupied") == key)
        {
          subSubtleLogDeprecated("The `unoccupied' style is deprecated, "
            "please use the `views' style for normal views instead.\n");

          subStyleMerge(&subtle->styles.views, style);
          subStyleKill(style);

          return ST_CONTINUE;
        }
    }
  else if(&subtle->styles.sublets == s)
    if(CHAR2SYM("separator") == key) subtle->styles.subletsep = style;

  /* Finally add style */
  if(!s->styles) s->styles = subArrayNew();
  subArrayPush(s->styles, (void *)style);

  return ST_CONTINUE;
} /* }}} */

/* Wrap */

/* RubyWrapLoadSubtlext {{{ */
static VALUE
RubyWrapLoadSubtlext(VALUE data)
{
  return rb_require("subtle/subtlext");
}/* }}} */

/* RubyWrapLoadPanels {{{ */
static VALUE
RubyWrapLoadPanels(VALUE data)
{
  int i;

  /* Pass 1: Load actual panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(!s->panels) s->panels = subArrayNew();

      RubyEvalPanel(s->top,    SUB_SCREEN_PANEL1, s);
      RubyEvalPanel(s->bottom, SUB_SCREEN_PANEL2, s);
    }

  /* Pass 2: Add remaining sublets if any */
  if(0 < subtle->sublets->ndata)
    {
      int j, k, pos;

      for(i = 0; i < subtle->screens->ndata; i++)
        {
          SubScreen *s = SCREEN(subtle->screens->data[i]);
          Window panel = s->panel1;

          for(j = 0; j < s->panels->ndata; j++)
            {
              SubPanel *p = PANEL(s->panels->data[j]);

              if(panel != s->panel2 && p->flags & SUB_PANEL_BOTTOM)
                panel = s->panel2;

              /* Find dummy panel */
              if(p->flags & SUB_PANEL_SUBLETS)
                {
                  SubPanel *sublet = NULL;

                  pos = j;

                  subArrayRemove(s->panels, (void *)p);

                  /* Find sublets not on any panel so far */
                  for(k = 0; k < subtle->sublets->ndata; k++)
                    {
                      sublet = PANEL(subtle->sublets->data[k]);

                      /* Check if screen is empty */
                      if(NULL == sublet->screen)
                        {
                          sublet->flags  |= SUB_PANEL_SEPARATOR2;
                          sublet->screen  = s;

                          /* Mark for bottom panel */
                          if(p->flags & SUB_PANEL_BOTTOM)
                            sublet->flags |= SUB_PANEL_BOTTOM;

                          subArrayInsert(s->panels, pos++, (void *)sublet);
                        }
                    }

                  /* Check spacers and separators in first and last sublet */
                  if((sublet = PANEL(subArrayGet(s->panels, j))))
                    sublet->flags |= (p->flags & (SUB_PANEL_BOTTOM|
                      SUB_PANEL_SPACER1|SUB_PANEL_SEPARATOR1|SUB_PANEL_CENTER));

                  if((sublet = PANEL(subArrayGet(s->panels, pos - 1))))
                    {
                      sublet->flags |= (p->flags & SUB_PANEL_SPACER2);

                      if(!(p->flags & SUB_PANEL_SEPARATOR2))
                        sublet->flags &= ~SUB_PANEL_SEPARATOR2;
                    }

                  free(p);

                  break;
                }
            }
        }

      /* Unloaded non-visible sublets */
      for(i = 0; i < subtle->sublets->ndata; i++)
        {
          SubPanel *p = PANEL(subtle->sublets->data[i]);

          if(p->flags & SUB_PANEL_SUBLET && !p->screen)
            subRubyUnloadSublet(p);
        }

      /* Finally sort sublets */
      subArraySort(subtle->sublets, subPanelCompare);
    }

  return Qnil;
} /* }}} */

/* RubyWrapCall {{{ */
static VALUE
RubyWrapCall(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  /* Check call type */
  switch((int)rargs[0])
    {
      case SUB_CALL_CONFIGURE: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__configure"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_RUN: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__run"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_DATA: /* {{{ */
          {
            int nlist = 0;
            char **list = NULL;
            Atom prop = subEwmhGet(SUB_EWMH_SUBTLE_DATA);
            VALUE meth = rb_intern("__data"), str = Qnil;

            /* Fetch data or create empty string */
            if((list = subSharedPropertyGetStrings(subtle->dpy, ROOT,
                prop, &nlist)))
              {
                if(list && 0 < nlist)
                  str = rb_str_new2(list[0]);

                XFreeStringList(list);
              }
            else str = rb_str_new2("");

            subSharedPropertyDelete(subtle->dpy, ROOT, prop);

            /* Finally call method */
            rb_funcall(rargs[1], meth,
              MINMAX(rb_obj_method_arity(rargs[1], meth), 1, 2),
              rargs[1], str);
          }
        break; /* }}} */
      case SUB_CALL_WATCH: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__watch"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_DOWN: /* {{{ */
          {
            int *args = (int *)rargs[2];
            VALUE meth = rb_intern("__down");

            rb_funcall(rargs[1], meth,
              MINMAX(rb_obj_method_arity(rargs[1], meth), 1, 4),
              rargs[1], INT2FIX(args[0]), INT2FIX(args[1]), INT2FIX(args[2]));
          }
        break; /* }}} */
      case SUB_CALL_OVER: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__over"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_OUT: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__out"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_UNLOAD: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__unload"), 1, rargs[1]);
        break; /* }}} */
      default: /* {{{ */
        /* Call instance methods or just a proc */
        if(rb_obj_is_instance_of(rargs[1], rb_cMethod))
          {
            int arity = 0;
            VALUE receiver = Qnil;

            /* Get arity */
            receiver = rb_funcall(rargs[1], rb_intern("receiver"), 0, NULL);
            arity    = FIX2INT(rb_funcall(rargs[1], rb_intern("arity"),
              0, NULL));
            arity    = -1 == arity ? 2 : MINMAX(arity, 1, 2);

            rb_funcall(rargs[1], rb_intern("call"), arity, receiver,
              RubySubtleToSubtlext((VALUE *)rargs[2]));

            subScreenUpdate();
            subScreenRender();
          }
        else
          {
            rb_funcall(rargs[1], rb_intern("call"),
              MINMAX(rb_proc_arity(rargs[1]), 0, 1),
              RubySubtleToSubtlext((VALUE *)rargs[2]));
          }
        break; /* }}} */
    }

  return Qnil;
} /* }}} */

/* RubyWrapRelease {{{ */
static VALUE
RubyWrapRelease(VALUE value)
{
  /* Relase value from shelter */
  if(Qtrue == rb_ary_includes(shelter, value))
    rb_ary_delete(shelter, value);

  return Qnil;
} /* }}} */

/* RubyWrapEvalFile {{{ */
static VALUE
RubyWrapEvalFile(VALUE data)
{
  VALUE *rargs = (VALUE *)data, rargs2[3] = { Qnil };

  /* Wrap data */
  rargs2[0] = rb_funcall(rb_cFile, rb_intern("read"), 1, rargs[0]);
  rargs2[1] = rargs[0];
  rargs2[2] = rargs[1];

  rb_obj_instance_eval(2, rargs2, rargs[1]);

  return Qnil;
} /* }}} */

/* RubyWrapSubletConfig {{{ */
static VALUE
RubyWrapSubletConfig(VALUE data)
{
  VALUE *rargs = (VALUE *)data, hash = Qnil;
  SubSublet *s = SUBLET(rargs[1]);

  /* Check if config hash exists */
  if(T_HASH == rb_type(hash = rb_hash_lookup(config_sublets, rargs[0])))
    {
      VALUE value = Qnil;

      /* Set sublet interval */
      if(FIXNUM_P(value = rb_hash_lookup(hash, CHAR2SYM("interval"))))
        s->interval = FIX2INT(value);

      /* Set sublet style */
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(hash,
          CHAR2SYM("style"))))
        {
          value = rb_sym_to_s(value);

          subStyleFind(&subtle->styles.sublets, RSTRING_PTR(value),
            &s->styleid);
        }
    }

  /* Check if there is a matching style */
  subStyleFind(&subtle->styles.sublets, s->name, &s->styleid);

  return Qnil;
} /* }}} */

/* Object */

/* RubyObjectDispatcher {{{ */
/*
 * Dispatcher for Subtlext constants - internal use only
 */

static VALUE
RubyObjectDispatcher(VALUE self,
  VALUE missing)
{
  VALUE ret = Qnil;

  /* Load subtlext on demand */
  if(CHAR2SYM("Subtlext") == missing)
    {
      int state = 0;
      ID id = SYM2ID(missing);

      /* Carefully load sublext */
      rb_protect(RubyWrapLoadSubtlext, Qnil, &state);
      if(state)
        {
          subSubtleLogWarn("Cannot load subtlext\n");
          RubyBacktrace();
          subSubtleFinish();

          exit(-1);
        }

      ret = rb_const_get(rb_mKernel, id);
    }

  return ret;
} /* }}} */

/* Options */

/* RubyOptionsInit {{{ */
/*
 * call-seq: init -> Subtle::Options
 *
 * Create a new Options object
 *
 *  options = Subtle::Options.new
 *  => #<Subtle::Options:xxx>
 */

static VALUE
RubyOptionsInit(VALUE self)
{
  rb_iv_set(self, "@params", rb_hash_new());

  return Qnil;
} /* }}} */

/* RubyOptionsDispatcher {{{ */
/*
 * Dispatcher for DSL proc methods - internal use only
 */

static VALUE
RubyOptionsDispatcher(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE ret = Qnil, missing = Qnil, args[4] = { Qnil };

  rb_scan_args(argc, argv, "23", &missing,
    &args[0], &args[1], &args[2], &args[3]);

  /* Check whether missing is included in methods array
   * and dispatch to config or just store param*/
  if(RTEST(config_methods) && rb_ary_includes(config_methods, missing))
    {
      ret = rb_funcall2(config_instance, rb_to_id(missing), --argc, ++argv);
    }
  else
    {
      VALUE arg = Qnil;

      /* Convert multiple argument to one array */
      if(2 < argc)
        {
          int i;

          /* Move args into array */
          arg = rb_ary_new();

          for(i = 0; i < LENGTH(args); i++)
            if(!NIL_P(args[i])) rb_ary_push(arg, args[i]);
        }
      else arg = args[0];

      ret = rb_hash_aset(rb_iv_get(self, "@params"), missing, arg);
    }

  return ret;
} /* }}} */

/* RubyOptionsMatch {{{ */
/*
 * call-seq: match -> nil
 *
 * Append match hashes if called multiple times
 *
 *  option.match :name => /foo/
 *  => nil
 */

static VALUE
RubyOptionsMatch(VALUE self,
  VALUE value)
{
  VALUE params = Qnil, ary = Qnil, hash = Qnil, sym = Qnil;

  /* Get params and convert value to hash */
  params = rb_iv_get(self, "@params");
  hash   = RubyValueToHash(value);
  sym    = CHAR2SYM("match");

  /* Check if hash contains key and add or otherwise create it */
  if(T_ARRAY != rb_type(ary = rb_hash_lookup(params, sym)))
    {
      ary = rb_ary_new();

      rb_hash_aset(params, sym, ary);
    }

  /* Finally add value to array */
  rb_ary_push(ary, hash);

  return Qnil;
} /* }}} */

/* RubyOptionsGravity {{{ */
/*
 * call-seq: gravity -> nil
 *
 * Overwrite global gravity method
 *
 *  option.gravity :center
 *  => nil
 */

static VALUE
RubyOptionsGravity(VALUE self,
  VALUE gravity)
{
  VALUE params = rb_iv_get(self, "@params");

  /* Just store param */
  return rb_hash_aset(params, CHAR2SYM("gravity"), gravity);
} /* }}} */

/* RubyOptionsStyle {{{ */
/*
 * call-seq: style(name) -> nil
 *
 * Overwrite global style method
 *
 *  option.style :urgent do
 *    foreground "#fecf35"
 *  end
 *  => nil
 */

static VALUE
RubyOptionsStyle(VALUE self,
  VALUE name)
{
  /* Check if block is given */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil, styles = rb_iv_get(self, "@styles");

      /* Create styles hash if necessary */
      if(NIL_P(styles))
        {
          styles = rb_hash_new();
          rb_iv_set(self, "@styles", styles);
        }

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 1, self);
      rb_obj_instance_eval(0, 0, options);

      rb_hash_aset(styles, name, options);
    }
  else
    {
      VALUE params = rb_iv_get(self, "@params");

      /* Just append to params */
      rb_hash_aset(params, CHAR2SYM("style"), name);
    }

  return Qnil;
} /* }}} */

/* RubyOptionsOnMatch {{{ */
/*
 * call-seq: on_match(, blk) -> nil
 *
 * Add a tag on match proc
 *
 *  tag "test" do
 *    match "foobar"
 *
 *    on_match do |c|
 *      c.gravity = :foobar
 *    end
 *  end
 */

static VALUE
RubyOptionsOnMatch(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE value = Qnil;

  rb_scan_args(argc, argv, "01", &value);

  if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

  if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

  rb_hash_aset(rb_iv_get(self, "@params"),
    CHAR2SYM("on_match"), value);

  return Qnil;
} /* }}} */

/* Config */

/* RubyConfigMissing {{{ */
/*
 * Check error of missing methods
 */

static VALUE
RubyConfigMissing(int argc,
  VALUE *argv,
  VALUE self)
{
  char *name = NULL;
  VALUE missing = Qnil, args = Qnil;

  rb_scan_args(argc, argv, "1*", &missing, &args);

  name = (char *)rb_id2name(SYM2ID(missing));

  subSubtleLogWarn("Cannot find method `%s'\n", name);

  return Qnil;
} /* }}} */

/* RubyConfigAdded {{{ */
/*
 * Updated methods list when singleton methods are added
 */

static VALUE
RubyConfigAdded(VALUE self,
  VALUE name)
{
  /* Append method name to methods list */
  rb_ary_push(config_methods, name);

  subSubtleLogDebugRuby("Added: singleton method=`%s'\n", SYM2CHAR(name));

  return Qnil;
} /* }}} */

/* RubyConfigSet {{{ */
/*
 * call-seq: set(option, value) -> nil
 *
 * Set options
 *
 *  set :urgent, true
 */

static VALUE
RubyConfigSet(VALUE self,
  VALUE option,
  VALUE value)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(option))
    {
      switch(rb_type(value))
        {
          case T_FIXNUM: /* {{{ */
            if(CHAR2SYM("step") == option ||
                CHAR2SYM("increase_step") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->step = FIX2INT(value);
              }
            else if(CHAR2SYM("snap") == option ||
                CHAR2SYM("border_snap") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->snap = FIX2INT(value);
              }
            else if(CHAR2SYM("gravity") == option ||
                CHAR2SYM("default_gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else subSubtleLogWarn("Unknown option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          case T_SYMBOL: /* {{{ */
            if(CHAR2SYM("gravity") == option ||
                CHAR2SYM("default_gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else subSubtleLogWarn("Unknown option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          case T_TRUE:
          case T_FALSE: /* {{{ */
            if(CHAR2SYM("urgent") == option ||
                CHAR2SYM("urgent_dialogs") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_URGENT;
              }
            else if(CHAR2SYM("resize") == option ||
                CHAR2SYM("honor_size_hints") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_RESIZE;
              }
            else if(CHAR2SYM("tiling") == option ||
                CHAR2SYM("gravity_tiling") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_TILING;
              }
            else if(CHAR2SYM("click_to_focus") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_FOCUS_CLICK;
              }
            else if(CHAR2SYM("skip_pointer_warp") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_SKIP_WARP;
              }
            else if(CHAR2SYM("skip_urgent_warp") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_SKIP_URGENT_WARP;
              }
            else subSubtleLogWarn("Unknown option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          case T_STRING: /* {{{ */
            if(CHAR2SYM("wmname") == option)
              {
                /* Set support window to root (broken Java)
                 * and update WM_NAME */
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    Window root = ROOT;

                    subEwmhSetWindows(ROOT,
                      SUB_EWMH_NET_SUPPORTING_WM_CHECK, &root, 1);
                    subEwmhSetString(root, SUB_EWMH_NET_WM_NAME,
                      RSTRING_PTR(value));
                  }
              }
            else subSubtleLogWarn("Unknown option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          default:
            rb_raise(rb_eArgError, "Unexpected value type for option `%s'",
              SYM2CHAR(option));
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value type for set");

  return Qnil;
} /* }}} */

/* RubyConfigGravity {{{ */
/*
 * call-seq: gravity(name, value, tile) -> nil
 *
 * Create gravity and optionally enable tiling, either
 * horizonally (:horz) or vertically. (:vert)
 *
 *  gravity :top_left, [0, 0, 50, 50]
 *
 *  gravity :top_left, [0, 0, 50, 50], :vert
 */

static VALUE
RubyConfigGravity(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE name = Qnil, value = Qnil, tile = Qnil;

  rb_scan_args(argc, argv, "21", &name, &value, &tile);

  /* Check value type */
  if(T_SYMBOL == rb_type(name) && T_ARRAY == rb_type(value))
    {
      XRectangle geometry = { 0 };

      RubyArrayToGeometry(value, &geometry);

      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubGravity *g = NULL;

          /* Finally create new gravity */
          if((g = subGravityNew(SYM2CHAR(name), &geometry)))
            {
              /* Tile just clients with this gravity */
              if(T_SYMBOL == rb_type(tile))
                {
                  if(CHAR2SYM("horz")      == tile) g->flags |= SUB_GRAVITY_HORZ;
                  else if(CHAR2SYM("vert") == tile) g->flags |= SUB_GRAVITY_VERT;
                }

              subArrayPush(subtle->gravities, (void *)g);
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for gravity");

  return Qnil;
} /* }}} */

/* RubyConfigGrab {{{ */
/*
 * call-seq: grab(chain, value) -> nil
 *           grab(chain, &blk)  -> nil
 *
 * Create grabs
 *
 *  grab "A-F1", :ViewJump1
 */

static VALUE
RubyConfigGrab(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE chain = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &chain, &value);

  if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

  RubyEvalGrab(chain, value);

  return Qnil;
} /* }}} */

/* RubyConfigTag {{{ */
/*
 * call-seq: tag(name, regex) -> nil
 *           tag(name, blk)   -> nil
 *
 * Add a new tag
 *
 *  tag "foobar", "regex"
 *
 *  tag "foobar" do
 *    regex = "foobar"
 *  end
 */

static VALUE
RubyConfigTag(int argc,
  VALUE *argv,
  VALUE self)
{
  int flags = 0, screenid = -1;
  unsigned long gravityid = 0;
  XRectangle geom = { 0 };
  VALUE name = Qnil, match = Qnil, params = Qnil, value = Qnil, proc = Qnil;

  rb_scan_args(argc, argv, "11", &name, &match);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil;

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 1, self);
      rb_obj_instance_eval(0, 0, options);
      params = rb_iv_get(options, "@params");

      /* Check matcher */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("match"))))
        match = value; ///< Lazy eval

      /* Set gravity */
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("gravity"))) || T_FIXNUM == rb_type(value) ||
          T_ARRAY == rb_type(value))
        {
          flags     |= SUB_TAG_GRAVITY;
          gravityid  = value; ///< Lazy eval
        }

      /* Set geometry */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("geometry"))))
        {
          flags |= SUB_TAG_GEOMETRY;
          RubyArrayToGeometry(value, &geom);
        }

      /* Set geometry */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("position"))))
        {
          flags |= SUB_TAG_POSITION;
          RubyArrayToGeometry(value, &geom);
        }

      /* Set window type */
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("type"))))
        {
          /* Check type */
          if(CHAR2SYM("normal")       == value) flags = SUB_CLIENT_TYPE_NORMAL;
          else if(CHAR2SYM("desktop") == value) flags = SUB_CLIENT_TYPE_DESKTOP;
          else if(CHAR2SYM("dock")    == value) flags = SUB_CLIENT_TYPE_DOCK;
          else if(CHAR2SYM("toolbar") == value) flags = SUB_CLIENT_TYPE_TOOLBAR;
          else if(CHAR2SYM("splash")  == value) flags = SUB_CLIENT_TYPE_SPLASH;
          else if(CHAR2SYM("dialog")  == value) flags = SUB_CLIENT_TYPE_DIALOG;
        }

      /* Check state properties */
      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("borderless")))) flags |= SUB_CLIENT_MODE_BORDERLESS;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("center")))) flags |= SUB_CLIENT_MODE_CENTER;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("fixed")))) flags |= SUB_CLIENT_MODE_FIXED;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("float")))) flags |= SUB_CLIENT_MODE_FLOAT;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("full")))) flags |= SUB_CLIENT_MODE_FULL;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("resize")))) flags |= SUB_CLIENT_MODE_RESIZE;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("urgent")))) flags |= SUB_CLIENT_MODE_URGENT;

      if(Qtrue == (value = rb_hash_lookup(params,
        CHAR2SYM("zaphod")))) flags |= SUB_CLIENT_MODE_ZAPHOD;

      /* Set stick screen */
      if(RTEST(value = rb_hash_lookup(params, CHAR2SYM("stick"))))
        {
          /* Either screen id or just true */
          if(FIXNUM_P(value))
            {
              screenid  = FIX2INT(value);
              flags    |= SUB_CLIENT_MODE_STICK;
            }
          else if(Qtrue == value) flags |= SUB_CLIENT_MODE_STICK;
        }

      /* Set match proc */
      if(RTEST(value = rb_hash_lookup(params, CHAR2SYM("on_match"))))
        {
          proc   = value;
          flags |= SUB_TAG_PROC;

          rb_ary_push(shelter, proc); ///< Protect from GC
        }
    }

  /* Check value type */
  if(T_STRING == rb_type(name))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          int duplicate = False;
          SubTag *t = NULL;

          /* Finally create and add new tag if no duplicate */
          if((t = subTagNew(RSTRING_PTR(name), &duplicate)) &&
              False == duplicate)
            {
              int i;
              VALUE entry = Qnil, rargs[2] = { 0 };

              /* Set tag values */
              t->flags     |= flags;
              t->gravityid = gravityid;
              t->screenid  = screenid;
              t->geom      = geom;
              t->proc      = proc;

              /* Add matcher */
              rargs[0] = (VALUE)t;
              switch(rb_type(match))
                {
                  case T_ARRAY:
                    for(i = 0; T_HASH == rb_type(entry =
                        rb_ary_entry(match, i)); i++)
                      {
                        rargs[1] = 0; ///< Reset matcher count
                        rb_hash_foreach(entry, RubyForeachMatcher, (VALUE)&rargs);
                      }
                    break;
                  case T_REGEXP:
                  case T_STRING:
                    RubyForeachMatcher(Qnil, match, (VALUE)&rargs);
                }

              subArrayPush(subtle->tags, (void *)t);
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for tag");

  return Qnil;
} /* }}} */

/* RubyConfigView {{{ */
/*
 * call-seq: view(name, regex) -> nil
 *
 * Add a new view
 *
 *  view "foobar", "regex"
 */

static VALUE
RubyConfigView(int argc,
  VALUE *argv,
  VALUE self)
{
  int flags = 0;
  VALUE name = Qnil, match = Qnil, params = Qnil, value = Qnil, icon = value;

  rb_scan_args(argc, argv, "11", &name, &match);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil;

      /* Collect options */
      klass    = rb_const_get(mod, rb_intern("Options"));
      options  = rb_funcall(klass, rb_intern("new"), 1, self);
      rb_obj_instance_eval(0, 0, options);
      params  = rb_iv_get(options, "@params");

      /* Check match */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("match"))))
        match = rb_hash_lookup(rb_ary_entry(value, 0), Qnil); ///< Lazy eval

      /* Check dynamic */
      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("dynamic"))))
        flags |= SUB_VIEW_DYNAMIC;

      /* Check icon only */
      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("icon_only"))))
        flags |= SUB_VIEW_ICON_ONLY;

      /* Check icon */
      icon = RubyValueToIcon(rb_hash_lookup(params, CHAR2SYM("icon")));
    }

  /* Check value type */
  if(T_STRING == rb_type(name))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubView *v = NULL;
          char *re = NULL;

          /* Convert type */
          switch(rb_type(match))
            {
              case T_REGEXP:
                match = rb_funcall(match, rb_intern("source"), 0, NULL);
              case T_STRING:
                re = RSTRING_PTR(match);
                break;
            }

          /* Finally create new view */
          if((v = subViewNew(RSTRING_PTR(name), re)))
            {
              v->flags |= flags;

              subArrayPush(subtle->views, (void *)v);

              /* Add icon */
              if(!NIL_P(icon))
                {
                  v->flags |= SUB_VIEW_ICON;
                  v->icon   = ICON(subSharedMemoryAlloc(1, sizeof(SubIcon)));

                  RubyIconToIcon(icon, v->icon);

                  rb_ary_push(shelter, icon); ///< Protect from GC
                }
              else v->flags &= ~SUB_VIEW_ICON_ONLY;
            }
       }
    }
  else rb_raise(rb_eArgError, "Unknown value type for view");

  return Qnil;
} /* }}} */

/* RubyConfigOn {{{ */
/*
 * call-seq: on(event, &block) -> nil
 *
 * Event block for hooks
 *
 *  on :event do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubyConfigOn(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE event = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &event, &value);

  /* Check value type */
  if(T_SYMBOL == rb_type(event))
    {
      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

      RubyEvalHook(event, value);
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
} /* }}} */

/* RubyConfigSublet {{{ */
/*
 * call-seq: sublet(name, blk)   -> nil
 *
 * Configure a sublet
 *
 *  sublet :jdownloader do
 *    interval 20
 *  end
 */

static VALUE
RubyConfigSublet(VALUE self,
  VALUE sublet)
{
  VALUE klass = Qnil, options = Qnil;

  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(sublet))
    {
      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 1, self);
      rb_obj_instance_eval(0, 0, options);

      /* Clone to get rid of object instance and store it */
      rb_hash_aset(config_sublets, sublet,
        rb_obj_clone(rb_iv_get(options, "@params")));
    }
  else rb_raise(rb_eArgError, "Unknown value type for sublet");

  return Qnil;
} /* }}} */

/* RubyConfigScreen {{{ */
/*
 * call-seq: screen(name, blk) -> nil
 *
 * Set options for screen
 *
 *  screen 1 do
 *    stipple  false
 *    top      []
 *    bottom   []
 *  end
 */

static VALUE
RubyConfigScreen(VALUE self,
  VALUE id)
{
  VALUE params = Qnil, value = Qnil, klass = Qnil, options = Qnil;
  SubScreen *s = NULL;

  /* FIXME: rb_need_block() */
  if(!rb_block_given_p()) return Qnil;

  /* Collect options */
  klass   = rb_const_get(mod, rb_intern("Options"));
  options = rb_funcall(klass, rb_intern("new"), 1, self);
  rb_obj_instance_eval(0, 0, options);
  params = rb_iv_get(options, "@params");

  /* Check value type */
  if(FIXNUM_P(id))
    {
      int flags = 0, vid = -1;
      Pixmap stipple = None;
      VALUE top = Qnil, bottom = Qnil;

      /* Get options */
      if(T_HASH == rb_type(params))
        {
          if(!NIL_P(value = RubyValueToIcon(rb_hash_lookup(params,
              CHAR2SYM("stipple")))))
            {
              flags   |= SUB_SCREEN_STIPPLE;
              stipple  = NUM2LONG(rb_iv_get(value, "@pixmap"));
            }
          if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("top"))))
            {
              top = value; /// Lazy eval
              rb_ary_push(shelter, value); ///< Protect from GC
            }
          if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("bottom"))))
            {
              bottom = value; ///< Lazy eval
              rb_ary_push(shelter, value); ///< Protect from GC
            }
          if(T_FIXNUM == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("view"))))
            vid = FIX2INT(value);
        }

      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          if((s = subArrayGet(subtle->screens, FIX2INT(id) - 1)))
            {
              s->flags   |= (flags|SUB_RUBY_DATA);
              s->top      = top;
              s->bottom   = bottom;
              s->stipple  = stipple;

              if(-1 != vid) s->viewid = vid;
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for screen");

  return Qnil;
} /* }}} */

/* RubyConfigStyle {{{ */
/*
 * call-seq: style(name, blk)   -> nil
 *
 * Add style values
 *
 *  style :title do
 *    foreground "#fecf35"
 *    background "#202020"
 *  end
 */

static VALUE
RubyConfigStyle(VALUE self,
  VALUE name)
{
  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(name))
    {
      SubStyle *s = NULL;
      VALUE klass = Qnil, options = Qnil, styles = Qnil;

      /* Select style struct */
      if(CHAR2SYM("all")            == name) s = &subtle->styles.all;
      else if(CHAR2SYM("views")     == name) s = &subtle->styles.views;
      else if(CHAR2SYM("title")     == name) s = &subtle->styles.title;
      else if(CHAR2SYM("sublets")   == name) s = &subtle->styles.sublets;
      else if(CHAR2SYM("separator") == name) s = &subtle->styles.separator;
      else if(CHAR2SYM("clients")   == name) s = &subtle->styles.clients;
      else if(CHAR2SYM("subtle")    == name) s = &subtle->styles.subtle;
      else
        {
          subSubtleLogWarn("Unexpected style name `:%s'\n", SYM2CHAR(name));

          return Qnil;
        }

      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 1, self);
      rb_obj_instance_eval(0, 0, options);

      /* Eval style before styles */
      RubyEvalStyle(name, s, rb_iv_get(options, "@params"));

      /* Eval styles */
      if(T_HASH == rb_type((styles = rb_iv_get(options, "@styles"))))
        rb_hash_foreach(styles, RubyForeachStyle, (VALUE)s);

    }
  else rb_raise(rb_eArgError, "Unexpected value type for style `%s'",
    rb_obj_classname(name));

  return Qnil;
} /* }}} */

/* RubyConfigLoadConfig {{{ */
/*
 * call-seq: load_config(file) -> true or false
 *
 * Load config file
 *
 *  load_config "gravities.rb"
 *  => true
 */

static VALUE
RubyConfigLoadConfig(VALUE self,
  VALUE file)
{
  int state = 0;
  char buf[100] = { 0 };
  VALUE rargs[2] = { Qnil };

  /* Check if file exists otherwise try to find it */
  if(-1 == access(RSTRING_PTR(file), R_OK))
    {
      int len = 0;
      char *home = NULL, *dirs = NULL, *tok = NULL,
        tokens[200] = { 0 }, *tokensp = tokens;

      /* Combine XDG paths */
      if((home = getenv("XDG_CONFIG_HOME")))
        len += snprintf(tokens, sizeof(tokens), "%s", home);
      else len += snprintf(tokens, sizeof(tokens), "%s/.config",
        getenv("HOME"));

      if((dirs = getenv("XDG_CONFIG_DIRS")))
        len += snprintf(tokens + len, sizeof(tokens), ":%s", dirs);
      else len += snprintf(tokens + len, sizeof(tokens), ":%s/%s",
        "/etc/xdg", PKG_NAME);

      if((home = getenv("XDG_DATA_HOME")))
        {
          snprintf(buf, sizeof(buf), "%s:%s/%s/sublets",
            tokens, home, PKG_NAME);
        }
      else snprintf(buf, sizeof(buf), "%s:%s/.local/share/%s/sublets",
        tokens, getenv("HOME"), PKG_NAME);

      /* Search file in XDG paths */
      while((tok = strsep(&tokensp, ":")))
        {
          /* Check if config file exists in tok or tok/subtle */
          snprintf(buf, sizeof(buf), "%s/%s", tok, RSTRING_PTR(file));

          if(-1 != access(buf, R_OK)) break;
          else
            {
              snprintf(buf, sizeof(buf), "%s/%s/%s",
                tok, PKG_NAME, RSTRING_PTR(file));

              if(-1 != access(buf, R_OK)) break;
            }
        }
    }
  else snprintf(buf, sizeof(buf), "%s", RSTRING_PTR(file));

  printf("Reading file `%s'\n", buf);

  /* Carefully load and eval file */
  rargs[0] = rb_str_new2(buf);
  rargs[1] = self;

  rb_protect(RubyWrapEvalFile, (VALUE)&rargs, &state);
  if(state)
    {
      subSubtleLogWarn("Cannot load file `%s'\n", buf);
      RubyBacktrace();

      return Qfalse;
    }

  return Qtrue;
} /* }}} */

/* Sublet */

/* RubySubletDispatcher {{{ */
/*
 * Dispatcher for Sublet instance variables - internal use only
 */

static VALUE
RubySubletDispatcher(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE missing = Qnil, args = Qnil, ret = Qnil;

  rb_scan_args(argc, argv, "1*", &missing, &args);

  /* Dispatch calls */
  if(rb_respond_to(self, rb_to_id(missing))) ///< Intance methods
    ret = rb_funcall2(self, rb_to_id(missing), --argc, ++argv);
  else ///< Instance variables
    {
      char buf[20] = { 0 };
      char *name = (char *)rb_id2name(SYM2ID(missing));

      snprintf(buf, sizeof(buf), "@%s", name);

      if(index(name, '='))
        {
          int len = 0;
          VALUE value = Qnil;

          value    = rb_ary_entry(args, 0); ///< Get first arg
          len      = strlen(name);
          buf[len] = '\0'; ///< Overwrite equal sign

          rb_funcall(self, rb_intern("instance_variable_set"), 2, CHAR2SYM(buf), value);
        }
      else ret = rb_funcall(self, rb_intern("instance_variable_get"), 1, CHAR2SYM(buf));
    }

  return ret;
} /* }}} */

/* RubySubletConfig {{{ */
/*
 * call-seq: config -> Hash
 *
 * Get config hash from config
 *
 *  s.config
 *  => { :interval => 30 }
 */

static VALUE
RubySubletConfig(VALUE self)
{
  SubPanel *p = NULL;
  VALUE hash = Qnil;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Get config hash */
      if(NIL_P(hash = rb_hash_lookup(config_sublets, CHAR2SYM(p->sublet->name))))
        hash = rb_hash_new();
    }

  return hash;
} /* }}} */

/* RubySubletConfigure {{{ */
/*
 * call-seq: configure -> nil
 *
 * Configure block for Sublet
 *
 *  configure :sublet do |s|
 *    s.interval = 60
 *  end
 */

static VALUE
RubySubletConfigure(VALUE self,
  VALUE name)
{
  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(name))
    {
      SubPanel *p = NULL;

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          VALUE proc = Qnil;

          /* Assume latest sublet */
          proc            = rb_block_proc();
          p->sublet->name = strdup(SYM2CHAR(name));

          /* Define configure method */
          rb_funcall(rb_singleton_class(p->sublet->instance),
            rb_intern("define_method"),
            2, CHAR2SYM("__configure"), proc);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for configure");

  return Qnil;
} /* }}} */

/* RubySubletHelper {{{ */
/*
 * call-seq: helper -> nil
 *
 * Helper block for Sublet
 *
 *  helper do |s|
 *    def test
 *      puts "test"
 *    end
 *  end
 */

static VALUE
RubySubletHelper(VALUE self)
{
  SubPanel *p = NULL;

  rb_need_block();

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Instance eval the block */
      rb_yield_values(1, p->sublet->instance);
      rb_obj_instance_eval(0, 0, p->sublet->instance);
    }

  return Qnil;
} /* }}} */

/* RubySubletOn {{{ */
/*
 * call-seq: on(event, &block) -> nil
 *
 * Event block for hooks
 *
 *  on :event do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubySubletOn(VALUE self,
  VALUE event)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(event) && rb_block_given_p())
    {
      SubPanel *p = NULL;

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          char buf[64] = { 0 };
          int i, arity = 0;
          VALUE proc = Qnil, sing = Qnil, meth = Qnil;

          RubyMethods methods[] =
          {
            { CHAR2SYM("run"),        CHAR2SYM("__run"),    SUB_SUBLET_RUN,    1 },
            { CHAR2SYM("data"),       CHAR2SYM("__data"),   SUB_SUBLET_DATA,   2 },
            { CHAR2SYM("watch"),      CHAR2SYM("__watch"),  SUB_SUBLET_WATCH,  1 },
            { CHAR2SYM("unload"),     CHAR2SYM("__unload"), SUB_SUBLET_UNLOAD, 1 },
            { CHAR2SYM("mouse_down"), CHAR2SYM("__down"),   SUB_PANEL_DOWN,    4 },
            { CHAR2SYM("mouse_over"), CHAR2SYM("__over"),   SUB_PANEL_OVER,    1 },
            { CHAR2SYM("mouse_out"),  CHAR2SYM("__out"),    SUB_PANEL_OUT,     1 }
          };

          /* Collect stuff */
          proc  = rb_block_proc();
          arity = rb_proc_arity(proc);
          sing  = rb_singleton_class(p->sublet->instance);
          meth  = rb_intern("define_method");

          /* Special hooks */
          for(i = 0; LENGTH(methods) > i; i++)
            {
              if(methods[i].sym == event)
                {
                  /* Check proc arity */
                  if(-1 == arity || (1 <= arity && methods[i].arity >= arity))
                    {
                      /* Add flags */
                      if(methods[i].flags & (SUB_PANEL_DOWN|
                          SUB_PANEL_OVER|SUB_PANEL_OUT))
                        p->flags |= methods[i].flags;
                      else p->sublet->flags |= methods[i].flags;

                      /* Create instance method from proc */
                      rb_funcall(sing, meth, 2, methods[i].real, proc);

                      return Qnil;
                    }
                  else rb_raise(rb_eArgError, "Wrong number of arguments (%d for %d)",
                    arity, methods[i].arity);
               }
            }

          /* Generic hooks */
          snprintf(buf, sizeof(buf), "__hook_%s", SYM2CHAR(event));
          rb_funcall(sing, meth, 2, CHAR2SYM(buf), proc);

          RubyEvalHook(event, rb_obj_method(p->sublet->instance,
            CHAR2SYM(buf)));
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
} /* }}} */

/* RubySubletGrab {{{ */
/*
 * call-seq: grab(chain, &block) -> nil
 *
 * Add grabs to sublets
 *
 *  grab "A-b" do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubySubletGrab(VALUE self,
  VALUE name)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(name) && rb_block_given_p())
    {
      SubPanel *p = NULL;

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          int i;
          char buf[64] = { 0 };
          VALUE meth = Qnil;

          /* Add proc as instance method */
          snprintf(buf, sizeof(buf), "__grab_%s", SYM2CHAR(name));

          rb_funcall(rb_singleton_class(p->sublet->instance),
            rb_intern("define_method"), 2, CHAR2SYM(buf), rb_block_proc());

          meth = rb_obj_method(p->sublet->instance, CHAR2SYM(buf));

          /* Find grabs with this symbol */
          for(i = 0; i < subtle->grabs->ndata; i++)
            {
              SubGrab *g = GRAB(subtle->grabs->data[i]);

              if(g->flags & SUB_RUBY_DATA && g->data.num == name)
                {
                  g->flags    ^= (SUB_RUBY_DATA|SUB_GRAB_PROC);
                  g->data.num  = (unsigned long)meth;

                  rb_ary_push(shelter, meth); ///< Protect from GC
                }
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for grab");

  return Qnil;
} /* }}} */

/* RubySubletRender {{{ */
/*
 * call-seq: render -> nil
 *
 * Render a Sublet
 *
 *  sublet.render
 *  => nil
 */

static VALUE
RubySubletRender(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p) subScreenRender();

  return Qnil;
} /* }}} */

/* RubySubletNameReader {{{ */
/*
 * call-seq: name -> String
 *
 * Get name of Sublet
 *
 *  puts sublet.name
 *  => "sublet"
 */

static VALUE
RubySubletNameReader(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);

  return p ? rb_str_new2(p->sublet->name) : Qnil;
} /* }}} */

/* RubySubletIntervalReader {{{ */
/*
 * call-seq: interval -> Fixnum
 *
 * Get interval time of Sublet
 *
 *  puts sublet.interval
 *  => 60
 */

static VALUE
RubySubletIntervalReader(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);

  return p ? INT2FIX(p->sublet->interval) : Qnil;
} /* }}} */

/* RubySubletIntervalWriter {{{ */
/*
 * call-seq: interval=(fixnum) -> nil
 *
 * Set interval time of Sublet
 *
 *  sublet.interval = 60
 *  => nil
 */

static VALUE
RubySubletIntervalWriter(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(FIXNUM_P(value))
        {
          p->sublet->interval = FIX2INT(value);
          p->sublet->time     = subSubtleTime() + p->sublet->interval;

          if(0 < p->sublet->interval)
            p->sublet->flags |= SUB_SUBLET_INTERVAL;
          else p->sublet->flags &= ~SUB_SUBLET_INTERVAL;
        }
      else rb_raise(rb_eArgError, "Unknown value type `%s'", rb_obj_classname(value));
    }

  return Qnil;
} /* }}} */

/* RubySubletDataReader {{{ */
/*
 * call-seq: data -> String or nil
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle"
 */

static VALUE
RubySubletDataReader(VALUE self)
{
  int i;
  VALUE string = Qnil;
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(0 < p->sublet->text->nitems)
        {
          /* Concat string */
          for(i = 0; i < p->sublet->text->nitems; i++)
            {
              SubTextItem *item = (SubTextItem *)p->sublet->text->items[i];

              if(Qnil == string) rb_str_new2(item->data.string);
              else rb_str_cat(string, item->data.string, strlen(item->data.string));
            }
        }
    }

  return string;
} /* }}} */

/* RubySubletDataWriter {{{ */
/*
 * call-seq: data=(string) -> nil
 *
 * Set data of Sublet
 *
 *  sublet.data = "subtle"
 *  => nil
 */

static VALUE
RubySubletDataWriter(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Check value type */
      if(T_STRING == rb_type(value))
        {
          SubStyle *s = &subtle->styles.sublets, *style = NULL;

          /* Select style */
          if(s->styles && (style = subArrayGet(s->styles, p->sublet->styleid)))
              s = style;

          p->sublet->width = subTextParse(p->sublet->text,
            subtle->styles.sublets.font, RSTRING_PTR(value)) +
            STYLE_WIDTH((*s));
        }
      else rb_raise(rb_eArgError, "Unknown value type");
    }

  return Qnil;
} /* }}} */

/* RubySubletGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get geometry of a sublet
 *
 *  sublet.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
RubySubletGeometryReader(VALUE self)
{
  SubPanel *p = NULL;
  VALUE geometry = Qnil;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      SubStyle *s = NULL;
      XRectangle geom = { 0 };
      VALUE subtlext = Qnil, klass = Qnil;

      /* Pick sublet style */
      if(subtle->styles.sublets.styles)
        s = subArrayGet(subtle->styles.sublets.styles, p->sublet->styleid);

      subPanelGeometry(p, s ? s : &subtle->styles.sublets, &geom);

      /* Create geometry object */
      subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
      klass    = rb_const_get(subtlext, rb_intern("Geometry"));
      geometry = rb_funcall(klass, rb_intern("new"), 4,
        INT2FIX(geom.x), INT2FIX(geom.y), INT2FIX(geom.width),
        INT2FIX(geom.height));
    }

  return geometry;
} /* }}} */

/* RubySubletScreenReader {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get screen of a sublet
 *
 *  sublet.screen
 *  => #<Subtlext::screen:xxx>
 */

VALUE
RubySubletScreenReader(VALUE self)
{
  SubPanel *p = NULL;
  VALUE screen = Qnil;

  Data_Get_Struct(self, SubPanel, p);
  if(p) screen = RubySubtleToSubtlext(p->screen);

  return screen;
} /* }}} */

/* RubySubletStyleWriter {{{ */
/*
 * call-seq: style=(string) -> nil
 *           style=(symbol) -> nil
 *
 * Set style of Sublet
 *
 *  sublet.style = :subtle
 *  => nil
 */

static VALUE
RubySubletStyleWriter(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Check value type */
      if(FIXNUM_P(value))
        {
          SubStyle *s = &subtle->styles.sublets, *style = NULL;

          /* Select style */
          if(s->styles && (style = subArrayGet(s->styles, FIX2INT(value))))
            {
              s                  = style;
              p->sublet->styleid = FIX2INT(value);
            }

          p->sublet->width = subTextParse(p->sublet->text,
            subtle->styles.sublets.font, RSTRING_PTR(value)) +
            STYLE_WIDTH((*s));
        }
      else rb_raise(rb_eArgError, "Unknown value type");
    }

  return Qnil;
} /* }}} */

/* RubySubletShow {{{ */
/*
 * call-seq: show -> nil
 *
 * Show sublet on panel
 *
 *  sublet.show
 *  => nil
 */

static VALUE
RubySubletShow(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->flags &= ~SUB_PANEL_HIDDEN;

      /* Update screens */
      subScreenUpdate();
      subScreenRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletHide {{{ */
/*
 * call-seq: hide -> nil
 *
 * Hide sublet from panel
 *
 *  sublet.hide
 *  => nil
 */

static VALUE
RubySubletHide(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->flags |= SUB_PANEL_HIDDEN;

      /* Update screens */
      subScreenUpdate();
      subScreenRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletWatch {{{ */
/*
 * call-seq: watch(source) -> true or false
 *
 * Add watch file via inotify or socket
 *
 *  watch "/path/to/file"
 *  => true
 *
 *  @socket = TCPSocket("localhost", 6600)
 *  watch @socket
 */

static VALUE
RubySubletWatch(VALUE self,
  VALUE value)
{
  VALUE ret = Qfalse;
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(!(p->sublet->flags & (SUB_SUBLET_SOCKET|SUB_SUBLET_INOTIFY)) &&
          RTEST(value))
        {
          /* Socket file descriptor or ruby socket */
          if(FIXNUM_P(value) || rb_respond_to(value, rb_intern("fileno")))
            {
              int flags = 0;

              p->sublet->flags |= SUB_SUBLET_SOCKET;

              /* Get socket file descriptor */
              if(FIXNUM_P(value)) p->sublet->watch = FIX2INT(value);
              else
                {
                  p->sublet->watch = FIX2INT(rb_funcall(value, rb_intern("fileno"),
                    0, NULL));
                }

              XSaveContext(subtle->dpy, subtle->windows.support,
                p->sublet->watch, (void *)p);
              subEventWatchAdd(p->sublet->watch);

              /* Set nonblocking */
              if(-1 == (flags = fcntl(p->sublet->watch, F_GETFL, 0))) flags = 0;
              fcntl(p->sublet->watch, F_SETFL, flags | O_NONBLOCK);

              ret = Qtrue;
            }
#ifdef HAVE_SYS_INOTIFY_H
          else if(T_STRING == rb_type(value)) /// Inotify file
            {
              char buf[100] = { 0 };

#ifdef HAVE_WORDEXP_H
              /* Expand tildes in path */
              wordexp_t we;

              if(0 == wordexp(RSTRING_PTR(value), &we, 0))
                {
                  snprintf(buf, sizeof(buf), "%s", we.we_wordv[0]);

                  wordfree(&we);
                }
              else
#endif /* HAVE_WORDEXP_H */
              snprintf(buf, sizeof(buf), "%s", RSTRING_PTR(value));

              /* Create inotify watch */
              if(0 < (p->sublet->watch = inotify_add_watch(
                  subtle->notify, buf, IN_MODIFY)))
                {
                  p->sublet->flags |= SUB_SUBLET_INOTIFY;

                  XSaveContext(subtle->dpy, subtle->windows.support,
                    p->sublet->watch, (void *)p);
                  subSubtleLogDebug("Inotify: add watch=%s\n", buf);

                  ret = Qtrue;
                }
              else subSubtleLogWarn("Cannot watch file `%s': %s\n",
                buf, strerror(errno));
            }
#endif /* HAVE_SYS_INOTIFY_H */
          else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
            rb_obj_classname(value));
        }
    }

  return ret;
} /* }}} */

/* RubySubletUnwatch {{{ */
/*
 * call-seq: unwatch -> true or false
 *
 * Remove watch from Sublet
 *
 *  unwatch
 *  => true
 */

static VALUE
RubySubletUnwatch(VALUE self)
{
  VALUE ret = Qfalse;
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Probably a socket */
      if(p->sublet->flags & SUB_SUBLET_SOCKET)
        {
          XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
          subEventWatchDel(p->sublet->watch);

          p->sublet->flags &= ~SUB_SUBLET_SOCKET;
          p->sublet->watch  = 0;

          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      /* Inotify file */
      else if(p->sublet->flags & SUB_SUBLET_INOTIFY)
        {
          subSubtleLogDebug("Inotify: remove watch=%d\n", p->sublet->watch);

          XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
          inotify_rm_watch(subtle->notify, p->sublet->watch);

          p->sublet->flags &= ~SUB_SUBLET_INOTIFY;
          p->sublet->watch  = 0;

          ret = Qtrue;
        }
#endif /* HAVE_SYS_INOTIFY_H */
    }

  return ret;
} /* }}} */

/* RubySubletWarn {{{ */
/*
 * call-seq: warn(string) -> nil
 *
 * Print sublet warning
 *
 *  sublet.warn("test")
 *  => "<WARNING SUBLET sublet> test"
 */

static VALUE
RubySubletWarn(VALUE self,
  VALUE str)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p && T_STRING == rb_type(str))
    subSubtleLogSubletError(p->sublet->name, RSTRING_PTR(str));

  return Qnil;
} /* }}} */

/* Public */

 /** subRubyInit {{{
  * @brief Init ruby
  **/

void
subRubyInit(void)
{
  VALUE config = Qnil, options = Qnil, sublet = Qnil;

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

#ifdef HAVE_RB_ENC_SET_DEFAULT_INTERNAL
  {
    VALUE encoding = Qnil;

    /* FIXME: Fix for ruby 1.9.2p429 borrowed from ruby? */
    (void)rb_filesystem_encoding();

    /* Set encoding */
    encoding = rb_enc_from_encoding(rb_locale_encoding());
    rb_enc_set_default_external(encoding);
  }
#endif /* HAVE_RB_ENC_SET_DEFAULT_INTERNAL */

/* FIXME: Init ruby gems */
  {
    char *opts[] = { "ruby", "-e;" };

    /* Fake ruby_init_prelude() */
    ruby_options(2, opts);
  }

  /* FIXME: Autload seems to be broken in <1.9.2, use dispatcher */
  //rb_autoload(rb_cObject, SYM2ID(CHAR2SYM("Subtlext")), "subtle/subtlext");

  /*
   * Document-class: Object
   *
   * Ruby Object class dispatcher
   */

  rb_define_singleton_method(rb_cObject, "const_missing", RubyObjectDispatcher, 1);

  /*
   * Document-class: Subtle
   *
   * Subtle is the module for interaction with the window manager
   */

  mod = rb_define_module("Subtle");

  /*
   * Document-class: Config
   *
   * Config class for DSL evaluation
   */

  config = rb_define_class_under(mod, "Config", rb_cObject);

  /* Class methods */
  rb_define_method(config, "method_missing",         RubyConfigMissing,  -1);
  rb_define_method(config, "singleton_method_added", RubyConfigAdded,     1);
  rb_define_method(config, "set",                    RubyConfigSet,       2);
  rb_define_method(config, "gravity",                RubyConfigGravity,  -1);
  rb_define_method(config, "grab",                   RubyConfigGrab,     -1);
  rb_define_method(config, "tag",                    RubyConfigTag,      -1);
  rb_define_method(config, "view",                   RubyConfigView,     -1);
  rb_define_method(config, "on",                     RubyConfigOn,       -1);
  rb_define_method(config, "sublet",                 RubyConfigSublet,    1);
  rb_define_method(config, "screen",                 RubyConfigScreen,    1);
  rb_define_method(config, "style",                  RubyConfigStyle,     1);
  rb_define_method(config, "load_config",            RubyConfigLoadConfig,  1);

  /*
   * Document-class: Options
   *
   * Options class for DSL evaluation
   */

  options = rb_define_class_under(mod, "Options", rb_cObject);

  /* Params list */
  rb_define_attr(options, "params", 1, 1);
  rb_define_attr(options, "styles", 1, 1);

  /* Class methods */
  rb_define_method(options, "initialize",     RubyOptionsInit,        1);
  rb_define_method(options, "match",          RubyOptionsMatch,       1);
  rb_define_method(options, "gravity",        RubyOptionsGravity,     1);
  rb_define_method(options, "style",          RubyOptionsStyle,       1);
  rb_define_method(options, "on_match",       RubyOptionsOnMatch,    -1);
  rb_define_method(options, "method_missing", RubyOptionsDispatcher, -1);

  /*
   * Document-class: Subtle::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* DSL stuff */
  rb_define_method(sublet, "configure",      RubySubletConfigure,  1);
  rb_define_method(sublet, "helper",         RubySubletHelper,     0);
  rb_define_method(sublet, "on",             RubySubletOn,         1);
  rb_define_method(sublet, "grab",           RubySubletGrab,       1);

  /* Class methods */
  rb_define_method(sublet, "method_missing", RubySubletDispatcher,       -1);
  rb_define_method(sublet, "config",         RubySubletConfig,            0);
  rb_define_method(sublet, "render",         RubySubletRender,            0);
  rb_define_method(sublet, "name",           RubySubletNameReader,        0);
  rb_define_method(sublet, "interval",       RubySubletIntervalReader,    0);
  rb_define_method(sublet, "interval=",      RubySubletIntervalWriter,    1);
  rb_define_method(sublet, "data",           RubySubletDataReader,        0);
  rb_define_method(sublet, "data=",          RubySubletDataWriter,        1);
  rb_define_method(sublet, "geometry",       RubySubletGeometryReader,    0);
  rb_define_method(sublet, "screen",         RubySubletScreenReader,      0);
  rb_define_method(sublet, "show",           RubySubletShow,              0);
  rb_define_method(sublet, "style=",         RubySubletStyleWriter,       1);
  rb_define_method(sublet, "hide",           RubySubletHide,              0);
  rb_define_method(sublet, "watch",          RubySubletWatch,             1);
  rb_define_method(sublet, "unwatch",        RubySubletUnwatch,           0);
  rb_define_method(sublet, "warn",           RubySubletWarn,              1);

  /* Bypassing garbage collection */
  shelter = rb_ary_new();
  rb_gc_register_address(&shelter);

  subSubtleLogDebugSubtle("Init\n");
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file
  * @param[in]  path  Path to config file
  **/

void
subRubyLoadConfig(void)
{
  VALUE klass = Qnil;
  SubTag *t = NULL;

  /* Create default tag */
  if(!(subtle->flags & SUB_SUBTLE_CHECK) &&
      (t = subTagNew("default", NULL)))
    subArrayPush(subtle->tags, (void *)t);

  /* Reset styles */
  subStyleReset(&subtle->styles.all,        0); ///< Ensure sane base values
  subStyleReset(&subtle->styles.views,     -1);
  subStyleReset(&subtle->styles.title,     -1);
  subStyleReset(&subtle->styles.sublets,   -1);
  subStyleReset(&subtle->styles.separator, -1);
  subStyleReset(&subtle->styles.clients,    0);
  subStyleReset(&subtle->styles.subtle,     0);

  /* Reset values */
  subtle->gravity           = -1;
  subtle->styles.subtle.bg  = -1; ///< Must be -1 for wallpaper
  subtle->styles.urgent     = NULL;
  subtle->styles.occupied   = NULL;
  subtle->styles.focus      = NULL;
  subtle->styles.viewsep    = NULL;
  subtle->styles.subletsep  = NULL;

  /* Create and register config values */
  config_sublets = rb_hash_new();
  config_methods = rb_ary_new();
  rb_gc_register_address(&config_sublets);
  rb_gc_register_address(&config_methods);

  /* Load supplied config or default */
  klass           = rb_const_get(mod, rb_intern("Config"));
  config_instance = rb_funcall(klass, rb_intern("new"), 0, NULL);
  rb_gc_register_address(&config_instance);

  if(Qfalse == RubyConfigLoadConfig(config_instance,
      rb_str_new2(subtle->paths.config ? subtle->paths.config : PKG_CONFIG)))
    {
      subSubtleFinish();

      exit(-1);
    }
  else if(subtle->flags & SUB_SUBTLE_CHECK) printf("Syntax OK\n");

  /* If not check only, lazy eval config values */
  if(!(subtle->flags & SUB_SUBTLE_CHECK)) RubyEvalConfig();

  /* Release methods list */
  rb_gc_unregister_address(&config_methods);

  return;
} /* }}} */

 /** subRubyReloadConfig {{{
  * @brief Reload config file
  **/

void
subRubyReloadConfig(void)
{
  int i, rx = 0, ry = 0, x = 0, y = 0, *vids = NULL;
  unsigned int mask = 0;
  Window root = None, win = None;
  SubClient *c = NULL;

  /* Reset panel height */
  subtle->ph = 0;

  /* Reset flags before reloading */
  subtle->flags &= (SUB_SUBTLE_DEBUG|SUB_SUBTLE_EWMH|SUB_SUBTLE_RUN|
    SUB_SUBTLE_XINERAMA|SUB_SUBTLE_XRANDR|SUB_SUBTLE_URGENT);

  /* Unregister config values */
  rb_gc_unregister_address(&config_sublets);
  rb_gc_unregister_address(&config_instance);
  rb_gc_unregister_address(&config_methods);

  /* Reset sublet panel flags */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->sublets->data[i]);

      p->flags &= ~(SUB_PANEL_BOTTOM|SUB_PANEL_SPACER1|
        SUB_PANEL_SPACER1| SUB_PANEL_SEPARATOR1|SUB_PANEL_SEPARATOR2);
      p->screen = NULL;
    }

  /* Allocate memory to store current views per screen */
  vids = (int *)subSharedMemoryAlloc(subtle->screens->ndata, sizeof(int));

  /* Reset screen panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      vids[i]   = s->viewid; ///< Store views
      s->flags &= ~(SUB_SCREEN_STIPPLE|SUB_SCREEN_PANEL1|SUB_SCREEN_PANEL2);

      subArrayClear(s->panels, True);
    }

  /* Clear arrays */
  subArrayClear(subtle->hooks,     True); ///< Must be first
  subArrayClear(subtle->grabs,     True);
  subArrayClear(subtle->gravities, True);
  subArrayClear(subtle->sublets,   False);
  subArrayClear(subtle->tags,      True);
  subArrayClear(subtle->views,     True);

  /* Load and configure */
  subRubyLoadConfig();
  subRubyLoadSublets();
  subRubyLoadPanels();
  subDisplayConfigure();

  /* Restore current views */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      if(vids[i] < subtle->views->ndata)
        SCREEN(subtle->screens->data[i])->viewid = vids[i];
    }

  /* Update client tags */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      int flags = 0;

      c            = CLIENT(subtle->clients->data[i]);
      c->gravityid = -1;
      c->flags     = (c->flags & (SUB_TYPE_CLIENT|SUB_CLIENT_FOCUS|
        SUB_CLIENT_INPUT|SUB_CLIENT_CLOSE)); ///< Reset flags

      subClientSetType(c, &flags);
      subClientRetag(c, &flags);
      subClientToggle(c, ~c->flags & flags, True); ///< Toggle flags
    }

  printf("Reloaded config\n");

  /* Update screens and panels */
  subScreenConfigure();
  subScreenUpdate();
  subScreenRender();
  subPanelPublish();

  /* Focus pointer window */
  XQueryPointer(subtle->dpy, ROOT, &root, &win, &rx, &ry, &x, &y, &mask);

  if((c = CLIENT(subSubtleFind(win, CLIENTID))))
    subClientFocus(c, True);
  else
    {
      c = subClientNext(0, False);
      if(c) subClientFocus(c, True);
    }

  /* Hook: Reload */
  subHookCall(SUB_HOOK_RELOAD, NULL);

  free(vids);
} /* }}} */

 /** subRubyLoadSublet {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublet(const char *file)
{
  int state = 0;
  SubPanel *p = NULL;
  VALUE rargs[3] = { Qnil };

  /* Load sublet */
  p = subPanelNew(SUB_PANEL_SUBLET);
  p->sublet->instance = Data_Wrap_Struct(rb_const_get(mod,
    rb_intern("Sublet")), NULL, NULL, (void *)p);

  rb_ary_push(shelter, p->sublet->instance); ///< Protect from GC

  if(Qfalse == RubyConfigLoadConfig(p->sublet->instance, rb_str_new2(file)))
    {
      subSubtleLogWarn("Cannot load sublet `%s'\n", file);
      RubyBacktrace();

      subRubyUnloadSublet(p);

      return;
    }

  /* Carefully apply sublet config */
  rargs[0] = CHAR2SYM(p->sublet->name);
  rargs[1] = (VALUE)p->sublet;

  rb_protect(RubyWrapSubletConfig, (VALUE)&rargs, &state);
  if(state)
    {
      subSubtleLogWarn("Cannot configure sublet `%s'\n", file);
      RubyBacktrace();

      subRubyUnloadSublet(p);

      return;
    }

  /* Carefully configure sublet */
  if(!subRubyCall(SUB_CALL_CONFIGURE, p->sublet->instance, NULL))
    {
      subRubyUnloadSublet(p);

      return;
    }

  /* Sanitize interval time */
  if(0 >= p->sublet->interval) p->sublet->interval = 60;

  /* First run */
  if(p->sublet->flags & SUB_SUBLET_RUN)
    subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);

  subArrayPush(subtle->sublets, (void *)p);

  printf("Loaded sublet (%s)\n", p->sublet->name);
} /* }}} */

 /** subRubyUnloadSublet {{{
  * @brief Unload sublets at runtime
  * @param[in]  p  A #SubPanel
  **/

void
subRubyUnloadSublet(SubPanel *p)
{
  int i;

  assert(p);

  /* Remove sublet from panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(s->panels) subArrayRemove(s->panels, (void *)p);
    }

  /* Remove hooks */
  for(i = 0; i < subtle->hooks->ndata; i++)
    {
      SubHook *hook = HOOK(subtle->hooks->data[i]);

      if(RubyReceiver(p->sublet->instance, hook->proc))
        {
          subArrayRemove(subtle->hooks, (void *)hook);
          subRubyRelease(hook->proc);
          subHookKill(hook);
          i--; ///< Prevent skipping of entries
        }
    }

  /* Remove grabs */
  for(i = 0; i < subtle->grabs->ndata; i++)
    {
      SubGrab *grab = GRAB(subtle->grabs->data[i]);

      if(grab->flags & SUB_GRAB_PROC &&
          RubyReceiver(p->sublet->instance, grab->data.num))
        {
          subArrayRemove(subtle->grabs, (void *)grab);
          subRubyRelease(grab->data.num);
          subGrabKill(grab);
          i--; ///< Prevent skipping of entries
        }
    }

  subArrayRemove(subtle->sublets, (void *)p);
  subPanelKill(p);
  subPanelPublish();
} /* }}} */

 /** subRubyLoadPanels {{{
  * @brief Load panels
  **/

void
subRubyLoadPanels(void)
{
  int state = 0;

  /* Carefully load panels */
  rb_protect(RubyWrapLoadPanels, Qnil, &state);
  if(state)
    {
      subSubtleLogWarn("Cannot load panel config\n");
      RubyBacktrace();
      subSubtleFinish();

      exit(-1);
    }
} /* }}} */

 /** subRubyLoadSublets {{{
  * @brief Load sublets from path
  **/

void
subRubyLoadSublets(void)
{
  int i, num, len = 0;
  char buf[100] = { 0 };
  struct dirent **entries = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  /* Init inotify on demand */
  if(!subtle->notify)
    {
      if(0 > (subtle->notify = inotify_init()))
        {
          subSubtleLogWarn("Cannot init inotify\n");
          subSubtleLogDebug("Inotify: error=%s\n", strerror(errno));

          return;
        }
      else fcntl(subtle->notify, F_SETFL, O_NONBLOCK);
    }
#endif /* HAVE_SYS_INOTIFY_H */

  /* Check path */
  if(subtle->paths.sublets)
    len += snprintf(buf, sizeof(buf), "%s", subtle->paths.sublets);
  else
    {
      char *home = NULL;

      if((home = getenv("XDG_DATA_HOME")))
        {
          len += snprintf(buf, sizeof(buf), "%s/%s/sublets",
            home, PKG_NAME);
        }
      else len += snprintf(buf, sizeof(buf), "%s/.local/share/%s/sublets",
        getenv("HOME"), PKG_NAME);
    }

  /* Scan directory */
  if(0 < ((num = scandir(buf, &entries, RubyFilter, alphasort))))
    {
      for(i = 0; i < num; i++)
        {
          /* Temporary append file name to path */
          snprintf(buf + len, sizeof(buf), "/%s", entries[i]->d_name);

          subRubyLoadSublet(buf);

          /* Restore path */
          buf[strlen(buf) - (strlen(entries[i]->d_name) + 1)] = '\0';

          free(entries[i]);
        }
      free(entries);

      subArraySort(subtle->grabs, subGrabCompare);
    }
} /* }}} */

 /** subRubyCall {{{
  * @brief Safely call ruby script
  * @param[in]  type   Script type
  * @param[in]  proc   Script receiver
  * @param[in]  data   Extra data
  * @retval  1  Call was successful
  * @retval  0  Call failed
  **/

int
subRubyCall(int type,
  unsigned long proc,
  void *data)
{
  int state = 0;
  VALUE rargs[3] = { Qnil };

  /* Wrap up data */
  rargs[0] = (VALUE)type;
  rargs[1] = proc;
  rargs[2] = (VALUE)data;

  /* Carefully call */
  rb_protect(RubyWrapCall, (VALUE)&rargs, &state);
  if(state) RubyBacktrace();

#ifdef DEBUG
  subSubtleLogDebugRuby("Call: GC START\n");
  rb_gc_start();
#endif /* DEBUG */

  return !state; ///< Reverse odd logic
} /* }}} */

 /** subRubyRelease {{{
  * @brief Release value from shelter
  * @param[in]  value  The released value
  **/

int
subRubyRelease(unsigned long value)
{
  int state = 0;

  rb_protect(RubyWrapRelease, value, &state);

  return state;
} /* }}} */

 /** subRubyFinish {{{
  * @brief Finish ruby stack
  **/

void
subRubyFinish(void)
{
  if(Qnil != shelter)
    {
      ruby_finalize();

#ifdef HAVE_SYS_INOTIFY_H
      if(subtle && subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */
    }

  subSubtleLogDebugSubtle("Finish\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
