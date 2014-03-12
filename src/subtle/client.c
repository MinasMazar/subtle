
 /**
  * @package subtle
  *
  * @file Client functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/client.c,v 3209 2012/05/22 23:44:10 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <X11/Xatom.h>
#include "subtle.h"

/* Flags {{{ */
#define EDGE_LEFT   (1L << 0)
#define EDGE_RIGHT  (1L << 1)
#define EDGE_TOP    (1L << 2)
#define EDGE_BOTTOM (1L << 3)
/* }}} */

/* Typedef {{{ */
typedef struct clientmwmhints_t
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
} ClientMWMHints;
/* }}} */

/* Private */

/* ClientMask {{{ */
static void
ClientMask(XRectangle *geom)
{
  XDrawRectangle(subtle->dpy, ROOT, subtle->gcs.invert, geom->x - 1, geom->y - 1,
    geom->width + 1, geom->height + 1);
} /* }}} */

/* ClientGravity {{{ */
int
ClientGravity(void)
{
  int grav = 0;
  SubClient *c = NULL;

  /* Default gravity */
  if(-1 == subtle->gravity)
    {
      /* Copy gravity from current client */
      if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
        grav = c->gravityid;
    }
  else grav = subtle->gravity; ///< Set default

  return grav;
} /* }}} */

/* ClientBounds {{{ */
static void
ClientBounds(SubClient *c,
  XRectangle *bounds,
  XRectangle *geom,
  int adjustx,
  int adjusty)
{
  DEAD(c);
  assert(c && geom);

  /* Check size hints */
  if(!(c->flags & SUB_CLIENT_MODE_FIXED) &&
      (subtle->flags & SUB_SUBTLE_RESIZE ||
      c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_RESIZE)))
    {
      int bw = 0, maxw = 0, maxh = 0, diffw = 0, diffh = 0;

      /* Calculate max width and max height for bounds */
      bw   = 2 * BORDER(c) + subtle->styles.clients.margin.left +
        subtle->styles.clients.margin.right; 
      maxw = -1 == c->maxw ? bounds->width  - bw : c->maxw;
      maxh = -1 == c->maxh ? bounds->height - bw : c->maxh;

      /* Limit width and height */
      if(geom->width < c->minw)  geom->width  = c->minw;
      if(geom->width > maxw)     geom->width  = maxw;
      if(geom->height < c->minh) geom->height = c->minh;
      if(geom->height > maxh)    geom->height = maxh;

      /* Adjust based on increment values (see ICCCM 4.1.2.3) */
      diffw = (geom->width  - c->basew) % c->incw;
      diffh = (geom->height - c->baseh) % c->inch;

      /* Adjust x and/or y */
      if(adjustx) geom->x += diffw;
      if(adjusty) geom->y += diffh;

      /* Center client on current gravity */
      if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
        {
          geom->x += 0 < diffw ? diffw / 2 : 0;
          geom->y += 0 < diffh ? diffh / 2 : 0;
        }

      geom->width  -= diffw;
      geom->height -= diffh;

      /* Check aspect ratios */
      if(c->minr && geom->height * c->minr > geom->width)
        geom->width = (int)(geom->height * c->minr);

      if(c->maxr && geom->height * c->maxr < geom->width)
        geom->width = (int)(geom->height * c->maxr);
    }
} /* }}} */

/* ClientSnap {{{ */
static void
ClientSnap(SubClient *c,
  SubScreen *s,
  XRectangle *geom)
{
  DEAD(c);
  assert(c && s && geom);

  /* Snap to screen border when value is in snap margin - X axis */
  if(abs(s->geom.x - geom->x) <= subtle->snap)
    geom->x = s->geom.x + BORDER(c);
  else if(abs((s->geom.x + s->geom.width) -
      (geom->x + geom->width + BORDER(c))) <= subtle->snap)
    {
      geom->x = s->geom.x + s->geom.width -
        geom->width - BORDER(c);
    }

  /* Snap to screen border when is in snap margin - Y axis */
  if(abs(s->geom.y - geom->y) <= subtle->snap)
    geom->y = s->geom.y + BORDER(c);
  else if(abs((s->geom.y + s->geom.height) -
      (geom->y + geom->height + BORDER(c))) <= subtle->snap)
    {
      geom->y = s->geom.y + s->geom.height -
        geom->height - BORDER(c);
    }
} /* }}} */

/* ClientResize {{{ */
static void
ClientResize(SubClient *c,
  XRectangle *bounds)
{
  assert(c);

  /* Update border and gap */
  c->geom.x      += subtle->styles.clients.margin.left;
  c->geom.y      += subtle->styles.clients.margin.top;
  c->geom.width  -= (2 * BORDER(c) + subtle->styles.clients.margin.left +
    subtle->styles.clients.margin.right);
  c->geom.height -= (2 * BORDER(c) + subtle->styles.clients.margin.top +
    subtle->styles.clients.margin.bottom);

  subClientResize(c, bounds, True);
  XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
    c->geom.width, c->geom.height);
} /* }}} */

/* ClientTile {{{ */
static void
ClientTile(int gravity,
  int screen)
{
  int i, used = 0, pos = 0, calc = 0, fix = 0;
  XRectangle geom = { 1 };
  SubScreen *s = SCREEN(subArrayGet(subtle->screens, screen));
  SubGravity *g = GRAVITY(subArrayGet(subtle->gravities, gravity));

  /* Pass 1: Count clients with this gravity */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      if(c->gravityid == gravity && c->screenid == screen &&
        subtle->visible_tags & c->tags &&
        !(c->flags &(SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL))) used++;
    }

  if(0 == used || !s || !g) return;

  /* Calculate tiled gravity value and rounding fix */
  subGravityGeometry(g, &(s->geom), &geom);

  if(g->flags & SUB_GRAVITY_HORZ)
    {
      calc = geom.width / used;
      fix  = geom.width - calc * used;
    }
  else
    {
      calc = geom.height / used;
      fix  = geom.height - calc * used;
    }

  /* Pass 2: Update geometry of every client with this gravity */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      if(c->gravityid == gravity && c->screenid == screen &&
          subtle->visible_tags & c->tags &&
          !(c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL)))
        {
          if(g->flags & SUB_GRAVITY_HORZ)
            {
              c->geom.width  = pos == used ? calc + fix : calc;
              c->geom.height = geom.height;
              c->geom.x      = geom.x + pos++ * calc;
              c->geom.y      = geom.y;
            }
          else
            {
              c->geom.width  = geom.width;
              c->geom.height = pos == used ? calc + fix : calc;
              c->geom.x      = geom.x;
              c->geom.y      = geom.y + pos++ * calc;
            }

          ClientResize(c, &(s->geom));
        }
    }
} /* }}} */

/* ClientZaphod {{{ */
static void
ClientZaphod(SubClient *c,
  XRectangle *bounds)
{
  int i, flags = (SUB_SCREEN_PANEL1|SUB_SCREEN_PANEL2);

  /* Update bounds according to styles */
  bounds->x      = subtle->styles.subtle.padding.left;
  bounds->y      = subtle->styles.subtle.padding.top;
  bounds->width  = subtle->width - subtle->styles.subtle.padding.left -
    subtle->styles.subtle.padding.right;
  bounds->height = subtle->height - subtle->styles.subtle.padding.top -
    subtle->styles.subtle.padding.bottom;

  /* Iterate over screens to find fitting square */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s2 = SCREEN(subtle->screens->data[i]);

      if(s2->flags & flags)
        {
          if(s2->flags & SUB_SCREEN_PANEL1)
            {
              bounds->y      += subtle->ph;
              bounds->height -= subtle->ph;
            }

          if(s2->flags & SUB_SCREEN_PANEL2)
            bounds->height -= subtle->ph;

          flags &= ~(s2->flags &
            (SUB_SCREEN_PANEL1|SUB_SCREEN_PANEL2));
        }
    }
} /* }}} */

/* ClientCompare {{{ */
static int
ClientCompare(const void *a,
  const void *b)
{
  int ret = 0, dirret = 0;
  SubClient *c1 = *(SubClient **)a, *c2 = *(SubClient **)b;

  assert(a && b);

  /* Direction is required when we change stacking on a level */
  if(SUB_CLIENT_RESTACK_DOWN      == c1->dir) dirret = -1;
  else if(SUB_CLIENT_RESTACK_UP   == c1->dir) dirret = 1;
  else if(SUB_CLIENT_RESTACK_DOWN == c2->dir) dirret = 1;
  else if(SUB_CLIENT_RESTACK_UP   == c2->dir) dirret = -1;

  /* Complicated comparisons to ensure stacking order. Our desired
   * order is following: desktop < gravity < float < full
   *
   * This function returns following values:
   * -1 => c1 is on a lower level
   *  0 => c1 and c2 are on the same level
   *  1 => c1 is on a higher level */
  if(c1->flags & SUB_CLIENT_TYPE_DESKTOP)
    {
      if(c2->flags & SUB_CLIENT_TYPE_DESKTOP) ret = dirret;
      else ret = 0;
    }
  else if(c1->flags & SUB_CLIENT_MODE_FULL)
    {
      if(c2->flags & SUB_CLIENT_MODE_FULL) ret = dirret;
      else ret = 1;
    }
  else if(c1->flags & SUB_CLIENT_MODE_FLOAT)
    {
      if(c2->flags & SUB_CLIENT_MODE_FULL) ret = -1;
      else if(c2->flags & SUB_CLIENT_MODE_FLOAT) ret = dirret;
      else ret = 1;
    }
  else
    {
      if(c2->flags & SUB_CLIENT_TYPE_DESKTOP) ret = 1;
      else if(c2->flags & (SUB_CLIENT_MODE_FLOAT|
          SUB_CLIENT_MODE_FULL)) ret = -1;
      else ret = dirret;
    }

  return ret;
} /* }}} */

/* Public */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Client window
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, grav = 0, flags = 0;
  long vid = 0, extents[4] = { 0 };
  XWindowAttributes attrs;
  XSetWindowAttributes sattrs;
  Window *leader = NULL;
  SubClient *c = NULL;

  assert(win);

  /* Check override_redirect */
  XGetWindowAttributes(subtle->dpy, win, &attrs);
  if(True == attrs.override_redirect) return NULL;

  /* Create new client */
  c = CLIENT(subSharedMemoryAlloc(1, sizeof(SubClient)));
  c->gravities = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));
  c->flags     = (SUB_TYPE_CLIENT|SUB_CLIENT_INPUT);
  c->gravityid = -1; ///< Force update
  c->dir       = -1;
  c->win       = win;

  /* Window attributes */
  c->cmap        = attrs.colormap;
  c->geom.x      = attrs.x;
  c->geom.y      = attrs.y;
  c->geom.width  = MAX(MINW, attrs.width);
  c->geom.height = MAX(MINH, attrs.height);

  /* Init gravities */
  grav = ClientGravity();
  for(i = 0; i < subtle->views->ndata; i++)
    c->gravities[i] = grav;

   /* Fetch name, instance, class and role */
  subSharedPropertyClass(subtle->dpy, c->win, &c->instance, &c->klass);
  subSharedPropertyName(subtle->dpy, c->win, &c->name, c->klass);
  c->role = subSharedPropertyGet(subtle->dpy, c->win, XA_STRING,
    subEwmhGet(SUB_EWMH_WM_WINDOW_ROLE), NULL);

  /* X properties */
  sattrs.border_pixel = subtle->styles.clients.bg; ///< Inactive
  sattrs.event_mask   = CLIENTMASK;
  XChangeWindowAttributes(subtle->dpy, c->win,
    CWBorderPixel|CWEventMask, &sattrs);
  XAddToSaveSet(subtle->dpy, c->win);
  XSaveContext(subtle->dpy, c->win, CLIENTID, (void *)c);
  XSetWindowBorderWidth(subtle->dpy, c->win,
    subtle->styles.clients.border.top);

  /* Update client */
  subEwmhSetWMState(c->win, WithdrawnState);
  subClientSetProtocols(c);
  subClientSetStrut(c);
  subClientSetType(c, &flags);
  subClientRetag(c, &flags);
  subClientSetSizeHints(c, &flags);
  subClientSetWMHints(c, &flags);
  subClientSetState(c, &flags);
  subClientSetTransient(c, &flags);
  subClientSetMWMHints(c);
  subClientToggle(c, flags, False);
  subGrabUnset(c->win);

  /* Set leader window */
  if((leader = (Window *)subSharedPropertyGet(subtle->dpy, c->win, XA_WINDOW,
      subEwmhGet(SUB_EWMH_WM_CLIENT_LEADER), NULL)))
    {
      c->leader = *leader;

      free(leader);
    }

  /* EWMH: Gravity, screen, desktop, extents */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_GRAVITY,
    (long *)&subtle->gravity, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_SCREEN,
    (long *)&c->screenid, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_FRAME_EXTENTS, extents, 4);

  subSubtleLogDebugSubtle("New: name=%s, instance=%s, "
    "class=%s, win=%#lx, input=%d, focus=%d\n",
    c->name, c->instance, c->klass, win, !!(c->flags & SUB_CLIENT_INPUT),
    !!(c->flags & SUB_CLIENT_FOCUS));

  return c;
} /* }}} */

 /** subClientConfigure {{{
  * @brief Send a configure request to client
  * @param[in]  c  A #SubClient
  **/

void
subClientConfigure(SubClient *c)
{
  XConfigureEvent ev;

  assert(c);
  DEAD(c);

  /* Assemble event */
  ev.type              = ConfigureNotify;
  ev.event             = c->win;
  ev.window            = c->win;
  ev.x                 = c->geom.x;
  ev.y                 = c->geom.y;
  ev.width             = c->geom.width;
  ev.height            = c->geom.height;
  ev.border_width      = subtle->styles.clients.border.top;
  ev.above             = None;
  ev.override_redirect = False;

  XSendEvent(subtle->dpy, c->win, False, StructureNotifyMask, (XEvent *)&ev);

  subSubtleLogDebugSubtle("Configure: win=%#lx "
    "x=%03d, y=%03d, width=%03d, height=%03d\n",
    c->win, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
} /* }}} */

 /** subClientDimension {{{
  * @brief Redimension clients
  * @param[in]  id  View id
  **/

void
subClientDimension(int id)
{
  int i, j;

  /* Update all clients */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      /* Shift if necessary */
      for(j = id; -1 < id && j < subtle->views->ndata; j++)
        c->gravities[j] = c->gravities[j + 1];

      /* Resize array */
      c->gravities = (int *)subSharedMemoryRealloc((void *)c->gravities,
        subtle->views->ndata * sizeof(int));

      if(-1 == id) ///< Initialize
        c->gravities[subtle->views->ndata - 1] = ClientGravity();
    }
} /* }}} */

 /** subClientFocus {{{
  * @brief Set focus to client
  * @param[in]  c     A #SubClient
  * @param[in]  warp  Whether to warp pointer to window
  **/

void
subClientFocus(SubClient *c,
  int warp)
{
  SubScreen *s = NULL;
  SubView *v = NULL;
  SubClient *focus = NULL;

  DEAD(c);
  assert(c);

  if(!VISIBLE(c)) return;

  /* Remove urgent after getting focus */
  if(c->flags & SUB_CLIENT_MODE_URGENT)
    {
      c->flags            &= ~SUB_CLIENT_MODE_URGENT;
      subtle->urgent_tags &= ~c->tags;
    }

  /* Unset current focus */
  if((focus = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
    {
      int i;

      subGrabUnset(focus->win);

      /* Reorder focus history */
      for(i = (HISTORYSIZE - 1); 0 < i; i--)
        subtle->windows.focus[i] = subtle->windows.focus[i - 1];
      subtle->windows.focus[0] = 0;

      /* Exclude desktop type windows */
      if(!(focus->flags & SUB_CLIENT_TYPE_DESKTOP))
        XSetWindowBorder(subtle->dpy, focus->win, subtle->styles.clients.bg);
    }

  /* Check client input focus type (see ICCCM 4.1.7, 4.1.2.7, 4.2.8) */
  if(!(c->flags & SUB_CLIENT_INPUT) && c->flags & SUB_CLIENT_FOCUS)
    {
      subEwmhMessage(c->win, SUB_EWMH_WM_PROTOCOLS, NoEventMask,
        subEwmhGet(SUB_EWMH_WM_TAKE_FOCUS), CurrentTime, 0, 0, 0);
    }
  else if(c->flags & SUB_CLIENT_INPUT)
    XSetInputFocus(subtle->dpy, c->win, RevertToPointerRoot, CurrentTime);

  /* Update focus */
  subtle->windows.focus[0] = c->win;
  subGrabSet(c->win, SUB_GRAB_MOUSE);

  /* Exclude desktop and dock type windows */
  if(!(c->flags & (SUB_CLIENT_TYPE_DESKTOP|SUB_CLIENT_TYPE_DOCK)))
    XSetWindowBorder(subtle->dpy, c->win, subtle->styles.clients.fg);

  /* EWMH: Active window */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW,
    subtle->windows.focus, HISTORYSIZE);

  /* EWMH: Current desktop */
  if((s = SCREEN(subArrayGet(subtle->screens, c->screenid))))
    {
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP,
        (long *)&s->viewid, 1);
    }

  /* Set view focus */
  if((v = VIEW(subArrayGet(subtle->views, s->viewid))))
    v->focus = c->win;;

  /* Hook: Focus */
  subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_FOCUS),
    (void *)c);

  /* Warp pointer */
  if(warp && !(subtle->flags & SUB_SUBTLE_SKIP_WARP)) subClientWarp(c);

  /* Update screen */
  subScreenUpdate();
  subScreenRender();
} /* }}} */

 /** subClientNext {{{
  * @brief Find next client and set focus to it
  * @param[in]  screenid  Screen id
  * @param[in]  jump      Jump to other screen
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNext(int screenid,
  int jump)
{
  int i;
  SubClient *c = NULL;

  /* Pass 1: Check focus history of current screen */
  for(i = 1; i < HISTORYSIZE; i++)
    {
      if((c = CLIENT(subSubtleFind(subtle->windows.focus[i], CLIENTID))))
        {
          /* Check visibility on current screen */
          if(c->screenid == screenid && ALIVE(c) && VISIBLE(c) &&
              c->win != subtle->windows.focus[0])
            return c;
        }
    }

  /* Pass 2: Check client stacking list backwards of current screen */
  for(i = subtle->clients->ndata - 1; 0 <= i; i--)
    {
      c = CLIENT(subtle->clients->data[i]);

      /* Check visibility on current screen */
      if(c->screenid == screenid && ALIVE(c) && VISIBLE(c) &&
          c->win != subtle->windows.focus[0])
        return c;
    }

  /* Pass 3: Check client stacking list backwards of any visible screen */
  if(1 < subtle->screens->ndata && jump)
    {
      for(i = subtle->clients->ndata - 1; 0 <= i; i--)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Check visibility on current screen */
          if(ALIVE(c) && VISIBLE(c) &&
              c->win != subtle->windows.focus[0])
            return c;
        }
    }

  return NULL;
} /* }}} */

 /** subClientWarp {{{
  * @brief Warp pointer to window center
  * @param[in]  c  A #SubClient
  **/

void
subClientWarp(SubClient *c)
{
  DEAD(c);
  assert(c);

  /* Move pointer to window center */
  XWarpPointer(subtle->dpy, None, ROOT, 0, 0, 0, 0,
    c->geom.x + c->geom.width / 2, c->geom.y + c->geom.height / 2);

  subSubtleLogDebugSubtle("Warp\n");
} /* }}} */

 /** subClientDrag {{{
  * @brief Move and/or drag client
  * @param[in]  c          A #SubClient
  * @param[in]  mode       Drag/move mode
  * @param[in]  direction  Resize/drag direction
  **/

void
subClientDrag(SubClient *c,
  int mode,
  int direction)
{
  XEvent ev;
  Window root = None, win = None;
  unsigned int mask = 0;
  int loop = True, edge = 0, fx = 0, fy = 0, dx = 0, dy = 0;
  int wx = 0, wy = 0, ww = 0, wh = 0, rx = 0, ry = 0;
  SubScreen *s = NULL;
  XRectangle geom = { 0 };
  Cursor cursor;

  DEAD(c);
  assert(c);

  /* Init {{{ */
  XQueryPointer(subtle->dpy, c->win, &root, &win, &rx, &ry, &wx, &wy, &mask);
  geom.x      = rx - wx;
  geom.y      = ry - wy;
  geom.width  = ww = c->geom.width;
  geom.height = wh = c->geom.height;

  /* Set max width/height */
  s = SCREEN(subtle->screens->data[c->screenid]);

  /* Set variables according to mode */
  switch(mode)
    {
      case SUB_DRAG_MOVE:
        cursor = subtle->cursors.move;
        break;
      case SUB_DRAG_RESIZE:
        cursor = subtle->cursors.resize;

        /* Select starting edge */
        edge |= (wx < (geom.width  / 2)) ? EDGE_LEFT : EDGE_RIGHT;
        edge |= (wy < (geom.height / 2)) ? EDGE_TOP  : EDGE_BOTTOM;

        /* Set starting point */
        if(edge & EDGE_LEFT)
          {
            fx = geom.x + geom.width;
            dx = rx - c->geom.x;
          }
        else if(edge & EDGE_RIGHT)
          {
            fx = geom.x;
            dx = geom.x + geom.width - rx;
          }
        if(edge & EDGE_TOP)
          {
            fy = geom.y + geom.height;
            dy = ry - c->geom.y;
          }
        else if(edge & EDGE_BOTTOM)
          {
            fy = geom.y;
            dy = geom.y + geom.height - ry;
          }
        break;
    } /* }}} */

  /* Grab pointer server */
  XGrabPointer(subtle->dpy, c->win, True, GRABMASK, GrabModeAsync,
    GrabModeAsync, None, cursor, CurrentTime);
  XGrabServer(subtle->dpy);

  switch(direction)
    {
      case SUB_GRAB_DIRECTION_UP: /* {{{ */
        if(SUB_DRAG_RESIZE == mode)
          {
            c->geom.y      -= c->inch;
            c->geom.height += c->inch;
          }
        else c->geom.y -= subtle->step;

        ClientSnap(c, s, &c->geom);
        ClientBounds(c, &(s->geom), &c->geom, False, False);
        break; /* }}} */
      case SUB_GRAB_DIRECTION_RIGHT: /* {{{ */
        if(SUB_DRAG_RESIZE == mode)
          c->geom.width += c->incw;
        else c->geom.x += subtle->step;

        ClientSnap(c, s, &c->geom);
        ClientBounds(c, &(s->geom), &c->geom, False, False);
        break; /* }}} */
      case SUB_GRAB_DIRECTION_DOWN: /* {{{ */
        if(SUB_DRAG_RESIZE == mode)
          c->geom.height += c->inch;
        else c->geom.y += subtle->step;

        ClientSnap(c, s, &c->geom);
        ClientBounds(c, &(s->geom), &c->geom, False, False);
        break; /* }}} */
      case SUB_GRAB_DIRECTION_LEFT: /* {{{ */
        if(SUB_DRAG_RESIZE == mode)
          {
            c->geom.x     -= c->incw;
            c->geom.width += c->incw;
          }
        else c->geom.x -= subtle->step;

        ClientSnap(c, s, &c->geom);
        ClientBounds(c, &(s->geom), &c->geom, False, False);
        break; /* }}}*/
      default: /* {{{ */
        ClientMask(&geom);

        /* Start event loop */
        while(loop)
          {
            XMaskEvent(subtle->dpy, DRAGMASK, &ev);
            switch(ev.type)
              {
                case EnterNotify:   win = ev.xcrossing.window; break; ///< Find destination window
                case ButtonRelease: loop = False;              break;
                case FocusIn:
                case FocusOut:                                 break; ///< Ignore focus changes
                case MotionNotify: /* {{{ */
                  if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
                    {
                      /* Check values */
                      if(!XYINRECT(ev.xmotion.x_root - dx,
                          ev.xmotion.y_root - dy, s->geom))
                        continue;

                      ClientMask(&geom);

                      /* Calculate selection rect */
                      switch(mode)
                        {
                          case SUB_DRAG_MOVE: /* {{{ */
                            geom.x = (rx - wx) - (rx - ev.xmotion.x_root);
                            geom.y = (ry - wy) - (ry - ev.xmotion.y_root);

                            ClientSnap(c, s, &geom);
                            break; /* }}} */
                          case SUB_DRAG_RESIZE: /* {{{ */
                            /* Handle resize based on edge */
                            if(edge & EDGE_LEFT)
                              {
                                geom.x     = ev.xmotion.x_root - dx;
                                geom.width = fx - ev.xmotion.x_root + dx;
                              }
                            else if(edge & EDGE_RIGHT)
                              {
                                geom.x     = fx;
                                geom.width = ev.xmotion.x_root - fx + dx;
                              }
                            if(edge & EDGE_TOP)
                              {
                                geom.y      = ev.xmotion.y_root - dy;
                                geom.height = fy - ev.xmotion.y_root + dy;
                              }
                            else if(edge & EDGE_BOTTOM)
                              {
                                geom.y      = fy;
                                geom.height = ev.xmotion.y_root - fy + dy;
                              }

                            /* Adjust bounds based on edge */
                            ClientBounds(c, &(s->geom), &geom,
                              (edge & EDGE_LEFT), (edge & EDGE_TOP));
                            break; /* }}} */
                        }

                      ClientMask(&geom);
                    }
                  break; /* }}} */
              }
          }

        ClientMask(&geom); ///< Erase mask

        /* Subtract border width */
        if(!(c->flags & SUB_CLIENT_MODE_BORDERLESS))
          {
            geom.x -= subtle->styles.clients.border.top;
            geom.y -= subtle->styles.clients.border.top;
          } /* }}} */

        c->geom = geom;
    }

  XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
    c->geom.width, c->geom.height);

  /* Remove grabs */
  XUngrabPointer(subtle->dpy, CurrentTime);
  XUngrabServer(subtle->dpy);
} /* }}} */

 /** subClientTag {{{
  * @brief Set tag properties to client
  * @param[in]     c      A #SubClient
  * @param[in]     tag    Tag id
  * @param[inout]  flags  Mode flags
  * @return Return changed flags
  **/

void
subClientTag(SubClient *c,
  int tag,
  int *flags)
{
  SubTag *t = NULL;

  DEAD(c);
  assert(c);

  /* Update flags and tags */
  if((t = TAG(subArrayGet(subtle->tags, tag))))
    {
      int i;

      /* Collect flags and tags */
      *flags  |= (t->flags & (TYPES_ALL|MODES_ALL));
      c->tags |= (1L << (tag + 1));

      /* Set size/position and enable float */
      if(t->flags & (SUB_TAG_GEOMETRY|SUB_TAG_POSITION))
        {
          *flags |= SUB_CLIENT_MODE_FLOAT; ///< Disable size checks

          /* Apply size/position */
          if(t->flags & SUB_TAG_GEOMETRY)
            c->geom  = t->geom;
          else
            {
              c->geom.x = t->geom.x;
              c->geom.y = t->geom.y;
            }
        }

      /* Set screen */
      if(t->flags & SUB_CLIENT_MODE_STICK && -1 != t->screenid)
        {
          c->flags    |= SUB_CLIENT_MODE_STICK_SCREEN;
          c->screenid  = t->screenid;
        }

      /* Set gravity matching views */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          /* Match views with this tag or sticky only */
          if(v->tags & (1L << (tag + 1)) || t->flags & SUB_CLIENT_MODE_STICK)
            if(t->flags & SUB_TAG_GRAVITY) c->gravities[i] = t->gravityid;
        }

      /* Call proc if any */
      if(t->flags & SUB_TAG_PROC)
        subRubyCall(SUB_CALL_HOOKS, t->proc, (void *)c);
    }
} /* }}} */

 /** subClientRetag {{{
  * @brief Set client tags
  * @param[in]     c      A #SubClient
  * @param[inout]  flags  Mode flags
  **/

void
subClientRetag(SubClient *c,
  int *flags)
{
  int i;

  DEAD(c);
  assert(c);

  c->tags = 0; ///< Reset tags

  /* Check matching tags */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      /* Check if tag matches client */
      if(subTagMatcherCheck(TAG(subtle->tags->data[i]), c))
        subClientTag(c, i, flags);
    }

  /* Check if client is visible on at least one screen w/o stick */
  if(!(c->flags & SUB_CLIENT_MODE_STICK) && !(*flags & SUB_CLIENT_MODE_STICK))
    {
      int visible = 0;

      for(i = 0; i < subtle->views->ndata; i++)
        {
          if(VIEW(subtle->views->data[i])->tags & c->tags)
            {
              visible++;
              break;
            }
        }

      if(0 == visible) subClientTag(c, 0, flags); ///< Set default tag
    }

  /* EWMH: Tags */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
} /* }}} */

 /** subClientResize {{{
  * @brief Resize client for screen
  * @param[in]  c           A #SubClient
  * @param[in]  bounds      A #XRectangle
  * @param[in]  size_hints  Apply size hints
  **/

void
subClientResize(SubClient *c,
  XRectangle *bounds,
  int size_hints)
{
  DEAD(c);
  assert(c);

  /* Honor size hints */
  if(size_hints) ClientBounds(c, bounds, &c->geom, False, False);

  /* Fit into bounds */
  if(!(c->flags & (SUB_CLIENT_MODE_FULL|SUB_CLIENT_TYPE_DOCK)))
    {
      int maxx = 0, maxy = 0;

      /* Check size for clients we are allowed to change */
      if(!(c->flags & SUB_CLIENT_MODE_FIXED))
        {
          if(c->geom.width  > bounds->width)  c->geom.width  = bounds->width;
          if(c->geom.height > bounds->height) c->geom.height = bounds->height;
        }

      /* Check whether window fits into bounds */
      maxx = bounds->x + bounds->width;
      maxy = bounds->y + bounds->height;

      /* Check x and center */
      if(c->geom.x < bounds->x || c->geom.x > maxx ||
          c->geom.x + c->geom.width > maxx)
        {
          if(c->flags & SUB_CLIENT_MODE_FLOAT)
            c->geom.x = bounds->x + ((bounds->width - c->geom.width) / 2);
          else c->geom.x = bounds->x;
        }

      /* Check y and center */
      if(c->geom.y < bounds->y || c->geom.y > maxy ||
          c->geom.y + c->geom.height > maxy)
        {
          if(c->flags & SUB_CLIENT_MODE_FLOAT)
            c->geom.y = bounds->y + ((bounds->height - c->geom.height) / 2);
          else c->geom.y = bounds->y;
        }
    }
} /* }}} */

 /** subClientRestack {{{
  * @brief Restack client
  * @param[in]  c    A #SubClient
  * @param[in]  dir  Either below or above
  **/

void
subClientRestack(SubClient *c,
  int dir)
{
  c->dir = dir;
  subArraySort(subtle->clients, ClientCompare);
  c->dir = -1;

  subClientPublish(True);

  subSubtleLogDebugSubtle("Restack: instance=%s, win=%#lx, dir=%s\n",
    c->instance, c->win, SUB_CLIENT_RESTACK_DOWN == dir ? "down" : "up");
} /* }}} */

  /** subClientArrange {{{
   * @brief Arrange position of client
   * @param[in]  c        A #SubClient
   * @param[in]  gravity  The gravity id
   * @param[in]  screen   The screen id
   **/

void
subClientArrange(SubClient *c,
  int gravityid,
  int screenid)
{
  SubScreen *s = SCREEN(subArrayGet(subtle->screens, screenid));

  DEAD(c);
  assert(c && s);

  /* Check flags */
  if(c->flags & SUB_CLIENT_MODE_FULL)
    {
      /* Use all screens when in zaphod mode */
      if(c->flags & SUB_CLIENT_MODE_ZAPHOD)
        {
          XMoveResizeWindow(subtle->dpy, c->win, 0, 0,
            subtle->width, subtle->height);
        }
      else XMoveResizeWindow(subtle->dpy, c->win, s->base.x, s->base.y,
        s->base.width, s->base.height);

      XRaiseWindow(subtle->dpy, c->win);
    }
  else if(c->flags & SUB_CLIENT_MODE_FLOAT)
    {
      if(c->flags & SUB_CLIENT_ARRANGE || (-1 != screenid && c->screenid != screenid))
        {
          SubScreen *s2 = SCREEN(subArrayGet(subtle->screens,
            -1 != c->screenid ? c->screenid : 0));

          /* Update screen offsets */
          if(s != s2)
            {
              c->geom.x      = c->geom.x - s2->geom.x + s->geom.x;
              c->geom.y      = c->geom.y - s2->geom.y + s->geom.y;
              c->geom.width  = c->geom.width;
              c->geom.height = c->geom.height;
              c->screenid    = screenid;
            }

          /* Finally resize window */
          subClientResize(c, &(s->geom), True);

          XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
            c->geom.width, c->geom.height);
        }
    }
  else if(c->flags & SUB_CLIENT_TYPE_DESKTOP)
    {
      c->geom = s->geom;

      /* Just use screen size for desktop windows */
      XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
        c->geom.width, c->geom.height);
      XLowerWindow(subtle->dpy, c->win);
    }
  else if(c->flags & SUB_CLIENT_TYPE_DOCK)
    {
      /* Just use screen size for desktop windows */
      XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
        c->geom.width, c->geom.height);
      XLowerWindow(subtle->dpy, c->win);
    }
  else
    {
      if(c->flags & SUB_CLIENT_ARRANGE ||
          c->gravityid != gravityid || c->screenid != screenid)
        {
          XRectangle bounds = s->geom;
          int old_gravity = c->gravityid, old_screen = c->screenid;
          SubGravity *g = NULL, *old_g = NULL;

          /* Set values */
          if(-1 != screenid)  c->screenid  = screenid;
          if(-1 != gravityid)
            c->gravityid = c->gravities[s->viewid] = gravityid;

          g     = GRAVITY(subArrayGet(subtle->gravities, gravityid));
          old_g = GRAVITY(subArrayGet(subtle->gravities, old_gravity));

          /* Gravity tiling */
          if(-1 != old_screen && (subtle->flags & SUB_SUBTLE_TILING ||
              (old_g && old_g->flags & (SUB_GRAVITY_HORZ|SUB_GRAVITY_VERT))))
            ClientTile(old_gravity, old_screen);

          if(subtle->flags & SUB_SUBTLE_TILING ||
              (g && g->flags & (SUB_GRAVITY_HORZ|SUB_GRAVITY_VERT)))
            {
              ClientTile(gravityid, -1 == screenid ? 0 : screenid);
            }
          else
            {
              /* Set size for bounds*/
              if(c->flags & SUB_CLIENT_MODE_ZAPHOD) ClientZaphod(c, &bounds);
              subGravityGeometry(g, &bounds, &c->geom);
              ClientResize(c, &bounds);
            }

          /* EWMH: Gravity */
          subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_GRAVITY,
            (long *)&c->gravityid, 1);

          XSync(subtle->dpy, False); ///< Sync before going on

          /* Hook: Gravity */
          subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_GRAVITY),
            (void *)c);
        }
    }
} /* }}} */

 /** subClientToggle {{{
  * @brief Toggle various states of client
  * @param[in]  c            A #SubClient
  * @param[in]  flags        Toggle flag
  * @param[in]  set_gravity  Whether gravity should be set
  **/

void
subClientToggle(SubClient *c,
  int flags,
  int set_gravity)
{
  int nstates = 0;
  Atom states[3] = { None };

  DEAD(c);
  assert(c);

  /* Set arrange flags for certain modes */
  if(flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_STICK|SUB_CLIENT_MODE_FULL|
      SUB_CLIENT_MODE_ZAPHOD|SUB_CLIENT_MODE_BORDERLESS|SUB_CLIENT_MODE_CENTER))
    c->flags |= SUB_CLIENT_ARRANGE; ///< Force rearrange

  /* Handle sticky mode */
  if(flags & SUB_CLIENT_MODE_STICK)
    {
      SubClient *focus = NULL;

      /* Unset stick mode */
      if(c->flags & SUB_CLIENT_MODE_STICK)
        {
          /* Update highlight urgent client */
          if(c->flags & SUB_CLIENT_MODE_URGENT)
            subtle->urgent_tags &= ~c->tags;
        }
      else
        {
          /* Check if gravity should be set */
          if(set_gravity)
            {
              int i;

              /* Set gravity for untagged views */
              for(i = 0; i < subtle->views->ndata; i++)
                {
                  SubView *v = VIEW(subtle->views->data[i]);

                  /* Check visibility manually */
                  if(!(v->tags & c->tags) && -1 != c->gravityid)
                    c->gravities[i] = c->gravityid;
                }
            }

          /* Set screen when required*/
          if(!(c->flags & SUB_CLIENT_MODE_STICK_SCREEN))
            {
              /* Find screen: Prefer screen of current window */
              if(subtle->flags & SUB_SUBTLE_SKIP_WARP &&
                  (focus = CLIENT(subSubtleFind(subtle->windows.focus[0],
                  CLIENTID))) && VISIBLE(focus))
                c->screenid = focus->screenid;
              else subScreenCurrent(&c->screenid);
            }
      }
  }

  /* Handle fullscreen mode */
  if(flags & SUB_CLIENT_MODE_FULL)
    {
      if(c->flags & SUB_CLIENT_MODE_FULL)
        {
          if(!(c->flags & SUB_CLIENT_MODE_BORDERLESS))
            XSetWindowBorderWidth(subtle->dpy, c->win,
              subtle->styles.clients.border.top);
        }
      else
        {
          /* Normally, you'd expect, that a fixed size window wants to keep
           * the size. Apparently, some broken clients just violate that, so we
           * exclude fixed windows with min != screen size from fullscreen */

          if(c->flags & SUB_CLIENT_MODE_FIXED)
            {
              SubScreen *s = SCREEN(subtle->screens->data[c->screenid]);

              if(s->base.width != c->minw || s->base.height != c->minh)
                {
                  flags &= ~SUB_CLIENT_MODE_FULL;

                  return;
                }
            }

          XSetWindowBorderWidth(subtle->dpy, c->win, 0);
        }
    }

  /* Handle borderless mode */
  if(flags & SUB_CLIENT_MODE_BORDERLESS)
    {
      /* Unset borderless or fullscreen mode */
      if(!(c->flags & SUB_CLIENT_MODE_BORDERLESS))
        XSetWindowBorderWidth(subtle->dpy, c->win,
          subtle->styles.clients.border.top);
      else XSetWindowBorderWidth(subtle->dpy, c->win, 0);
    }

  /* Handle urgent mode */
  if(flags & SUB_CLIENT_MODE_URGENT)
    subtle->urgent_tags |= c->tags;

  /* Handle center mode */
  if(flags & SUB_CLIENT_MODE_CENTER)
    {
      if(c->flags & SUB_CLIENT_MODE_CENTER)
        {
          c->flags &= ~SUB_CLIENT_MODE_FLOAT;
          c->flags |= SUB_CLIENT_ARRANGE;
        }
      else
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screenid]);

          /* Set to screen center */
          c->geom.x = s->geom.x +
            (s->geom.width - c->geom.width - 2 * BORDER(c)) / 2;
          c->geom.y = s->geom.y +
            (s->geom.height - c->geom.height - 2 * BORDER(c)) / 2;

          flags    |= SUB_CLIENT_MODE_FLOAT;
          c->flags |= SUB_CLIENT_ARRANGE;
        }
    }

  /* Handle desktop and dock type (one way) */
  if(flags & (SUB_CLIENT_TYPE_DESKTOP|SUB_CLIENT_TYPE_DOCK))
    {
      XSetWindowBorderWidth(subtle->dpy, c->win, 0);

      /* Special treatment */
      if(flags & SUB_CLIENT_TYPE_DESKTOP)
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screenid]);

          c->geom = s->base;

          /* Add panel heights without struts */
          if(s->flags & SUB_SCREEN_PANEL1)
            {
              c->geom.y      += subtle->ph;
              c->geom.height -= subtle->ph;
            }

          if(s->flags & SUB_SCREEN_PANEL2)
            c->geom.height -= subtle->ph;
        }
    }

  /* Finally toggle mode flags only */
  c->flags = ((c->flags & ~MODES_ALL) |
    ((c->flags & MODES_ALL) ^ (flags & MODES_ALL)));

  /* Sort for keeping stacking order */
  if(c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL|
      SUB_CLIENT_TYPE_DESKTOP|SUB_CLIENT_TYPE_DOCK))
    subClientRestack(c, SUB_CLIENT_RESTACK_UP);

  /* EWMH: State and flags */
  if(c->flags & SUB_CLIENT_MODE_FULL)
    states[nstates++] = subEwmhGet(SUB_EWMH_NET_WM_STATE_FULLSCREEN);
  if(c->flags & SUB_CLIENT_MODE_FLOAT)
    states[nstates++] = subEwmhGet(SUB_EWMH_NET_WM_STATE_ABOVE);
  if(c->flags & SUB_CLIENT_MODE_STICK)
    states[nstates++] = subEwmhGet(SUB_EWMH_NET_WM_STATE_STICKY);
  if(c->flags & SUB_CLIENT_MODE_URGENT)
    states[nstates++] = subEwmhGet(SUB_EWMH_NET_WM_STATE_ATTENTION);

  subEwmhTranslateClientMode(c->flags, &flags);

  XChangeProperty(subtle->dpy, c->win, subEwmhGet(SUB_EWMH_NET_WM_STATE),
    XA_ATOM, 32, PropModeReplace, (unsigned char *)&states, nstates);

  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_FLAGS, (long *)&flags, 1);

  XSync(subtle->dpy, False); ///< Sync all changes

  /* Hook: Mode */
  subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_MODE), (void *)c);

  subSubtleLogDebugSubtle("Toggle: flags=%d, set_gravity=%d\n", flags, set_gravity);
} /* }}} */

  /** subClientSetStrut {{{
   * @brief Set client strut
   * @param[in]  c  A #SubClient
   **/

void
subClientSetStrut(SubClient *c)
{
  unsigned long size = 0;
  long *strut = NULL;

  DEAD(c);
  assert(c);

  /* Get strut property */
  if((strut = (long *)subSharedPropertyGet(subtle->dpy, c->win, XA_CARDINAL,
      subEwmhGet(SUB_EWMH_NET_WM_STRUT), &size)))
    {
      if(4 == size) ///< Only complete struts
        {
          subtle->styles.subtle.padding.left   =
            MAX(subtle->styles.subtle.padding.left, strut[0]);
          subtle->styles.subtle.padding.right  =
            MAX(subtle->styles.subtle.padding.right, strut[1]);
          subtle->styles.subtle.padding.top    = 
            MAX(subtle->styles.subtle.padding.top, strut[2]);
          subtle->styles.subtle.padding.bottom =
            MAX(subtle->styles.subtle.padding.bottom, strut[3]);

          /* Update screen and clients */
          subScreenResize();
          subScreenConfigure();
        }

      XFree(strut);
    }
} /* }}} */

 /** subClientSetProtocols {{{
  * @brief Set client protocols
  * @param[in]  c  A #SubClient
  **/

void
subClientSetProtocols(SubClient *c)
{
  int i, n = 0;
  Atom *protos = NULL;

  assert(c);

  /* Window manager protocols */
  if(XGetWMProtocols(subtle->dpy, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          switch(subEwmhFind(protos[i]))
            {
              case SUB_EWMH_WM_TAKE_FOCUS:    c->flags |= SUB_CLIENT_FOCUS; break;
              case SUB_EWMH_WM_DELETE_WINDOW: c->flags |= SUB_CLIENT_CLOSE; break;
              default: break;
            }
         }

      XFree(protos);
    }
} /* }}} */

  /** subClientSetSizeHints {{{
   * @brief Set client size hints
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetSizeHints(SubClient *c,
  int *flags)
{
  long supplied = 0;
  XSizeHints *hints = NULL;
  SubScreen *s = NULL;

  DEAD(c);
  assert(c);

  if(!(hints = XAllocSizeHints()))
    {
      subSubtleLogError("Cannot alloc memory. Exhausted?\n");

      abort();
    }

  s = SCREEN(subtle->screens->data[0]); ///< Assume first screen

  /* Default values {{{ */
  c->minw  = MINW;
  c->minh  = MINH;
  c->maxw  = -1;
  c->maxh  = -1;
  c->minr  = 0.0f;
  c->maxr  = 0.0f;
  c->incw  = 1;
  c->inch  = 1;
  c->basew = 0;
  c->baseh = 0; /* }}} */

  /* Size hints - no idea why it's called normal hints */
  if(XGetWMNormalHints(subtle->dpy, c->win, hints, &supplied))
    {
      /* Program min size */
      if(hints->flags & PMinSize)
        {
          /* Limit min size to screen size if larger */
          if(hints->min_width)
            c->minw = c->minw > s->geom.width ? s->geom.width :
              MAX(MINW, hints->min_width);
          if(hints->min_height)
            c->minh = c->minh > s->geom.height ? s->geom.height :
              MAX(MINH, hints->min_height);
        }

      /* Program max size */
      if(hints->flags & PMaxSize)
        {
          /* Limit max size to screen size if larger */
          if(hints->max_width)
            c->maxw = hints->max_width > s->geom.width ?
              s->geom.width : hints->max_width;

          if(hints->max_height)
            c->maxh = hints->max_height > s->geom.height - subtle->ph ?
              s->geom.height - subtle->ph : hints->max_height;
        }

      /* Set float when min == max size (EWMH: Fixed size windows) */
      if(hints->flags & PMinSize && hints->flags & PMaxSize)
        {
          if(hints->min_width == hints->max_width &&
              hints->min_height == hints->max_height &&
              !(c->flags & SUB_CLIENT_TYPE_DESKTOP))
            *flags |= (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FIXED);
        }

      /* Aspect ratios */
      if(hints->flags & PAspect)
        {
          if(hints->min_aspect.y)
            c->minr = (float)hints->min_aspect.x / hints->min_aspect.y;
          if(hints->max_aspect.y)
            c->maxr = (float)hints->max_aspect.x / hints->max_aspect.y;
        }

      /* Resize increment steps */
      if(hints->flags & PResizeInc)
        {
          if(hints->width_inc)  c->incw = hints->width_inc;
          if(hints->height_inc) c->inch = hints->height_inc;
        }

      /* Base sizes */
      if(hints->flags & PBaseSize)
        {
          if(hints->base_width)  c->basew = hints->base_width;
          if(hints->base_height) c->baseh = hints->base_height;
        }

      /* Check for specific position */
      if((subtle->flags & SUB_SUBTLE_RESIZE || c->flags &
          (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_RESIZE|SUB_CLIENT_TYPE_DOCK)))
        {
          /* User/program size */
          if(hints->flags & (USSize|PSize))
            {
              c->geom.width  = hints->width;
              c->geom.height = hints->height;
            }

          /* User/program position */
          if(hints->flags & (USPosition|PPosition))
            {
              c->geom.x = hints->x;
              c->geom.y = hints->y;
            }

          /* Sanitize positions for stupid clients like GIMP */
          if(hints->flags & (USSize|PSize|USPosition|PPosition))
            subClientResize(c, &(s->geom), True);
        }
    }

  XFree(hints);

  subSubtleLogDebug("SetSizeHints: x=%d, y=%d, width=%d, height=%d, "
    "minw=%d, minh=%d, maxw=%d, maxh=%d, minr=%.1f, maxr=%.1f, "
    "incw=%d, inch=%d, basew=%d, baseh=%d\n",
    c->geom.x, c->geom.y, c->geom.width, c->geom.height, c->minw, c->minh,
    c->maxw, c->maxh, c->minr, c->maxr, c->incw, c->inch, c->basew, c->baseh);
} /* }}} */

  /** subClientSetWMHints {{{
   * @brief Set client WM hints
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetWMHints(SubClient *c,
  int *flags)
{
  XWMHints *hints = NULL;

  assert(c && flags);

  /* Window manager hints (ICCCM 4.1.7) */
  if((hints = XGetWMHints(subtle->dpy, c->win)))
    {
      /* Handle urgency hint:
       * Set urgency if window hasn't focus and and
       * remove it after getting focus */
      if(hints->flags & XUrgencyHint && c->win != subtle->windows.focus[0])
        {
          *flags |= SUB_CLIENT_MODE_URGENT;
        }

      /* Handle window group hint */
      if(hints->flags & WindowGroupHint)
        {
          SubClient *k = NULL;

          /* Copy tags and modes */
          if((k = CLIENT(subSubtleFind(hints->window_group, CLIENTID))))
            {
              *flags      |= (k->flags & MODES_ALL);
              c->tags     |= k->tags;
              c->screenid |= k->screenid;
            }
        }

      /* Handle just false value of input hint since it is default */
      if(hints->flags & InputHint && !hints->input)
        c->flags &= ~SUB_CLIENT_INPUT;

      XFree(hints);
    }

  subSubtleLogDebugSubtle("SetWMHints\n");
} /* }}} */

  /** subClientSetMWMHints {{{
   * @brief Set client MWM hints
   * @param[in]  c  A #SubClient
   **/

void
subClientSetMWMHints(SubClient *c)
{
  unsigned long size = 0;
  ClientMWMHints *hints = NULL;

  assert(c);

  /* Window manager hints */
  if((hints = (ClientMWMHints *)subSharedPropertyGet(subtle->dpy, c->win,
      subEwmhGet(SUB_EWMH_MOTIF_WM_HINTS),
      subEwmhGet(SUB_EWMH_MOTIF_WM_HINTS), &size)))
    {
      /* Check if hints contain decoration flags */
      if(hints->flags & MWM_FLAG_DECORATIONS)
        {
          /* Check window border */
          if(!(hints->decorations & (MWM_DECOR_ALL|MWM_DECOR_BORDER)))
            c->flags |= SUB_CLIENT_MODE_BORDERLESS;
        }

      free(hints);
    }

  subSubtleLogDebugSubtle("SetMWMHints\n");
} /* }}} */

  /** subClientSetState {{{
   * @brief Set client WM state
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetState(SubClient *c,
  int *flags)
{
  int i;
  unsigned long nstates = 0;
  Atom *states = NULL;

  assert(c);

  /* Window state */
  if((states = (Atom *)subSharedPropertyGet(subtle->dpy, c->win, XA_ATOM,
      subEwmhGet(SUB_EWMH_NET_WM_STATE), &nstates)))
    {
      for(i = 0; i < nstates; i++)
        subEwmhTranslateWMState(states[i], flags);

      XFree(states);
    }

  subSubtleLogDebugSubtle("SetState\n");
} /* }}} */

 /** subClientSetTransient {{{
  * @brief Set client transient
  * @param[in]  c      A #SubClient
  * @param[in]  flags  Mode flags
  **/

void
subClientSetTransient(SubClient *c,
  int *flags)
{
  Window trans = None;

  assert(c && flags);

  /* Check for transient windows */
  if(XGetTransientForHint(subtle->dpy, c->win, &trans))
    {
      SubClient *k = NULL;

      /* Check if transient windows should be urgent */
      *flags |= subtle->flags & SUB_SUBTLE_URGENT ?
        SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_URGENT : SUB_CLIENT_MODE_FLOAT;

      /* Find parent window */
      if((k = CLIENT(subSubtleFind(trans, CLIENTID))))
        {
          *flags      |= (k->flags & MODES_ALL);
          c->tags     |= k->tags;
          c->screenid |= k->screenid;
        }
     }

  subSubtleLogDebugSubtle("SetTransient\n");
} /* }}} */

 /** subClientSetType {{{
  * @brief Set client type
  * @param[in]  c      A #SubClient
  * @param[in]  flags  Mode flags
  **/

void
subClientSetType(SubClient *c,
  int *flags)
{
  int i;
  unsigned long size = 0;
  Atom *types = NULL;

  assert(c);

  /* Get window type */
  if((types = (Atom *)subSharedPropertyGet(subtle->dpy, c->win, XA_ATOM,
      subEwmhGet(SUB_EWMH_NET_WM_WINDOW_TYPE), &size)))
    {
      int id = 0;

      /* Set flags according to window types */
      for(i = 0; i < size; i++)
        {
          switch((id = subEwmhFind(types[i])))
            {
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DESKTOP:
                c->flags |= SUB_CLIENT_TYPE_DESKTOP;
                *flags   |= (SUB_CLIENT_MODE_FIXED|SUB_CLIENT_MODE_STICK);
                break;
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DOCK:
                c->flags |= SUB_CLIENT_TYPE_DOCK;
                *flags   |= (SUB_CLIENT_MODE_FIXED|SUB_CLIENT_MODE_STICK);
                break;
              case SUB_EWMH_NET_WM_WINDOW_TYPE_TOOLBAR:
                c->flags |= SUB_CLIENT_TYPE_TOOLBAR;
                break;
              case SUB_EWMH_NET_WM_WINDOW_TYPE_SPLASH:
                c->flags |= SUB_CLIENT_TYPE_SPLASH;
                *flags    = (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_CENTER);
                break;
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DIALOG:
                c->flags |= SUB_CLIENT_TYPE_DIALOG;
                *flags   |= (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_CENTER);
                break;
              default: break;
            }
        }

      XFree(types);
    }

  /* Set normal type */
  if(!(c->flags & TYPES_ALL)) c->flags |= SUB_CLIENT_TYPE_NORMAL;

  subSubtleLogDebugSubtle("SetType\n");
} /* }}} */

 /** subClientClose {{{
  * @brief Send client delete message or just kill it
  * @param[in]  c  A #SubClient
  **/

void
subClientClose(SubClient *c)
{
  assert(c);

  /* Honor window preferences (see ICCCM 4.1.2.7, 4.2.8.1) */
  if(c->flags & SUB_CLIENT_CLOSE)
    {
      subEwmhMessage(c->win, SUB_EWMH_WM_PROTOCOLS, NoEventMask,
        subEwmhGet(SUB_EWMH_WM_DELETE_WINDOW), CurrentTime, 0, 0, 0);
    }
  else
    {
      int sid = (subtle->windows.focus[0] == c->win ? c->screenid : -1); ///< Save

      /* Kill it manually */
      XKillClient(subtle->dpy, c->win);

      subArrayRemove(subtle->clients, (void *)c);
      subClientKill(c);
      subClientPublish(False);

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(-1 != sid)
        {
          SubClient *k = subClientNext(sid, False);

          if(k) subClientFocus(k, True);
        }
    }

  subSubtleLogDebugSubtle("Close\n");
} /* }}} */

 /** subClientKill {{{
  * @brief Kill a client
  * @param[in]  c  A #SubClient
  **/

void
subClientKill(SubClient *c)
{
  assert(c);

  /* Hook: Kill */
  subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_KILL),
    (void *)c);

  /* Remove _NET_WM_STATE (see EWMH 1.3) */
  subSharedPropertyDelete(subtle->dpy, c->win,
    subEwmhGet(SUB_EWMH_NET_WM_STATE));

  /* Ignore further events and delete context */
  XSelectInput(subtle->dpy, c->win, NoEventMask);
  XDeleteContext(subtle->dpy, c->win, CLIENTID);

  /* Remove client tags from urgent tags */
  if(c->flags & SUB_CLIENT_MODE_URGENT)
    subtle->urgent_tags &= ~c->tags;

  /* Tile remaining clients if necessary */
  if(VISIBLE(c))
    {
      SubGravity *g = GRAVITY(subArrayGet(subtle->gravities, c->gravityid));

      if(g && ((subtle->flags & SUB_SUBTLE_TILING) ||
          g->flags & (SUB_GRAVITY_HORZ|SUB_GRAVITY_VERT)))
        ClientTile(c->gravityid, c->screenid);
    }

  if(c->gravities) free(c->gravities);
  if(c->name)      free(c->name);
  if(c->instance)  free(c->instance);
  if(c->klass)     free(c->klass);
  if(c->role)      free(c->role);
  free(c);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subClientPublish {{{
  * @brief Publish clients
  * @param[in]  restack  Restack windows
  **/

void
subClientPublish(int restack)
{
  int i;
  Window *wins = (Window *)subSharedMemoryAlloc(subtle->clients->ndata,
    sizeof(Window));

  /* Sort clients from top (=> 0) to bottom */
  for(i = 0; i < subtle->clients->ndata; i++)
    wins[subtle->clients->ndata - 1 - i] = CLIENT(subtle->clients->data[i])->win;

  /* EWMH: Client list and client list stacking (same for us) */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, wins,
    subtle->clients->ndata);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, wins,
    subtle->clients->ndata);

  /* Restack windows? We assembled the array anyway. */
  if(restack) XRestackWindows(subtle->dpy, wins, subtle->clients->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(wins);

  subSubtleLogDebugSubtle("Publish: clients=%d, restack=%d\n",
    subtle->clients->ndata, restack);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
