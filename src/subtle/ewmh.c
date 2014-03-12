
 /**
  * @package subtle
  *
  * @file EWMH functions
  * @copyright Copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/ewmh.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <sys/types.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "subtle.h"

static Atom atoms[SUB_EWMH_TOTAL];

/* Typedef {{{ */
typedef struct xembedinfo_t
{
  CARD32 version, flags;
} XEmbedInfo; /* }}} */

 /** subEwmhInit {{{
  * @brief Init and register ICCCM/EWMH atoms
  **/

void
subEwmhInit(void)
{
  int len = 0;
  long data[2] = { 0, 0 }, pid = (long)getpid();
  char *selection = NULL, *names[] =
  {
    /* ICCCM */
    "WM_NAME", "WM_CLASS", "WM_STATE", "WM_PROTOCOLS", "WM_TAKE_FOCUS",
    "WM_DELETE_WINDOW", "WM_NORMAL_HINTS", "WM_SIZE_HINTS", "WM_HINTS",
    "WM_WINDOW_ROLE", "WM_CLIENT_LEADER",

    /* EWMH */
    "_NET_SUPPORTED", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING",
    "_NET_NUMBER_OF_DESKTOPS", "_NET_DESKTOP_NAMES", "_NET_DESKTOP_GEOMETRY",
    "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP", "_NET_ACTIVE_WINDOW",
    "_NET_WORKAREA", "_NET_SUPPORTING_WM_CHECK", "_NET_WM_FULL_PLACEMENT",
    "_NET_FRAME_EXTENTS",

    /* Client */
    "_NET_CLOSE_WINDOW", "_NET_RESTACK_WINDOW", "_NET_MOVERESIZE_WINDOW",
    "_NET_WM_NAME", "_NET_WM_PID", "_NET_WM_DESKTOP", "_NET_WM_STRUT",

    /* Types */
    "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_SPLASH", "_NET_WM_WINDOW_TYPE_DIALOG",

    /* States */
    "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_ABOVE",
    "_NET_WM_STATE_STICKY", "_NET_WM_STATE_DEMANDS_ATTENTION",

    /* Tray */
    "_NET_SYSTEM_TRAY_OPCODE", "_NET_SYSTEM_TRAY_MESSAGE_DATA",
    "_NET_SYSTEM_TRAY_S",

    /* Misc */
    "UTF8_STRING", "MANAGER", "_MOTIF_WM_HINTS",

    /* XEmbed */
    "_XEMBED", "_XEMBED_INFO",

    /* subtle */
    "SUBTLE_CLIENT_TAGS", "SUBTLE_CLIENT_RETAG",
    "SUBTLE_CLIENT_GRAVITY", "SUBTLE_CLIENT_SCREEN", "SUBTLE_CLIENT_FLAGS",
    "SUBTLE_GRAVITY_NEW", "SUBTLE_GRAVITY_FLAGS", "SUBTLE_GRAVITY_LIST",
    "SUBTLE_GRAVITY_KILL",
    "SUBTLE_TAG_NEW", "SUBTLE_TAG_LIST", "SUBTLE_TAG_KILL", "SUBTLE_TRAY_LIST",
    "SUBTLE_VIEW_NEW", "SUBTLE_VIEW_TAGS", "SUBTLE_VIEW_STYLE",
    "SUBTLE_VIEW_ICONS", "SUBTLE_VIEW_KILL",
    "SUBTLE_SUBLET_UPDATE", "SUBTLE_SUBLET_DATA", "SUBTLE_SUBLET_STYLE",
    "SUBTLE_SUBLET_FLAGS", "SUBTLE_SUBLET_LIST", "SUBTLE_SUBLET_KILL",
    "SUBTLE_SCREEN_PANELS", "SUBTLE_SCREEN_VIEWS", "SUBTLE_SCREEN_JUMP",
    "SUBTLE_VISIBLE_TAGS", "SUBTLE_VISIBLE_VIEWS",
    "SUBTLE_RENDER", "SUBTLE_RELOAD", "SUBTLE_RESTART", "SUBTLE_QUIT",
    "SUBTLE_COLORS", "SUBTLE_FONT", "SUBTLE_DATA", "SUBTLE_VERSION"
  };

  assert(SUB_EWMH_TOTAL == LENGTH(names));

  /* Update tray selection name for current screen */
  len       = strlen(names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION]) + 5; ///< For high screen counts
  selection = (char *)subSharedMemoryAlloc(len, sizeof(char));

  snprintf(selection, len, "%s%u", names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION], SCRN);
  subSubtleLogDebug("Selection: len=%d, name=%s\n", len, selection);
  names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION] = selection;

  /* Register atoms */
  XInternAtoms(subtle->dpy, names, SUB_EWMH_TOTAL, 0, atoms);
  subtle->flags |= SUB_SUBTLE_EWMH; ///< Set EWMH flag

  free(selection);

  /* EWMH: Supported hints */
  XChangeProperty(subtle->dpy, ROOT, atoms[SUB_EWMH_NET_SUPPORTED], XA_ATOM,
    32, PropModeReplace, (unsigned char *)&atoms, SUB_EWMH_TOTAL);

  /* EWMH: Window manager information */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_SUPPORTING_WM_CHECK,
    &subtle->windows.support, 1);
  subEwmhSetString(subtle->windows.support, SUB_EWMH_NET_WM_NAME, PKG_NAME);
  subEwmhSetString(subtle->windows.support, SUB_EWMH_WM_CLASS, PKG_NAME);
  subEwmhSetCardinals(subtle->windows.support, SUB_EWMH_NET_WM_PID, &pid, 1);
  subEwmhSetString(subtle->windows.support,
    SUB_EWMH_SUBTLE_VERSION, PKG_VERSION);

  /* EWMH: Desktop geometry */
  data[0] = subtle->width;
  data[1] = subtle->height;
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, NULL, 0);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, NULL, 0);

  subSubtleLogDebugSubtle("Init\n");
} /* }}} */

 /** subEwmhGet {{{
  * @brief Find intern atoms
  * @param[in]  e  A #SubEwmh
  * @return Returns the desired #Atom
  **/

Atom
subEwmhGet(SubEwmh e)
{
  assert(e <= SUB_EWMH_TOTAL);

  return atoms[e];
} /* }}} */

 /** subEwmhFind {{{
  * @brief Find id for intern atom
  * @param[in]  e  A #SubEwmh
  * @retval  >=0  Found index
  * @retval  -1   Atom was not found
  **/

SubEwmh
subEwmhFind(Atom atom)
{
  int i;

  for(i = 0; atom && i < SUB_EWMH_TOTAL; i++)
    if(atoms[i] == atom) return i;

  return -1;
} /* }}} */

 /** subEwmhGetWMState {{{
  * @brief Get WM state from window
  * @param[in]  win  A window
  * @return Returns window WM state
  **/

long
subEwmhGetWMState(Window win)
{
  Atom type = None;
  int format = 0;
  unsigned long unused, bytes;
  long *data = NULL, state = WithdrawnState;

  assert(win);

  if(Success == XGetWindowProperty(subtle->dpy, win, atoms[SUB_EWMH_WM_STATE], 0L, 2L, False,
      atoms[SUB_EWMH_WM_STATE], &type, &format, &bytes, &unused,
      (unsigned char **)&data) && bytes)
    {
      state = *data;
      XFree(data);
    }

  return state;
} /* }}} */

 /** subEwmhGetXEmbedState {{{
  * @brief Get window Xembed state
  * @param[in]  win  A window
  **/

long
subEwmhGetXEmbedState(Window win)
{
  long flags = 0;
  XEmbedInfo *info = NULL;

  /* Get xembed data */
  if((info = (XEmbedInfo *)subSharedPropertyGet(subtle->dpy, win,
      subEwmhGet(SUB_EWMH_XEMBED_INFO), subEwmhGet(SUB_EWMH_XEMBED_INFO),
      NULL)))
    {
      flags = (long)info->flags;

      subSubtleLogDebug("XEmbedInfo: win=%#lx, version=%ld, flags=%ld, mapped=%ld\n",
        win, info->version, info->flags, info->flags & XEMBED_MAPPED);

      XFree(info);
    }

  return flags;
} /* }}} */

 /** subEwmhSetWindows {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  e       A #SubEwmh
  * @param[in]  values  Window list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetWindows(Window win,
  SubEwmh e,
  Window *values,
  int size)
{
  XChangeProperty(subtle->dpy, win, atoms[e], XA_WINDOW, 32, PropModeReplace,
    (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetCardinals {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  e       A #SubEwmh
  * @param[in]  values  Cardinal list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetCardinals(Window win,
  SubEwmh e,
  long *values,
  int size)
{
  XChangeProperty(subtle->dpy, win, atoms[e], XA_CARDINAL, 32, PropModeReplace,
    (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetString {{{
  * @brief Change window property
  * @param[in]  win    Window
  * @param[in]  e      A #SubEwmh
  * @param[in]  value  String value
  **/

void
subEwmhSetString(Window win,
  SubEwmh e,
  char *value)
{
  XChangeProperty(subtle->dpy, win, atoms[e], atoms[SUB_EWMH_UTF8], 8,
    PropModeReplace, (unsigned char *)value, strlen(value));
} /* }}} */

 /** subEwmhSetWMState {{{
  * @brief Set WM state for window
  * @param[in]  win    A window
  * @param[in]  state  New state for the window
  **/

void
subEwmhSetWMState(Window win,
  long state)
{
  CARD32 data[2];
  data[0] = state;
  data[1] = None; /* No icons */

  assert(win);

  XChangeProperty(subtle->dpy, win, atoms[SUB_EWMH_WM_STATE],
    atoms[SUB_EWMH_WM_STATE], 32, PropModeReplace, (unsigned char *)data, 2);
} /* }}} */

 /** subEwmhTranslateWMState {{{
  * @brief Translate WM state to flags
  * @param[in]     atom   A window
  * @param[inout]  flags  Translated flags
  **/

void
subEwmhTranslateWMState(Atom atom,
  int *flags)
{
  /* Translate supported WM states */
  switch(subEwmhFind(atom))
    {
      case SUB_EWMH_NET_WM_STATE_FULLSCREEN:
        *flags |= SUB_CLIENT_MODE_FULL;
        break;
      case SUB_EWMH_NET_WM_STATE_ABOVE:
        *flags |= SUB_CLIENT_MODE_FLOAT;
        break;
      case SUB_EWMH_NET_WM_STATE_STICKY:
        *flags |= SUB_CLIENT_MODE_STICK;
        break;
      case SUB_EWMH_NET_WM_STATE_ATTENTION:
        *flags |= SUB_CLIENT_MODE_URGENT;
        break;
      default: break;
    }
} /* }}} */

 /** subEwmhTranslateClientMode {{{
  * @brief Translate mode flags to ewmh flags
  * @param[in]     client_flags  Client mode flags
  * @param[inout]  flags         Translated flags
  **/

void
subEwmhTranslateClientMode(int client_flags,
  int *flags)
{
  /* Translate supported mode flags */
  if(client_flags & SUB_CLIENT_MODE_FULL)       *flags |= SUB_EWMH_FULL;
  if(client_flags & SUB_CLIENT_MODE_FLOAT)      *flags |= SUB_EWMH_FLOAT;
  if(client_flags & SUB_CLIENT_MODE_STICK)      *flags |= SUB_EWMH_STICK;
  if(client_flags & SUB_CLIENT_MODE_RESIZE)     *flags |= SUB_EWMH_RESIZE;
  if(client_flags & SUB_CLIENT_MODE_URGENT)     *flags |= SUB_EWMH_URGENT;
  if(client_flags & SUB_CLIENT_MODE_ZAPHOD)     *flags |= SUB_EWMH_ZAPHOD;
  if(client_flags & SUB_CLIENT_MODE_FIXED)      *flags |= SUB_EWMH_FIXED;
  if(client_flags & SUB_CLIENT_MODE_BORDERLESS) *flags |= SUB_EWMH_BORDERLESS;
} /* }}} */

 /** subEwmhMessage {{{
  * @brief Send message
  * @param[in]  win    Target window
  * @param[in]  e      A #SubEwmh
  * @param[in]  mask   Event mask
  * @param[in]  data0  Data part 1
  * @param[in]  data1  Data part 2
  * @param[in]  data2  Data part 3
  * @param[in]  data3  Data part 4
  * @param[in]  data4  Data part 5
  * @retval  0   Error
  * @retval  >0  Success
  **/

int
subEwmhMessage(Window win,
  SubEwmh e,
  long mask,
  long data0,
  long data1,
  long data2,
  long data3,
  long data4)
{
  XClientMessageEvent ev;

  /* Assemble message */
  ev.type         = ClientMessage;
  ev.message_type = atoms[e];
  ev.window       = win;
  ev.format       = 32;
  ev.data.l[0]    = data0;
  ev.data.l[1]    = data1;
  ev.data.l[2]    = data2;
  ev.data.l[3]    = data3;
  ev.data.l[4]    = data4;

  return XSendEvent(subtle->dpy, win, False, mask, (XEvent *)&ev);
} /* }}} */

 /** subEwmhFinish {{{
  * @brief Delete set ICCCM/EWMH atoms
  **/

void
subEwmhFinish(void)
{
  /* Delete root properties on real shutdown */
  if(subtle->flags & SUB_SUBTLE_EWMH)
    {
      /* EWMH properties */
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_SUPPORTED));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_SUPPORTING_WM_CHECK));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_ACTIVE_WINDOW));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_CURRENT_DESKTOP));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_DESKTOP_NAMES));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_NUMBER_OF_DESKTOPS));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_DESKTOP_VIEWPORT));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_DESKTOP_GEOMETRY));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_WORKAREA));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_CLIENT_LIST));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_NET_CLIENT_LIST_STACKING));

      /* subtle extension */
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_GRAVITY_LIST));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_TAG_LIST));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_TRAY_LIST));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_VIEW_TAGS));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_COLORS));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_FONT));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_SUBLET_LIST));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_SCREEN_VIEWS));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_VISIBLE_VIEWS));
      subSharedPropertyDelete(subtle->dpy, ROOT, subEwmhGet(SUB_EWMH_SUBTLE_VISIBLE_TAGS));
    }

  subSubtleLogDebugSubtle("Finish\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
