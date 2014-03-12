 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/subtle.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* SubtleSend {{{ */
static VALUE
SubtleSend(char *message)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSubtlextConnect(NULL); ///< Implicit open connection

  subSharedMessage(display, DefaultRootWindow(display),
    message, data, 32, True);

  return Qnil;
} /* }}} */

/* Singleton */

/* subSubtleSingDisplayReader {{{ */
/*
 * call-seq: display -> String
 *
 * Get the display name.
 *
 *  subtle.display
 *  => ":0"
 */

VALUE
subSubtleSingDisplayReader(VALUE self)
{
  subSubtlextConnect(NULL); ///< Implicit open connection

  return rb_str_new2(DisplayString(display));
} /* }}} */

/* subSubtleSingDisplayWriter {{{ */
/*
 * call-seq: display=(string) -> nil
 *
 * Set the display name.
 *
 *  subtle.display = ":0"
 *  => nil
 */

VALUE
subSubtleSingDisplayWriter(VALUE self,
  VALUE display_string)
{
  /* Explicit open connection */
  subSubtlextConnect(T_STRING == rb_type(display_string) ?
    RSTRING_PTR(display_string) : NULL);

  return Qnil;
} /* }}} */

/* subSubtleSingAskRunning {{{ */
/*
 * call-seq: running? -> true or false
 *
 * Whether a subtle instance on current display is running.
 *
 *  subtle.running?
 *  => true
 *
 *  subtle.running?
 *  => false
 */

VALUE
subSubtleSingAskRunning(VALUE self)
{
  char *version = NULL;
  Window *support = NULL;
  VALUE running = Qfalse;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get supporting window */
  if((support = (Window *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW, XInternAtom(display,
      "_NET_SUPPORTING_WM_CHECK", False), NULL)))
    {
      /* Get version property */
      if((version = subSharedPropertyGet(display, *support, XInternAtom(display,
          "UTF8_STRING", False), XInternAtom(display, "SUBTLE_VERSION", False),
          NULL)))
        {
          running = Qtrue;

          free(version);
        }

      free(support);
    }

  return running;
} /* }}} */

/* subSubtleSingSelect {{{ */
/*
 * call-seq: select_window -> Fixnum
 *
 * Select a window and get the window id of it.
 *
 *  select_window
 *  => 8388617
 */

VALUE
subSubtleSingSelect(VALUE self)
{
  int i, format = 0, buttons = 0;
  unsigned int nwins = 0;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  XEvent event;
  Window win = None;
  Atom type = None, rtype = None;
  Window wroot = None, parent = None, root = None, *wins = NULL;
  Cursor cursor = None;

  subSubtlextConnect(NULL); ///< Implicit open connection

  root   = DefaultRootWindow(display);
  cursor = XCreateFontCursor(display, XC_cross);
  type   = XInternAtom(display, "WM_STATE", True);

  /* Grab pointer */
  if(XGrabPointer(display, root, False, ButtonPressMask|ButtonReleaseMask,
      GrabModeSync, GrabModeAsync, root, cursor, CurrentTime))
    {
      XFreeCursor(display, cursor);

      return Qnil;
    }

  /* Select a window */
  while(None == win || 0 != buttons)
    {
      XAllowEvents(display, SyncPointer, CurrentTime);
      XWindowEvent(display, root, ButtonPressMask|ButtonReleaseMask, &event);

      switch(event.type)
        {
          case ButtonPress:
            if(None == win)
              win = event.xbutton.subwindow ? event.xbutton.subwindow : root; ///< Sanitize
            buttons++;
            break;
          case ButtonRelease:
            if(0 < buttons) buttons--;
            break;
        }
      }

  /* Find children with WM_STATE atom */
  XQueryTree(display, win, &wroot, &parent, &wins, &nwins);

  for(i = 0; i < nwins; i++)
    {
      if(Success == XGetWindowProperty(display, wins[i], type, 0, 0, False,
          AnyPropertyType, &rtype, &format, &nitems, &bytes, &data))
        {
          if(data)
            {
              XFree(data);
              data = NULL;
            }

          if(type == rtype)
            {
              win = wins[i];

              break;
            }
        }
    }

  if(wins) XFree(wins);
  XFreeCursor(display, cursor);
  XUngrabPointer(display, CurrentTime);

  XSync(display, False); ///< Sync all changes

  return None != win ? LONG2NUM(win) : Qnil;
} /* }}} */

/* subSubtleSingRender {{{ */
/*
 * call-seq: render -> nil
 *
 * Force Subtle to render screen panels.
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleSingRender(VALUE self)
{
  return SubtleSend("SUBTLE_RENDER");
} /* }}} */

/* subSubtleSingReload {{{ */
/*
 * call-seq: reload -> nil
 *
 * Force Subtle to reload config and sublets.
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleSingReload(VALUE self)
{
  return SubtleSend("SUBTLE_RELOAD");
} /* }}} */

/* subSubtleSingRestart {{{ */
/*
 * call-seq: restart -> nil
 *
 * Force Subtle to restart.
 *
 *  subtle.restart
 *  => nil
 */

VALUE
subSubtleSingRestart(VALUE self)
{
  return SubtleSend("SUBTLE_RESTART");
} /* }}} */

/* subSubtleSingQuit {{{ */
/*
 * call-seq: quit -> nil
 *
 * Force Subtle to exit.
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleSingQuit(VALUE self)
{
  return SubtleSend("SUBTLE_QUIT");
} /* }}} */

/* subSubtleSingColors {{{ */
/*
 * call-seq: colors -> Hash
 *
 * Get an array of all Colors.
 *
 *  Subtlext::Subtle.colors
 *  => { :fg_panel => #<Subtlext::Color:xxx> }
 */

VALUE
subSubtleSingColors(VALUE self)
{
  int i;
  unsigned long ncolors = 0, *colors = NULL;
  VALUE meth = Qnil, klass = Qnil, hash = Qnil;
  const char *names[] = {
    "title_fg",           "title_bg",             "title_bo_top",
    "title_bo_right",     "title_bo_bottom",      "title_bo_left",
    "views_fg",           "views_bg",             "views_bo_top",
    "views_bo_right",     "views_bo_bottom",      "views_bo_left",
    "focus_fg",           "focus_bg",             "focus_bo_top",
    "focus_bo_right",     "focus_bo_bottom",      "focus_bo_left",
    "urgent_fg",          "urgent_bg",            "urgent_bo_top",
    "urgent_bo_right",    "urgent_bo_bottom",     "urgent_bo_left",
    "occupied_fg",        "occupied_bg",          "occupied_bo_top",
    "occupied_bo_right",  "occupied_bo_bottom",   "occupied_bo_left",
    "sublets_fg",         "sublets_bg",           "sublets_bo_top",
    "sublets_bo_right",   "sublets_bo_bottom",    "sublets_bo_left",
    "separator_fg",       "separator_bg",         "separator_bo_top",
    "separator_bo_right", "separator_bo_bottom",  "separator_bo_left",
    "client_active",      "client_inactive",
    "panel_top",          "panel_bottom",
    "stipple",            "background"
  };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Color"));
  hash  = rb_hash_new();

  /* Check result */
  if((colors = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "SUBTLE_COLORS", False), &ncolors)))
    {
      for(i = 0; i < ncolors && i < LENGTH(names); i++)
        {
          VALUE c = rb_funcall(klass, meth, 1, LONG2NUM(colors[i]));

          rb_hash_aset(hash, CHAR2SYM(names[i]), c);
        }

      free(colors);
    }

  return hash;
} /* }}} */

/* subSubtleSingFont {{{ */
/*
 * call-seq: Font -> String or nil
 *
 * Get the font used in subtle.
 *
 *  Subtlext::Subtle.font
 *  => "-*-*-medium-*-*-*-14-*-*-*-*-*-*-*"
 */

VALUE
subSubtleSingFont(VALUE self)
{
  char *prop = NULL;
  VALUE font = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get results */
  if((prop = subSharedPropertyGet(display, DefaultRootWindow(display),
      XInternAtom(display, "UTF8_STRING", False),
      XInternAtom(display, "SUBTLE_FONT", False),
      NULL)))
    {
      font = rb_str_new2(prop);

      free(prop);
    }

  return font;
} /* }}} */

/* subSubtleSingSpawn {{{ */
/*
 * call-seq: spawn(cmd) -> Subtlext::Client
 *
 * Spawn a command and returns a Client object.
 *
 *  spawn("xterm")
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subSubtleSingSpawn(VALUE self,
  VALUE cmd)
{
  VALUE ret = Qnil;

  /* Check object type */
  if(T_STRING == rb_type(cmd))
    {
      pid_t pid = 0;

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Create client with empty window id since we cannot
       * know the real window id at this point (race) */
      if(0 < (pid = subSharedSpawn(RSTRING_PTR(cmd))))
        {
          ret = subClientInstantiate((int)pid);
          rb_iv_set(ret, "@pid", INT2FIX((int)pid));
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(cmd));

  return ret;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
