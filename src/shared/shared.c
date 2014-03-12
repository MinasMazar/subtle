
 /**
  * @package subtle
  *
  * @file Shared functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/shared/shared.c,v 3204 2012/05/22 21:15:06 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "shared.h"

/* Memory */

 /** subSharedMemoryAlloc {{{
  * @brief Alloc memory and check result
  * @param[in]  n     Number of elements
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryAlloc(size_t n,
  size_t size)
{
  void *mem = NULL;

  /* Check result */
  if(!(mem = calloc(n, size)))
    {
      fprintf(stderr, "<ERROR> Failed allocating memory\n");

      abort();
    }

  return mem;
} /* }}} */

 /** subSharedMemoryRealloc {{{
  * @brief Realloc memory and check result
  * @param[in]  mem   Memory block
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryRealloc(void *mem,
  size_t size)
{
  /* Check result */
  if(!(mem = realloc(mem, size)))
    fprintf(stderr, "<ERROR> Memory has been freed. Expected?\n");

  return mem;
} /* }}} */

/* Regex */

 /** subSharedRegexNew {{{
  * @brief Create new regex
  * @param[in]  pattern  Regex
  * @return Returns a #regex_t or \p NULL
  **/

regex_t *
subSharedRegexNew(char *pattern)
{
  int ecode = 0;
  regex_t *preg = NULL;
  OnigErrorInfo einfo;

  assert(pattern);

  /* Create onig regex */
  ecode = onig_new(&preg, (UChar *)pattern,
    (UChar *)(pattern + strlen(pattern)),
    ONIG_OPTION_EXTEND|ONIG_OPTION_SINGLELINE|ONIG_OPTION_IGNORECASE,
    ONIG_ENCODING_ASCII, ONIG_SYNTAX_RUBY, &einfo);

  /* Check for compile errors */
  if(ecode)
    {
      UChar ebuf[ONIG_MAX_ERROR_MESSAGE_LEN] = { 0 };

      onig_error_code_to_str((UChar*)ebuf, ecode, &einfo);

      fprintf(stderr, "<CRITICAL> Failed compiling regex `%s': %s\n",
        pattern, ebuf);

      free(preg);

      return NULL;
    }

  return preg;
} /* }}} */

 /** subSharedRegexMatch {{{
  * @brief Check if string match preg
  * @param[in]  preg      A #regex_t
  * @param[in]  string    String
  * @retval  1  If string matches preg
  * @retval  0  If string doesn't match
  **/

int
subSharedRegexMatch(regex_t *preg,
  char *string)
{
  assert(preg);

  return ONIG_MISMATCH != onig_match(preg, (UChar *)string,
    (UChar *)(string + strlen(string)), (UChar *)string, NULL,
    ONIG_OPTION_NONE);
} /* }}} */

 /** subSharedRegexKill {{{
  * @brief Kill preg
  * @param[in]  preg  #regex_t
  **/

void
subSharedRegexKill(regex_t *preg)
{
  assert(preg);

  onig_free(preg);
} /* }}} */

/* Property */

 /** subSharedPropertyGet {{{
  * @brief Get window property
  * @param[in]     disp  Display
  * @param[in]     win   Client window
  * @param[in]     type  Property type
  * @param[in]     prop  Property
  * @param[inout]  size  Size of the property
  * return Returns the property
  **/

char *
subSharedPropertyGet(Display *disp,
  Window win,
  Atom type,
  Atom prop,
  unsigned long *size)
{
  int format = 0;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  Atom rtype = None;

  assert(win);

  /* Get property */
  if(Success != XGetWindowProperty(disp, win, prop, 0L, 4096,
      False, type, &rtype, &format, &nitems, &bytes, &data))
    return NULL;

  /* Check result */
  if(type != rtype)
    {
      XFree(data);

      return NULL;
    }

  if(size) *size = nitems;

  return (char *)data;
} /* }}} */

 /** subSharedPropertyGetStrings {{{
  * @brief Get property list
  * @param[in]     disp Display
  * @param[in]     win     Client window
  * @param[in]     prop    Property
  * @param[inout]  nlist   Size of the property list
  * return Returns the property list
  **/

char **
subSharedPropertyGetStrings(Display *disp,
  Window win,
  Atom prop,
  int *nlist)
{
  char **list = NULL;
  XTextProperty text;

  assert(win && nlist);

  /* Check UTF8 and XA_STRING */
  if((XGetTextProperty(disp, win, &text, prop) ||
      XGetTextProperty(disp, win, &text, XA_STRING)) && text.nitems)
    {
      XmbTextPropertyToTextList(disp, &text, &list, nlist);

      XFree(text.value);
    }

  return list;
} /* }}} */

 /** subSharedPropertySetStrings {{{
  * @brief Convert list to multibyte and set property
  * @param[in]  disp    Display
  * @param[in]  win     Window
  * @param[in]  prop    Property
  * @param[in]  list    String list
  * @param[in]  nsize   Number of elements
  * return Returns the property list
  **/

void
subSharedPropertySetStrings(Display *disp,
  Window win,
  Atom prop,
  char **list,
  int nlist)
{
  XTextProperty text;

  /* Convert list to multibyte text property */
  XmbTextListToTextProperty(disp, list, nlist, XUTF8StringStyle, &text);
  XSetTextProperty(disp, win, &text, prop);

  XFree(text.value);
} /* }}} */

 /** subSharedPropertyName {{{
  * @brief Get window name
  * @warning Must be free'd
  * @param[in]     disp      Display
  * @param[in]     win       A #Window
  * @param[inout]  name      Window WM_NAME
  * @param[in]     fallback  Fallback name
  **/

void
subSharedPropertyName(Display *disp,
  Window win,
  char **name,
  char *fallback)
{
 char **list = NULL;
  XTextProperty text;

  /* Get text property */
  XGetTextProperty(disp, win, &text, XInternAtom(disp, "_NET_WM_NAME", False));
  if(0 == text.nitems)
    {
      XGetTextProperty(disp, win, &text, XA_WM_NAME);
      if(0 == text.nitems)
        {
          *name = strdup(fallback);

          return;
        }
    }

  /* Handle encoding */
  if(XA_STRING == text.encoding)
    {
      *name = strdup((char *)text.value);
    }
  else ///< Utf8 string
    {
      int nlist = 0;

      /* Convert text property */
      if(Success == XmbTextPropertyToTextList(disp, &text, &list, &nlist) &&
          list)
        {
          if(0 < nlist && *list)
            {
              /* FIXME strdup() allocates not enough memory to hold string */
              *name = subSharedMemoryAlloc(text.nitems + 2, sizeof(char));
              strncpy(*name, *list, text.nitems);
            }
          XFreeStringList(list);
        }
    }

  if(text.value) XFree(text.value);

  /* Fallback */
  if(!*name) *name = strdup(fallback);
} /* }}} */

 /** subSharedPropertyClass {{{
  * @brief Get window class
  * @warning Must be free'd
  * @param[in]     disp  Display
  * @param[in]     win      A #Window
  * @param[inout]  inst     Window instance name
  * @param[inout]  klass    Window class name
  **/

void
subSharedPropertyClass(Display *disp,
  Window win,
  char **inst,
  char **klass)
{
  int size = 0;
  char **klasses = NULL;

  assert(win);

  klasses = subSharedPropertyGetStrings(disp, win, XA_WM_CLASS, &size);

  /* Sanitize instance/class names */
  if(inst)  *inst  = strdup(0 < size ? klasses[0] : "subtle");
  if(klass) *klass = strdup(1 < size ? klasses[1] : "subtle");

  if(klasses) XFreeStringList(klasses);
} /* }}} */

 /** subSharedPropertyGeometry {{{
  * @brief Get window geometry
  * @param[in]     disp   Display
  * @param[in]     win       A #Window
  * @param[inout]  geometry  A #XRectangle
  **/

void
subSharedPropertyGeometry(Display *disp,
  Window win,
  XRectangle *geometry)
{
  Window root = None;
  unsigned int bw = 0, depth = 0;
  XRectangle r = { 0 };

  assert(win && geometry);

  XGetGeometry(disp, win, &root, (int *)&r.x, (int *)&r.y,
    (unsigned int *)&r.width, (unsigned int *)&r.height, &bw, &depth);

  *geometry = r;
} /* }}} */

 /** subSharedPropertyDelete {{{
  * @brief Deletes the property
  * @param[in]  disp  Display
  * @param[in]  win   A #Window
  * @param[in]  prop  Property
  **/

void
subSharedPropertyDelete(Display *disp,
  Window win,
  Atom prop)
{
  assert(win);

  XDeleteProperty(disp, win, prop);
} /* }}} */

/* Draw */

 /** subSharedDrawString {{{
  * @brief Draw text
  * @param[in]  disp  Display
  * @param[in]  gc    GC
  * @param[in]  f     A #SubFont
  * @param[in]  win   Target window
  * @param[in]  x     X position
  * @param[in]  y     Y position
  * @param[in]  fg    Foreground color
  * @param[in]  bg    Background color
  * @param[in]  text  Text to draw
  * @param[in]  len   Text length
  **/

void
subSharedDrawString(Display *disp,
  GC gc,
  SubFont *f,
  Window win,
  int x,
  int y,
  long fg,
  long bg,
  const char *text,
  int len)
{
  XGCValues gvals;

  assert(f && text);

  /* Draw text */
#ifdef HAVE_X11_XFT_XFT_H
  if(f->xft) ///< XFT
    {
      XftColor color = { 0 };
      XColor xcolor = { 0 };

      /* Get color values */
      xcolor.pixel = fg;
      XQueryColor(disp, DefaultColormap(disp, DefaultScreen(disp)), &xcolor);

      color.pixel       = xcolor.pixel;
      color.color.red   = xcolor.red;
      color.color.green = xcolor.green;
      color.color.blue  = xcolor.blue;
      color.color.alpha = 0xffff;

      XftDrawChange(f->draw, win);
      XftDrawStringUtf8(f->draw, &color, f->xft, x, y, (XftChar8 *)text, len);
    }
  else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
    {
      /* Draw text */
      gvals.foreground = fg;
      gvals.background = bg;

      XChangeGC(disp, gc, GCForeground|GCBackground, &gvals);
      XmbDrawString(disp, win, f->xfs, gc, x, y, text, len);
    }
} /* }}} */

 /** subSharedDrawIcon {{{
  * @brief Draw text
  * @param[in]  disp    Display
  * @param[in]  gc      GC
  * @param[in]  win     Target window
  * @param[in]  x       X position
  * @param[in]  y       Y position
  * @param[in]  width   Width of pixmap
  * @param[in]  height  Height of pixmap
  * @param[in]  fg      Foreground color
  * @param[in]  bg      Background color
  * @param[in]  pixmap  Pixmap to draw
  * @param[in]  bitmap  Whether icon is a bitmap
  **/

void
subSharedDrawIcon(Display *disp,
  GC gc,
  Window win,
  int x,
  int y,
  int width,
  int height,
  long fg,
  long bg,
  Pixmap pixmap,
  int bitmap)
{
  XGCValues gvals;

  /* Plane color */
  gvals.foreground = fg;
  gvals.background = bg;
  XChangeGC(disp, gc, GCForeground|GCBackground, &gvals);

  /* Copy icon to destination window */
  if(bitmap)
    XCopyPlane(disp, pixmap, win, gc, 0, 0, width, height, x, y, 1);
#ifdef HAVE_X11_XPM_H
  else XCopyArea(disp, pixmap, win, gc, 0, 0, width, height, x, y);
#endif /* HAVE_X11_XPM_H */
} /* }}} */

/* Font */

 /** subSharedFontNew {{{
  * @brief Create new font
  * @param[in]  disp  Display
  * @param[in]  name  Font name
  * @return  New #SubFont
  **/

SubFont *
subSharedFontNew(Display *disp,
  const char *name)
{
  SubFont *f = NULL;

  /* Load font */
#ifdef HAVE_X11_XFT_XFT_H
  if(!strncmp(name, "xft:", 4))
    {
      XftFont *xft = NULL;

      /* Load XFT font */
      if((xft = XftFontOpenName(disp, DefaultScreen(disp), name + 4)))
        {
          /* Create new font */
          f = FONT(subSharedMemoryAlloc(1, sizeof(SubFont)));
          f->xft  = xft;
          f->draw = XftDrawCreate(disp, DefaultRootWindow(disp),
            DefaultVisual(disp, DefaultScreen(disp)),
            DefaultColormap(disp, DefaultScreen(disp)));

          /* Font metrics */
          f->height = f->xft->ascent + f->xft->descent + 2;
          f->y      = (f->height - 2 + f->xft->ascent) / 2;
        }
    }
  else
#endif /* HAVE_X11_XFT_XFT_H */
    {
      int n = 0;
      char *def = NULL, **missing = NULL, **names = NULL;
      XFontStruct **xfonts = NULL;
      XFontSet xfs;

      /* Load font set */
      if((xfs = XCreateFontSet(disp, name, &missing, &n, &def)))
        {
          /* Create new font */
          f = FONT(subSharedMemoryAlloc(1, sizeof(SubFont)));
          f->xfs = xfs;

          XFontsOfFontSet(f->xfs, &xfonts, &names);

          /* Font metrics */
          f->height = xfonts[0]->max_bounds.ascent +
            xfonts[0]->max_bounds.descent + 2;
          f->y      = (f->height - 2 + xfonts[0]->max_bounds.ascent) / 2;

          if(missing) XFreeStringList(missing); ///< Ignore this
        }
    }

  return f;
} /* }}} */

 /** subSharedFontKill {{{
  * @brief Kill a font
  * @param[in]  disp  Display
  * @param[in]  f     A #SubFont
  **/

void
subSharedFontKill(Display *disp,
  SubFont *f)
{
  assert(f);

#ifdef HAVE_X11_XFT_XFT_H
  if(f->xft)
    {
      XftFontClose(disp, f->xft);
      XftDrawDestroy(f->draw);
    }
  else
#endif /* HAVE_X11_XFT_XFT_H */
    {
      XFreeFontSet(disp, f->xfs);
    }

  free(f);
} /* }}} */

/* Misc */

 /** subSharedParseColor {{{
  * @brief Parse and load color
  * @param[in]  disp  Display
  * @param[in]  name     Color string
  * @return Color pixel value
  **/

unsigned long
subSharedParseColor(Display *disp,
  char *name)
{
  XColor xcolor = { 0 }; ///< Default color

  assert(name);

  /* Parse and store color */
  if(!XParseColor(disp, DefaultColormap(disp, DefaultScreen(disp)),
      name, &xcolor))
    {
      fprintf(stderr, "<CRITICAL> Failed loading color `%s'\n", name);
    }
  else if(!XAllocColor(disp, DefaultColormap(disp, DefaultScreen(disp)),
      &xcolor))
    fprintf(stderr, "<CRITICAL> Failed allocating color `%s'\n", name);

  return xcolor.pixel;
} /* }}} */

 /** subSharedParseKey {{{
  * @brief Parse key
  * @param[in]     disp     Display
  * @param[in]     key      Key string
  * @param[inout]  code     Key code
  * @param[inout]  state    Key state
  * @param[inout]  mouse    Whether this is mouse press
  * @return Color keysym or \p NoSymbol
  **/

KeySym
subSharedParseKey(Display *disp,
  const char *key,
  unsigned int *code,
  unsigned int *state,
  int *mouse)
{
  KeySym sym = NoSymbol;
  char *tokens = NULL, *tok = NULL, *save = NULL;

  /* Parse keys */
  tokens = strdup(key);
  tok    = strtok_r(tokens, "-", &save);

  while(tok)
    {
      /* Eval keysyms */
      switch((sym = XStringToKeysym(tok)))
        {
          /* Unknown keys */
          case NoSymbol:
            if('B' == tok[0])
              {
                int buttonid = 0;

                sscanf(tok, "B%d", &buttonid);

                *mouse = True;
                *code  = XK_Pointer_Button1 + buttonid; ///< FIXME: Implementation independent?
                sym    = XK_Pointer_Button1;
              }
            else
              {
                free(tokens);

                return sym;
              }
            break;

          /* Modifier keys */
          case XK_S: *state |= ShiftMask;   break;
          case XK_C: *state |= ControlMask; break;
          case XK_A: *state |= Mod1Mask;    break;
          case XK_M: *state |= Mod3Mask;    break;
          case XK_W: *state |= Mod4Mask;    break;
          case XK_G: *state |= Mod5Mask;    break;

          /* Normal keys */
          default:
            *mouse = False;
            *code  = XKeysymToKeycode(disp, sym);
        }

      tok = strtok_r(NULL, "-", &save);
    }

  free(tokens);

  return sym;
} /* }}} */

 /** subSharedSpawn {{{
  * @brief Spawn a command
  * @param[in]  cmd  Command string
  **/

pid_t
subSharedSpawn(char *cmd)
{
  pid_t pid = fork();

  /* Handle fork pids */
  switch(pid)
    {
      case 0:
        setsid();
        execlp("/bin/sh", "sh", "-c", cmd, NULL);

        /* Never to be reached */
        fprintf(stderr, "<CRITICAL> Failed executing command `%s'\n", cmd);
        exit(1);
      case -1:
        fprintf(stderr, "<CRITICAL> Failed forking command `%s'\n", cmd);
    }

  return pid;
} /* }}} */

 /** subSharedStringWidth {{{
  * @brief Get width of the smallest enclosing box
  * @param[in]     disp    Display
  * @param[in]     text    The text
  * @param[in]     len     Length of the string
  * @param[inout]  left    Left bearing
  * @param[inout]  right   Right bearing
  * @param[in]     center  Center text
  * @return Width of the box
  **/

int
subSharedStringWidth(Display *disp,
  SubFont *f,
  const char *text,
  int len,
  int *left,
  int *right,
  int center)
{
  int width = 0, lbearing = 0, rbearing = 0;

  assert(f);

  /* Get text extents based on font */
  if(text && 0 < len)
    {
#ifdef HAVE_X11_XFT_XFT_H
      if(f->xft) ///< XFT
        {
          XGlyphInfo extents;

          XftTextExtentsUtf8(disp, f->xft, (XftChar8 *)text, len, &extents);

          width    = extents.xOff;
          lbearing = extents.x;
        }
      else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
        {
          XRectangle overall_ink = { 0 }, overall_logical = { 0 };

          XmbTextExtents(f->xfs, text, len,
            &overall_ink, &overall_logical);

          width    = overall_logical.width;
          lbearing = overall_logical.x;
        }

      /* Get left and right spacing */
      if(left)  *left  = lbearing;
      if(right) *right = rbearing;
    }

  return center ? width - abs(lbearing - rbearing) : width;
} /* }}} */

#ifndef SUBTLE

 /** subSharedMessage {{{
  * @brief Send client message to window
  * @param[in]  disp    Display
  * @param[in]  win     Client window
  * @param[in]  type    Message type
  * @param[in]  data    A #SubMessageData
  * @param[in]  format  Data format
  * @param[in]  xsync   Sync connection
  * @returns
  **/

int
subSharedMessage(Display *disp,
  Window win,
  char *type,
  SubMessageData data,
  int format,
  int xsync)
{
  int status = 0;
  XEvent ev;
  long mask = SubstructureRedirectMask|SubstructureNotifyMask;

  assert(disp && win);

  /* Assemble event */
  ev.xclient.type         = ClientMessage;
  ev.xclient.serial       = 0;
  ev.xclient.send_event   = True;
  ev.xclient.message_type = XInternAtom(disp, type, False);
  ev.xclient.window       = win;
  ev.xclient.format       = format;

  /* Copy data over */
  ev.xclient.data.l[0] = data.l[0];
  ev.xclient.data.l[1] = data.l[1];
  ev.xclient.data.l[2] = data.l[2];
  ev.xclient.data.l[3] = data.l[3];
  ev.xclient.data.l[4] = data.l[4];

  status = XSendEvent(disp, DefaultRootWindow(disp), False, mask, &ev);

  if(True == xsync) XSync(disp, False);

  return status;
} /* }}} */

#endif /* SUBTLE */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
