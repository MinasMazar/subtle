
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/display.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <X11/cursorfont.h>
#include <unistd.h>
#include <locale.h>
#include "subtle.h"

/* DisplayClaim {{{ */
int
DisplayClaim(void)
{
  int success = True;
  char buf[10] = { 0 };
  Atom session = None;
  Window owner = None;

  /* Get session atom */
  snprintf(buf, sizeof(buf), "WM_S%d", DefaultScreen(subtle->dpy));
  session = XInternAtom(subtle->dpy, buf, False);

  if((owner = XGetSelectionOwner(subtle->dpy, session)))
    {
      if(!(subtle->flags & SUB_SUBTLE_REPLACE))
        {
          subSubtleLogError("Found a running window manager\n");

          return False;
        }

      XSelectInput(subtle->dpy, owner, StructureNotifyMask);
      XSync(subtle->dpy, False);
    }

  /* Aquire session selection */
  XSetSelectionOwner(subtle->dpy, session,
    subtle->windows.support, CurrentTime);

  /* Wait for previous window manager to exit */
  if(XGetSelectionOwner(subtle->dpy, session) == subtle->windows.support)
    {
      if(owner)
        {
          int i;
          XEvent event;

          printf("Waiting for current window manager to exit\n");

          for(i = 0; i < WAITTIME; i++)
            {
              if(XCheckWindowEvent(subtle->dpy, owner,
                  StructureNotifyMask, &event) &&
                  DestroyNotify == event.type)
                return True;

              sleep(1);
            }

          subSubtleLogError("Giving up waiting for window managert\n");
          success = False;
        }
    }
  else
    {
      subSubtleLogWarn("Failed replacing current window manager\n");
      success = False;
    }

  return success;
} /* }}} */

/* DisplayStyleToColor {{{ */
static void
DisplayStyleToColor(SubStyle *s,
  unsigned long *colors,
  int *pos)
{
  colors[(*pos)++] = s->fg;
  colors[(*pos)++] = s->bg;
  colors[(*pos)++] = s->top;
  colors[(*pos)++] = s->right;
  colors[(*pos)++] = s->bottom;
  colors[(*pos)++] = s->left;
} /* }}} */

 /* DisplayXError {{{ */
static int
DisplayXError(Display *disp,
  XErrorEvent *ev)
{
#ifdef DEBUG
  if(subtle->loglevel & SUB_LOG_XERROR)
    {
      if(42 != ev->request_code) /* X_SetInputFocus */
        {
          char error[255] = { 0 };

          XGetErrorText(disp, ev->error_code, error, sizeof(error));
          subSubtleLog(SUB_LOG_XERROR, __FILE__, __LINE__,
            "%s: win=%#lx, request=%d\n",
            error, ev->resourceid, ev->request_code);
        }
    }
#endif /* DEBUG */

  return 0;
} /* }}} */

/* Public */

 /** subDisplayInit {{{
  * @brief Open connection to X server and create display
  * @param[in]  display  The display name as string
  **/

void
subDisplayInit(const char *display)
{
  XGCValues gvals;
  XSetWindowAttributes sattrs;
  unsigned long mask = 0;

#if defined HAVE_X11_EXTENSIONS_XINERAMA_H || defined HAVE_X11_EXTENSIONS_XRANDR_H
  int event = 0, junk = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H HAVE_X11_EXTENSIONS_XRANDR_H */

  assert(subtle);

  /* Set locale */
  if(!setlocale(LC_CTYPE, "")) XSupportsLocale();

  /* Connect to display and setup error handler */
  if(!(subtle->dpy = XOpenDisplay(display)))
    {
      subSubtleLogError("Cannot connect to display `%s'\n",
        (display) ? display : ":0.0");

      subSubtleFinish();

      exit(-1);
    }

  /* Create supporting window */
  subtle->windows.support = XCreateSimpleWindow(subtle->dpy, ROOT,
    -100, -100, 1, 1, 0, 0, 0);

  sattrs.override_redirect = True;
  sattrs.event_mask        = PropertyChangeMask;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.support,
    CWEventMask|CWOverrideRedirect, &sattrs);

  /* Claim and setup display */
  if(!DisplayClaim())
    {
      subSubtleFinish();

      exit(-1);
    }

  XSetErrorHandler(DisplayXError);
  setenv("DISPLAY", DisplayString(subtle->dpy), True); ///< Set display for clients

  /* Create GCs */
  gvals.fill_style     = FillStippled;
  mask                 = GCFillStyle;
  subtle->gcs.stipple  = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  gvals.function       = GXinvert;
  gvals.subwindow_mode = IncludeInferiors;
  gvals.line_width     = 3;
  mask                 = GCFunction|GCSubwindowMode|GCLineWidth;
  subtle->gcs.invert   = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  gvals.line_width     = 1;
  gvals.line_style     = LineSolid;
  gvals.join_style     = JoinMiter;
  gvals.cap_style      = CapButt;
  gvals.fill_style     = FillSolid;
  mask                 = GCLineWidth|GCLineStyle|GCJoinStyle|GCCapStyle|
    GCFillStyle;
  subtle->gcs.draw     = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  /* Create cursors */
  subtle->cursors.arrow  = XCreateFontCursor(subtle->dpy, XC_left_ptr);
  subtle->cursors.move   = XCreateFontCursor(subtle->dpy, XC_dotbox);
  subtle->cursors.resize = XCreateFontCursor(subtle->dpy, XC_sizing);

  /* Update root window */
  sattrs.cursor     = subtle->cursors.arrow;
  sattrs.event_mask = ROOTMASK;
  XChangeWindowAttributes(subtle->dpy, ROOT, CWCursor|CWEventMask, &sattrs);

  /* Create tray window */
  subtle->windows.tray = XCreateSimpleWindow(subtle->dpy,
    ROOT, 0, 0, 1, 1, 0, 0, 0);

  sattrs.override_redirect = True;
  sattrs.event_mask        = KeyPressMask|ButtonPressMask;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.tray,
    CWOverrideRedirect|CWEventMask, &sattrs);

  /* Init screen width and height */
  subtle->width  = DisplayWidth(subtle->dpy, DefaultScreen(subtle->dpy));
  subtle->height = DisplayHeight(subtle->dpy, DefaultScreen(subtle->dpy));

  /* Check extensions */
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  if(!XineramaQueryExtension(subtle->dpy, &event, &junk))
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */
    subtle->flags &= ~SUB_SUBTLE_XINERAMA;

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
  if(!XRRQueryExtension(subtle->dpy, &event, &junk))
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */
    subtle->flags &= ~SUB_SUBTLE_XRANDR;

  XSync(subtle->dpy, False);

  printf("Display (%s) is %dx%d\n", DisplayString(subtle->dpy),
    subtle->width, subtle->height);

  subSubtleLogDebugSubtle("Init\n");
} /* }}} */

 /** subDisplayConfigure {{{
  * @brief Configure display
  **/

void
subDisplayConfigure(void)
{
  XGCValues gvals;

  assert(subtle);

  /* Update GCs */
  gvals.foreground = subtle->styles.subtle.fg;
  gvals.line_width = subtle->styles.clients.border.top;
  XChangeGC(subtle->dpy, subtle->gcs.stipple,
    GCForeground|GCLineWidth, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->dpy, subtle->windows.tray,
    subtle->styles.subtle.top);

  /* Set background if set */
  if(-1 != subtle->styles.subtle.bg)
    XSetWindowBackground(subtle->dpy, ROOT, subtle->styles.subtle.bg);

  XClearWindow(subtle->dpy, subtle->windows.tray);
  XClearWindow(subtle->dpy, ROOT);

  /* Update struts and panels */
  subScreenResize();
  subScreenUpdate();

  XSync(subtle->dpy, False); ///< Sync all changes

  subSubtleLogDebugSubtle("Configure\n");
} /* }}} */

 /** subDisplayScan {{{
  * @brief Scan root window for clients
  **/

void
subDisplayScan(void)
{
  unsigned int i, nwins = 0;
  Window wroot = None, parent = None, *wins = NULL;

  assert(subtle);

  /* Scan for client windows */
  XQueryTree(subtle->dpy, ROOT, &wroot, &parent, &wins, &nwins);

  for(i = 0; i < nwins; i++)
    {
      SubClient *c = NULL;
      XWindowAttributes attrs;

      XGetWindowAttributes(subtle->dpy, wins[i], &attrs);
      switch(attrs.map_state)
        {
          case IsViewable:
            if((c = subClientNew(wins[i])))
              subArrayPush(subtle->clients, (void *)c);
            break;
        }
    }

  XFree(wins);

  subClientPublish(False);

  subSubtleLogDebugSubtle("Scan\n");
} /* }}} */

 /** subDisplayPublish {{{
  * @brief Update EWMH infos
  **/

void
subDisplayPublish(void)
{
  int pos = 0;
  unsigned long *colors;

#define NCOLORS 54

  /* Create color array */
  colors = (unsigned long *)subSharedMemoryAlloc(NCOLORS,
    sizeof(unsigned long));

  DisplayStyleToColor(&subtle->styles.title, colors, &pos);
  DisplayStyleToColor(&subtle->styles.views, colors, &pos);

  if(subtle->styles.focus)
    DisplayStyleToColor(subtle->styles.focus, colors, &pos);

  if(subtle->styles.urgent)
    DisplayStyleToColor(subtle->styles.urgent, colors, &pos);

  if(subtle->styles.occupied)
    DisplayStyleToColor(subtle->styles.occupied, colors, &pos);

  DisplayStyleToColor(&subtle->styles.sublets,   colors, &pos);
  DisplayStyleToColor(&subtle->styles.separator, colors, &pos);

  colors[pos++] = subtle->styles.clients.fg; ///< Active
  colors[pos++] = subtle->styles.clients.bg; ///< Inactive
  colors[pos++] = subtle->styles.subtle.top;
  colors[pos++] = subtle->styles.subtle.bottom;
  colors[pos++] = subtle->styles.subtle.bg;
  colors[pos++] = subtle->styles.subtle.fg;  ///< Stipple

  /* EWMH: Colors */
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_COLORS, (long *)colors, NCOLORS);

  free(colors);

  XSync(subtle->dpy, False); ///< Sync all changes

  subSubtleLogDebugSubtle("Publish: colors=%d\n", NCOLORS);
} /* }}} */

 /** subDisplayFinish {{{
  * @brief Close connection
  **/

void
subDisplayFinish(void)
{
  assert(subtle);

  if(subtle->dpy)
    {
      XSync(subtle->dpy, False); ///< Sync all changes

      /* Free cursors */
      if(subtle->cursors.arrow)  XFreeCursor(subtle->dpy, subtle->cursors.arrow);
      if(subtle->cursors.move)   XFreeCursor(subtle->dpy, subtle->cursors.move);
      if(subtle->cursors.resize) XFreeCursor(subtle->dpy, subtle->cursors.resize);

      /* Free GCs */
      if(subtle->gcs.stipple) XFreeGC(subtle->dpy, subtle->gcs.stipple);
      if(subtle->gcs.invert)  XFreeGC(subtle->dpy, subtle->gcs.invert);
      if(subtle->gcs.draw)    XFreeGC(subtle->dpy, subtle->gcs.draw);

      XDestroyWindow(subtle->dpy, subtle->windows.tray);
      XDestroyWindow(subtle->dpy, subtle->windows.support);

      XInstallColormap(subtle->dpy, DefaultColormap(subtle->dpy, SCRN));
      XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);
      XCloseDisplay(subtle->dpy);
    }

  subSubtleLogDebugSubtle("Finish\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
