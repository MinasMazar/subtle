
 /**
  * @package subtle
  *
  * @file Event functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/event.c,v 3202 2012/04/14 14:30:51 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <unistd.h>
#include <X11/Xatom.h>
#include <sys/poll.h>
#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif /* HAVE_SYS_INOTIFY_H */

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
#include <X11/extensions/Xrandr.h>
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

/* Globals */
struct pollfd *watches = NULL;
XClientMessageEvent *queue = NULL;
int nwatches = 0, nqueue = 0;

/* EventUntag {{{ */
static void
EventUntag(SubClient *c,
  int id)
{
  int i, tag;

  /* Shift bits */
  for(i = id; i < subtle->tags->ndata - 1; i++)
    {
      tag = (1L << (i + 1));

      if(c->tags & (1L << (i + 2))) ///< Next bit
        c->tags |= tag;
      else
        c->tags &= ~tag;
    }

  /* EWMH: Tags */
  if(c->flags & SUB_TYPE_CLIENT)
    {
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS,
        (long *)&c->tags, 1);
    }
} /* }}} */

/* EventFindSublet {{{ */
static SubPanel *
EventFindSublet(int id)
{
  int i = 0, j = 0, idx = 0;

  /* Find sublet in panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      for(j = 0; j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          if(p->flags & SUB_PANEL_SUBLET &&
              !(p->flags & SUB_PANEL_COPY) && idx++ == id)
            return p;
        }
    }

  return NULL;
} /* }}} */

/* EventQueuePush {{{ */
static void
EventQueuePush(XClientMessageEvent *ev,
  long type)
{
  /* Since we are dealing with race conditions we need to cache
   * client messages when a client/view/tag isn't ready yet */
  queue = (XClientMessageEvent *)subSharedMemoryRealloc(queue,
    (nqueue + 1) * sizeof(XClientMessageEvent));
  queue[nqueue] = *ev;
  queue[nqueue].data.l[2] = type;
  nqueue++;

  subSubtleLogDebugEvents("Queue push: id=%ld, tags=%ld, type=%ld\n",
    ev->data.l[0], ev->data.l[1], type);
} /* }}} */

/* EventQueuePop {{{ */
static void
EventQueuePop(long value,
  long type)
{
  /* Check queue */
  if(queue)
    {
      int i;

      /* Check queued events */
      for(i = 0; i < nqueue; i++)
        {
          /* Check event type matches */
          if(queue[i].data.l[2] == type)
            {
              int j;

              subSubtleLogDebugEvents("Queue pop: id=%ld, tags=%ld, type=%ld\n",
                queue[i].data.l[0], queue[i].data.l[1], queue[i].data.l[2]);

              /* Update window id or array index and put back */ 
              queue[i].data.l[0] = value;
              XPutBackEvent(subtle->dpy, (XEvent *)&queue[i]);

              /* Remove element from queue */
              for(j = i; j < nqueue - 1; j++)
                queue[j] = queue[j + 1];

              nqueue--;
              queue = (XClientMessageEvent *)subSharedMemoryRealloc(queue,
                nqueue * sizeof(XClientMessageEvent));
              i--;
            }
        }
    }
} /* }}} */

/* EventMatch {{{ */
static int
EventMatch(int type,
  XRectangle *origin,
  XRectangle *test)
{
  int cx_origin = 0, cx_test = 0, cy_origin = 0, cy_test = 0, dx = 0, dy = 0;

  /* This check is complicated and consists of three parts:
   * 1) Calculate window center positions
   * 2) Check if x/y values decrease in given direction
   * 3) Check if a corner of one of the rects is close enough to
   *    a side of the other rect */

  /* Calculate window centers */
  cx_origin = origin->x + (origin->width / 2);
  cx_test   = test->x + (test->width / 2);

  cy_origin = origin->y + (origin->height / 2);
  cy_test   = test->y + (test->height / 2);

  /* Check geometries */
  if((((SUB_GRAB_DIRECTION_LEFT  == type      && cx_test   <= cx_origin)                  ||
       (SUB_GRAB_DIRECTION_RIGHT == type      && cx_test   >= cx_origin))                 &&
       ((cy_test                 >= origin->y && cy_test   <= origin->y + origin->height) ||
       (cy_origin                >= test->y   && cy_origin <= test->y   + test->height))) ||

     (((SUB_GRAB_DIRECTION_UP    == type      && cy_test   <= cy_origin)                  ||
       (SUB_GRAB_DIRECTION_DOWN  == type      && cy_test   >= cy_origin))                 &&
       ((cx_test                 >= origin->x && cx_test   <= origin->x + origin->width)  ||
       (cx_origin                 >= test->x   && cx_origin <= test->x   + test->width))))
    {
      /* Euclidean distance */
      dx = abs(cx_origin - cx_test);
      dy = abs(cy_origin - cy_test);

      /* Zero distance means same dimensions - highest distance */
      if(0 == dx && 0 == dy) dx = dy = 1L << 15;
    }
  else
    {
      /* No match - highest distance too */
      dx = 1L << 15;
      dy = 1L << 15;
    }

  return dx + dy;
} /* }}} */

/* Events */

/* EventColormap {{{ */
static void
EventColormap(XColormapEvent *ev)
{
  SubClient *c = (SubClient *)subSubtleFind(ev->window, 1);
  if(c && ev->new)
    {
      c->cmap = ev->colormap;
      XInstallColormap(subtle->dpy, c->cmap);
    }

  subSubtleLogDebugEvents("Colormap: win=%#lx\n", ev->window);
} /* }}} */

/* EventConfigure {{{ */
static void
EventConfigure(XConfigureEvent *ev)
{
  /* Ckeck window */
  if(ROOT == ev->window)
    {
      int sw = 0, sh = 0;

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
      /* Update RandR config */
      if(subtle->flags & SUB_SUBTLE_XRANDR)
        XRRUpdateConfiguration((XEvent *)ev);
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

      /* Fetch screen geometry after update */
      sw = DisplayWidth(subtle->dpy, DefaultScreen(subtle->dpy));
      sh = DisplayHeight(subtle->dpy, DefaultScreen(subtle->dpy));

      /* Skip event if screen size didn't change */
      if(subtle->width == sw && subtle->height == sh) return;

      /* Reload screens */
      subArrayClear(subtle->sublets, False);
      subArrayClear(subtle->screens, True);
      subScreenInit();

      subRubyReloadConfig();
      subScreenResize();

      /* Update size */
      subtle->width  = sw;
      subtle->height = sh;

      printf("Updated screens\n");
    }

  subSubtleLogDebugEvents("Configure: win=%#lx\n", ev->window);
} /* }}} */

/* EventConfigureRequest {{{ */
static void
EventConfigureRequest(XConfigureRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Complicated request! (see ICCCM 4.1.5)
   * No change    -> Synthetic ConfigureNotify
   * Move/restack -> Synthetic + real ConfigureNotify
   * Resize       -> Real ConfigureNotify */

  /* Check window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      /* Check flags if the request is important */
      if(!(c->flags & SUB_CLIENT_MODE_FULL) &&
          (subtle->flags & SUB_SUBTLE_RESIZE ||
          c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_RESIZE)))
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screenid]);

          /* Handle request values and check if they make sense
           * Some clients outsmart us and center their toplevel windows */
          if(ev->value_mask & CWX)
            c->geom.x = ev->x < s->geom.x ? s->geom.x + ev->x : ev->x;
          if(ev->value_mask & CWY)
            c->geom.y = ev->y < s->geom.y ? s->geom.y + ev->y : ev->y;
          if(ev->value_mask & CWWidth)  c->geom.width  = ev->width;
          if(ev->value_mask & CWHeight) c->geom.height = ev->height;

          subClientResize(c, &(s->geom), False);

          /* Send synthetic configure notify */
          if(!(ev->value_mask & (CWX|CWY|CWWidth|CWHeight)) ||
              ((ev->value_mask & (CWX|CWY)) &&
              !(ev->value_mask & (CWWidth|CWHeight))))
            subClientConfigure(c);

          /* Send real configure notify */
          if(ev->value_mask & (CWX|CWY|CWWidth|CWHeight))
            XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
              c->geom.width, c->geom.height);
        }
      else subClientConfigure(c);
    }
  else ///< Unmanaged windows
    {
      XWindowChanges wc;

      wc.x            = ev->x;
      wc.y            = ev->y;
      wc.width        = ev->width;
      wc.height       = ev->height;
      wc.border_width = 0;
      wc.sibling      = ev->above;
      wc.stack_mode   = ev->detail;

      XConfigureWindow(subtle->dpy, ev->window, ev->value_mask, &wc);
    }

  subSubtleLogDebugEvents("ConfigureRequest: win=%#lx\n", ev->window);
} /* }}} */

/* EventCrossing {{{ */
static void
EventCrossing(XCrossingEvent *ev)
{
  SubClient *c = NULL;
  SubScreen *s = NULL;

  /* Handle both crossing events */
  switch(ev->type)
    {
      case EnterNotify:
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))) && ALIVE(c)) ///< Client
          {
            if(!(subtle->flags & SUB_SUBTLE_FOCUS_CLICK))
              subClientFocus(c, False);
          }
        else if((s = SCREEN(subSubtleFind(ev->window, SCREENID)))) ///< Screen panels
          {
            subPanelAction(s->panels, SUB_PANEL_OVER, ev->x, ev->y, -1,
              s->panel2 == ev->window);
          }
        break;
      case LeaveNotify:
        if((s = SCREEN(subSubtleFind(ev->window, SCREENID)))) ///< Screen panels
          {
            subPanelAction(s->panels, SUB_PANEL_OUT, ev->x, ev->y, -1,
              s->panel2 == ev->window);
          }
    }

  subSubtleLogDebugEvents("Enter: win=%#lx\n", ev->window);
} /* }}} */

/* EventDestroy {{{ */
static void
EventDestroy(XDestroyWindowEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
    {
      int sid = (subtle->windows.focus[0] == c->win ? c->screenid : -1); ///< Save

      /* Kill client */
      subArrayRemove(subtle->clients, (void *)c);
      subClientKill(c);
      subClientPublish(False);

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if((c = subClientNext(sid, False))) subClientFocus(c, True);
    }
  else if((t = TRAY(subSubtleFind(ev->event, TRAYID)))) ///< Tray
    {
      int focus = (subtle->windows.focus[0] == ev->window); ///< Save

      /* Kill tray */
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subTrayPublish();

      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(focus && (c = subClientNext(0, False))) subClientFocus(c, True);
    }
  else
    {
      int i;

      /* Check if window is client leader */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Mark all windows with leader as dead */
          if(c->leader == ev->window) c->flags |= SUB_CLIENT_DEAD;
        }
    }

  subSubtleLogDebugEvents("Destroy: win=%#lx\n", ev->window);
} /* }}} */

/* EventExpose {{{ */
static void
EventExpose(XExposeEvent *ev)
{
  if(0 == ev->count) subScreenRender(); ///< Render once

  subSubtleLogDebugEvents("Expose: win=%#lx\n", ev->window);
} /* }}} */

/* EventFocus {{{ */
static void
EventFocus(XFocusChangeEvent *ev)
{
  /* Check window has focus or focus is caused by grabs */
  if(ev->window == subtle->windows.focus[0] ||
    NotifyGrab == ev->mode || NotifyUngrab == ev->mode) return;

  subSubtleLogDebugEvents("Focus: win=%#lx, mode=%d\n", ev->window, ev->mode);
} /* }}} */

/* EventGrab {{{ */
static void
EventGrab(XEvent *ev)
{
  int x = 0, y = 0;
  unsigned int chain = False;
  SubGrab *g = NULL;
  SubClient *c = NULL;
  SubScreen *s = NULL;
  KeySym sym = None;

  /* Distinct types {{{ */
  switch(ev->type)
    {
      case ButtonPress:
        if((s = SCREEN(subSubtleFind(ev->xbutton.window, SCREENID)))) ///< Screen panels
          {
            subPanelAction(s->panels, SUB_PANEL_DOWN, ev->xbutton.x,
              ev->xbutton.y, ev->xbutton.button,
              s->panel2 == ev->xbutton.window);

            return;
          }
        else if(subtle->flags & SUB_SUBTLE_FOCUS_CLICK &&
            (c = CLIENT(subSubtleFind(ev->xbutton.window, CLIENTID)))) ///< Client
          {
            if(ALIVE(c)) subClientFocus(c, False);

            return;
          }

        g = subGrabFind((XK_Pointer_Button1 + ev->xbutton.button),
          ev->xbutton.state); ///< Build button number
        x = ev->xbutton.x;
        y = ev->xbutton.y;

        subSubtleLogDebugEvents("Grab: type=mouse, win=%#lx\n",
          ev->xbutton.window);
        break;
      case KeyPress:
        g = subGrabFind(ev->xkey.keycode, ev->xkey.state);
        x = ev->xkey.x;
        y = ev->xkey.y;

        subSubtleLogDebugEvents("Grab: type=key, keycode=%d, state=%d\n",
          ev->xkey.keycode, ev->xkey.state);
        break;
    } /* }}} */

  /* Find grab */

  /* Check chain end {{{ */
  if(subtle->keychain)
    {
      /* Check if key is just a modifier */
      if(!g)
        {
          sym = XLookupKeysym(&(ev->xkey), 0);

          if(IsModifierKey(sym)) return;
        }

      /* Check if grab belongs to current chain */
      if(g && subtle->keychain->keys)
        chain = (-1 != subArrayIndex(subtle->keychain->keys, (void *)g));

      /* Break chain on end or invalid link */
      if(!chain || (g && !g->keys))
        {
          /* Update panel if in use */
          if(subtle->panels.keychain.keychain)
            {
              free(subtle->panels.keychain.keychain->keys);
              subtle->panels.keychain.keychain->keys = NULL;
              subtle->panels.keychain.keychain->len  = 0;
            }

          subtle->keychain = NULL;

          subScreenUpdate();
          subScreenRender();

          /* Restore binds */
          subGrabUnset(ROOT);
          printf("DEBUG %s:%d\n", __FILE__, __LINE__);
          subGrabSet(ROOT, SUB_GRAB_KEY);

          if(!chain) return;
        }
    } /* }}} */

  /* Handle grab */
  if(g)
    {
      FLAGS flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE|
        SUB_GRAB_CHAIN_START|SUB_GRAB_CHAIN_LINK|SUB_GRAB_CHAIN_END); ///< Clear mask

      /* Handle key chains {{{ */
      if(g->flags & SUB_GRAB_CHAIN_START && g->keys)
        {
          /* Update keychain panel if in use {{{ */
          if(subtle->panels.keychain.keychain)
            {
              char *key = NULL, buf[12] = { 0 };
              int len = 0, pos = 0;

              /* Convert event to key */
              sym = XLookupKeysym(&(ev->xkey), 0);
              key = XKeysymToString(sym);

              /* Append space before each key */
              if(0 < subtle->panels.keychain.keychain->len) buf[pos++] = ' ';

              /* Translate states to string {{{ */
              if(g->state & ShiftMask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "S-");
              if(g->state & ControlMask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "C-");
              if(g->state & Mod1Mask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "A-");
              if(g->state & Mod3Mask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "M-");
              if(g->state & Mod4Mask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "W-");
              if(g->state & Mod5Mask)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", "G-"); /* }}} */

              /* Assemble chain string */
              len += strlen(buf) + strlen(key);

              subtle->panels.keychain.keychain->keys = subSharedMemoryRealloc(
                subtle->panels.keychain.keychain->keys, (len +
                subtle->panels.keychain.keychain->len + 1) * sizeof(char));

              snprintf(subtle->panels.keychain.keychain->keys +
                subtle->panels.keychain.keychain->len,
                subtle->panels.keychain.keychain->len + len,
                "%s%s", buf, key);

              subtle->panels.keychain.keychain->len += len;

              subScreenUpdate();
              subScreenRender();
            }  /* }}} */

          /* Keep chain position */
          subtle->keychain = g;

          /* Bind any keys to exit chain on invalid link */
          subGrabUnset(ROOT);

          XGrabKey(subtle->dpy, AnyKey, AnyModifier, ROOT, True,
            GrabModeAsync, GrabModeAsync);

          return;
        } /* }}} */

      /* Select action */
      switch(flag)
        {
          case SUB_GRAB_SPAWN: /* {{{ */
            if(g->data.string) subSharedSpawn(g->data.string);
            break; /* }}} */
          case SUB_GRAB_PROC: /* {{{ */
            subRubyCall(SUB_CALL_HOOKS, g->data.num,
              subSubtleFind(subtle->windows.focus[0], CLIENTID));
            break; /* }}} */
          case SUB_GRAB_WINDOW_MOVE:
          case SUB_GRAB_WINDOW_RESIZE: /* {{{ */
            /* Prevent resize of fixed and move/resize of fullscreen windows */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))) &&
                !(c->flags & SUB_CLIENT_MODE_FULL) &&
                !(SUB_GRAB_WINDOW_RESIZE == flag &&
                  c->flags & SUB_CLIENT_MODE_FIXED))
              {
                /* Set floating when necessary */
                if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
                  {
                    subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);
                    subScreenUpdate();
                    subScreenRender();
                  }

                /* Translate flags */
                if(SUB_GRAB_WINDOW_MOVE == flag)        flag = SUB_DRAG_MOVE;
                else if(SUB_GRAB_WINDOW_RESIZE == flag) flag = SUB_DRAG_RESIZE;

                subClientDrag(c, flag, (int)g->data.num);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_TOGGLE: /* {{{ */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
              {
                subClientToggle(c, g->data.num, True);

                /* Update screen and focus */
                if(VISIBLE(c) ||
                    SUB_CLIENT_MODE_STICK == g->data.num)
                  {
                    subScreenConfigure();

                    /* Find next and focus */
                    if(!VISIBLE(c) && (c = subClientNext(c->screenid, False)))
                      subClientFocus(c, True);

                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_STACK: /* {{{ */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP) &&
                VISIBLE(c))
              subClientRestack(c, g->data.num);
            break; /* }}} */
          case SUB_GRAB_WINDOW_SELECT: /* {{{ */
            {
              SubClient *found = NULL;

              /* Check if a window is currently focussed or just select next*/
              if((c = CLIENT(subSubtleFind(subtle->windows.focus[0],
                  CLIENTID))))
                {
                  int i, j, match = (1L << 30), distance = 0;

                  /* Iterate once to find a client with smallest distance */
                  for(i = 0; i < subtle->clients->ndata; i++)
                    {
                      SubClient *k = CLIENT(subtle->clients->data[i]);

                      /* Check if both clients are different and visible */
                      if(c != k && (subtle->visible_tags & k->tags ||
                          k->flags & SUB_CLIENT_MODE_STICK))
                        {
                          /* Substract stack position index to get top window */
                          distance = EventMatch(g->data.num, &c->geom,
                            &k->geom) - i;

                          /* Substract history stack position index */
                          for(j = 1; j < HISTORYSIZE; j++)
                            {
                              if(subtle->windows.focus[j] == k->win)
                                {
                                  distance -= (HISTORYSIZE - j);
                                  break;
                                }
                            }

                          /* Finally compare distance */
                          if(match > distance)
                            {
                              match = distance;
                              found = k;
                            }
                        }
                    }
                  }
                else found = subClientNext(-1, True);

                if(found) subClientFocus(found, True);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))) &&
                !(c->flags & SUB_CLIENT_MODE_FIXED))
              {
                int i, id = -1, cid = 0, fid = (int)g->data.string[0] -
                  GRAVITYSTRLIMIT, size = strlen(g->data.string);

                /* Remove float/fullscreen mode */
                if(c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL))
                  {
                    subClientToggle(c, c->flags &
                      (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL), True);
                    subScreenUpdate();
                    subScreenRender();

                    c->gravityid = -1; ///< Reset
                  }

                /* Select next gravity */
                for(i = 0; -1 == id && i < size; i++)
                  {
                    cid = (int)g->data.string[i] - GRAVITYSTRLIMIT;

                    /* Toggle gravity */
                    if(c->gravityid == cid)
                      {
                        /* Select first or next id */
                        if(i == size - 1) id = fid;
                        else id = (int)g->data.string[i + 1] - GRAVITYSTRLIMIT;
                      }
                  }
                if(-1 == id) id = fid; ///< Fallback

                /* Sanity check */
                if(0 <= id && id < subtle->gravities->ndata)
                  {
                    subClientArrange(c, id, c->screenid);
                    subClientRestack(c, SUB_CLIENT_RESTACK_UP);

                    /* Warp pointer */
                    if(!(subtle->flags & SUB_SUBTLE_SKIP_WARP))
                      subClientWarp(c);

                    /* Hook: Tile */
                    subHookCall(SUB_HOOK_TILE, NULL);
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_KILL: /* {{{ */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
              subClientClose(c);
            break; /* }}} */
          case SUB_GRAB_VIEW_FOCUS:
          case SUB_GRAB_VIEW_SWAP: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              {
                int sid = -1;

                /* Find screen: Prefer screen of current window */
                if(subtle->flags & SUB_SUBTLE_SKIP_WARP &&
                    (c = CLIENT(subSubtleFind(subtle->windows.focus[0],
                    CLIENTID))) && VISIBLE(c))
                  sid = c->screenid;
                else subScreenFind(x, y, &sid);

                /* Focus or swap */
                subViewFocus(VIEW(subtle->views->data[g->data.num]), sid,
                  (SUB_GRAB_VIEW_SWAP == flag), True);
              }
            break; /* }}} */
          case SUB_GRAB_VIEW_SELECT: /* {{{ */
              {
                int vid = 0, sid = 0;

                /* Find screen: Prefer screen of current window */
                if(subtle->flags & SUB_SUBTLE_SKIP_WARP &&
                    (c = CLIENT(subSubtleFind(subtle->windows.focus[0],
                    CLIENTID))) && VISIBLE(c))
                  {
                    s   = SCREEN(subArrayGet(subtle->screens, c->screenid));
                    sid = c->screenid;
                  }
                else s = subScreenFind(x, y, &sid);

                /* Select view */
                if(SUB_VIEW_NEXT == g->data.num)
                  {
                    /* Cycle if necessary */
                    if(s->viewid < (subtle->views->ndata - 1))
                      vid = s->viewid + 1;
                  }
                else if(SUB_VIEW_PREV == g->data.num)
                  {
                    /* Cycle if necessary */
                    if(0 < s->viewid) vid = s->viewid - 1;
                    else vid = subtle->views->ndata - 1;
                  }

                subViewFocus(subtle->views->data[vid], sid, False, True);
              }
            break; /* }}} */
          case SUB_GRAB_SCREEN_JUMP: /* {{{ */
            if(g->data.num < subtle->screens->ndata)
              subScreenWarp(SCREEN(subtle->screens->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RELOAD: /* {{{ */
            if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD;
            break; /* }}} */
          case SUB_GRAB_SUBTLE_QUIT: /* {{{ */
            if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RESTART: /* {{{ */
            if(subtle)
              {
                subtle->flags &= ~SUB_SUBTLE_RUN;
                subtle->flags |= SUB_SUBTLE_RESTART;
              }
            break; /* }}} */
          default: subSubtleLogWarn("Cannot find grab action!\n");
        }
    }
} /* }}} */

/* EventMap {{{ */
static void
EventMap(XMapEvent *ev)
{
  SubTray *t = NULL;

  /* Check if we know the window */
  if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      t->flags &= ~SUB_TRAY_DEAD;

      subTrayUpdate();
      subScreenUpdate();
      subScreenRender();
    }

  subSubtleLogDebugEvents("Map: win=%#lx\n", ev->window);
} /* }}} */

/* EventMapping {{{ */
static void
EventMapping(XMappingEvent *ev)
{
  XRefreshKeyboardMapping(ev);

  /* Update grabs */
  if(MappingKeyboard == ev->request)
    {
      subGrabUnset(ROOT);
      subGrabSet(ROOT, SUB_GRAB_KEY);
    }

  subSubtleLogDebugEvents("Map: win=%#lx\n", ev->window);
} /* }}} */

/* EventMapRequest {{{ */
static void
EventMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Check if we know the window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      c->flags &= ~SUB_CLIENT_DEAD;
      c->flags |= SUB_CLIENT_ARRANGE;

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();
    }
  else if((c = subClientNew(ev->window)))
    {
      subArrayPush(subtle->clients, (void *)c);
      subClientRestack(c, SUB_CLIENT_RESTACK_UP);

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      EventQueuePop(ev->window, SUB_TYPE_CLIENT);

      /* Hook: Create */
      subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_CREATE),
        (void *)c);
    }

  subSubtleLogDebugEvents("MapRequest: win=%#lx\n", ev->window);
} /* }}} */

/* EventMessage {{{ */
static void
EventMessage(XClientMessageEvent *ev)
{
  SubClient *c = NULL;
  SubTray *r = NULL;

  /* Messages for root window {{{ */
  if(ROOT == ev->window)
    {
      SubPanel   *p = NULL;
      SubTag     *t = NULL;
      SubView    *v = NULL;
      SubGravity *g = NULL;
      SubScreen  *s = NULL;

      switch(subEwmhFind(ev->message_type))
        {
          /* ICCCM */
          case SUB_EWMH_NET_CURRENT_DESKTOP: /* {{{ */
            /* Switchs views of screen */
            if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata)
              {
                int sid = 0;

                /* Focus view of specified or current screen */
                if(0 > ev->data.l[2] || ev->data.l[2] >= subtle->screens->ndata)
                  {
                    /* Find screen: Prefer screen of current window */
                    if(subtle->flags & SUB_SUBTLE_SKIP_WARP &&
                        (c = CLIENT(subSubtleFind(subtle->windows.focus[0],
                        CLIENTID))) && VISIBLE(c))
                      sid = c->screenid;
                    else subScreenCurrent(&sid);
                  }
                else sid = ev->data.l[2];

                subViewFocus(subtle->views->data[ev->data.l[0]],
                  sid, True, True);
              }
            break; /* }}} */
          case SUB_EWMH_NET_ACTIVE_WINDOW: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))))
              {
                if(!(VISIBLE(c))) ///< Client is on current view?
                  {
                    int i;

                    /* Find matching view */
                    for(i = 0; i < subtle->views->ndata; i++)
                      {
                        if(c && (VIEW(subtle->views->data[i])->tags & c->tags ||
                            c->flags & SUB_CLIENT_MODE_STICK))
                          {
                            subViewFocus(VIEW(subtle->views->data[i]),
                              c->screenid, False, True);
                            break;
                          }
                      }
                  }

                subClientFocus(c, True);
              }
            else if((r = TRAY(subSubtleFind(ev->data.l[0], TRAYID))))
              {
                XSetInputFocus(subtle->dpy, r->win,
                  RevertToPointerRoot, CurrentTime);
              }
            break; /* }}} */
          case SUB_EWMH_NET_RESTACK_WINDOW: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[1], CLIENTID))))
              subClientRestack(c, Above == ev->data.l[2] ?
                SUB_CLIENT_RESTACK_UP : SUB_CLIENT_RESTACK_DOWN);
            break; /* }}} */

          /* subtle */
          case SUB_EWMH_SUBTLE_CLIENT_TAGS: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))))
              {
                int i, flags = 0, tags = 0;

                /* Select only new tags */
                tags = (c->tags ^ (int)ev->data.l[1]) & (int)ev->data.l[1];

                /* Remove highlight of tagless, urgent client */
                if(0 == tags && c->flags & SUB_CLIENT_MODE_URGENT)
                  subtle->urgent_tags &= ~c->tags;

                /* Update tags and assign properties */
                for(i = 0; i < 31; i++)
                  if(tags & (1L << (i + 1))) subClientTag(c, i, &flags);

                subClientToggle(c, flags, True); ///< Toggle flags
                c->tags = (int)ev->data.l[1]; ///< Write all tags

                /* EWMH: Tags */
                subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS,
                  (long *)&c->tags, 1);

                subScreenConfigure();

                /* Check visibility of focus window after updating tags
                 * and reactivate grabs if necessary */
                if(subtle->windows.focus[0] == c->win &&
                    !VISIBLE(c))
                  {
                    c = subClientNext(c->screenid, False);
                    if(c) subClientFocus(c, True);
                  }

                subScreenUpdate();
                subScreenRender();
              }
            else EventQueuePush(ev, SUB_TYPE_CLIENT);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_CLIENT_RETAG: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))))
              {
                int flags = 0;

                c->tags = 0; ///> Reset tags

                subClientRetag(c, &flags);
                subClientToggle(c, (~c->flags & flags), True); ///< Toggle flags

                if(VISIBLE(c) ||
                    flags & SUB_CLIENT_MODE_FULL)
                  {
                    subScreenConfigure();
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_CLIENT_GRAVITY: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))) &&
                ((g = GRAVITY(subArrayGet(subtle->gravities,
                (int)ev->data.l[1])))))
              {
                /* Set gravity for specified view */
                if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[2]))))
                  {
                    c->gravities[(int)ev->data.l[2]] = (int)ev->data.l[1];

                    if(subtle->visible_views & (1L << ((int)ev->data.l[2] + 1)))
                      {
                        subClientArrange(c, c->gravities[(int)ev->data.l[2]], c->screenid);
                        XRaiseWindow(subtle->dpy, c->win);

                        /* Warp pointer */
                        if(!(subtle->flags & SUB_SUBTLE_SKIP_WARP))
                          subClientWarp(c);

                        /* Hook: Tile */
                        subHookCall(SUB_HOOK_TILE, NULL);
                      }
                  }
                else if(VISIBLE(c))
                  {
                    subClientArrange(c, (int)ev->data.l[1], c->screenid);
                    XRaiseWindow(subtle->dpy, c->win);

                    /* Warp pointer */
                    if(!(subtle->flags & SUB_SUBTLE_SKIP_WARP))
                      subClientWarp(c);

                    /* Hook: Tile */
                    subHookCall(SUB_HOOK_TILE, NULL);
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_CLIENT_FLAGS: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))))
              {
                int flags = 0;

                /* Translate flags */
                if(ev->data.l[1] & SUB_EWMH_FULL)       flags |= SUB_CLIENT_MODE_FULL;
                if(ev->data.l[1] & SUB_EWMH_FLOAT)      flags |= SUB_CLIENT_MODE_FLOAT;
                if(ev->data.l[1] & SUB_EWMH_STICK)      flags |= SUB_CLIENT_MODE_STICK;
                if(ev->data.l[1] & SUB_EWMH_RESIZE)     flags |= SUB_CLIENT_MODE_RESIZE;
                if(ev->data.l[1] & SUB_EWMH_URGENT)     flags |= SUB_CLIENT_MODE_URGENT;
                if(ev->data.l[1] & SUB_EWMH_ZAPHOD)     flags |= SUB_CLIENT_MODE_ZAPHOD;
                if(ev->data.l[1] & SUB_EWMH_FIXED)      flags |= SUB_CLIENT_MODE_FIXED;
                if(ev->data.l[1] & SUB_EWMH_BORDERLESS) flags |= SUB_CLIENT_MODE_BORDERLESS;

                subClientToggle(c, flags, False);

                /* Configure and render when necessary */
                if(VISIBLE(c) ||
                    flags & (SUB_CLIENT_MODE_FULL|SUB_CLIENT_MODE_URGENT))
                  {
                    subScreenConfigure();
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            else EventQueuePush(ev, SUB_TYPE_CLIENT);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_GRAVITY_NEW: /* {{{ */
            if(ev->data.b)
              {
                XRectangle geom = { 0 };
                char buf[30] = { 0 };

                sscanf(ev->data.b, "%hdx%hd+%hd+%hd#%s", &geom.x, &geom.y,
                  &geom.width, &geom.height, buf);

                /* Add gravity */
                g = subGravityNew(buf, &geom);

                subArrayPush(subtle->gravities, (void *)g);
                subGravityPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_GRAVITY_FLAGS: /* {{{ */
            if((g = GRAVITY(subArrayGet(subtle->gravities,
                (int)ev->data.l[0]))))
              {
                int i, flags = 0;

                /* Translate and update flags */
                if(ev->data.l[1] & SUB_EWMH_HORZ) flags |= SUB_GRAVITY_HORZ;
                if(ev->data.l[1] & SUB_EWMH_VERT) flags |= SUB_GRAVITY_VERT;

                g->flags = (g->flags & SUB_TYPE_GRAVITY) | flags;

                /* Find clients with that gravity and mark them for arrange */
                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    c = CLIENT(subtle->clients->data[i]);

                    if(c->gravityid == ev->data.l[0])
                      c->flags |= SUB_CLIENT_ARRANGE;
                  }

                subScreenConfigure();
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_GRAVITY_KILL: /* {{{ */
            if((g = GRAVITY(subArrayGet(subtle->gravities, (int)ev->data.l[0]))))
              {
                int i;

                /* Check clients if gravity is in use */
                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    if((c = CLIENT(subtle->clients->data[i])) && c->gravityid == ev->data.l[0])
                      {
                        subClientArrange(c, 0, -1); ///< Fallback to first gravity
                        XRaiseWindow(subtle->dpy, c->win);

                        /* Warp pointer */
                        if(!(subtle->flags & SUB_SUBTLE_SKIP_WARP))
                          subClientWarp(c);
                      }
                  }

                /* Finallly remove gravity */
                subArrayRemove(subtle->gravities, (void *)g);
                subGravityKill(g);
                subGravityPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SCREEN_JUMP: /* {{{ */
            if((s = SCREEN(subArrayGet(subtle->screens,
                (int)ev->data.l[0]))))
              {
                subScreenWarp(s);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_DATA: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])) &&
                p->sublet->flags & SUB_SUBLET_DATA)
              {
                subRubyCall(SUB_CALL_DATA, p->sublet->instance, NULL);
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_STYLE: /* {{{ */
            if(ev->data.b)
              {
                int subletid = 0;
                char name[30] = { 0 };

                sscanf(ev->data.b, "%d#%s", &subletid, name);

                /* Find sublet and state */
                if((p = EventFindSublet(subletid)))
                  {
                    int styleid = -1;
                    subStyleFind(&subtle->styles.sublets, name, &styleid);

                    p->sublet->styleid = -1 != styleid ? styleid : -1;
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_FLAGS: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                /* Update visibility */
                if(ev->data.l[1] & SUB_EWMH_VISIBLE &&
                    p->flags & SUB_PANEL_HIDDEN)
                  {
                    p->flags &= ~SUB_PANEL_HIDDEN;
                    subScreenUpdate();
                    subScreenRender();
                  }
                else if(ev->data.l[1] & SUB_EWMH_HIDDEN &&
                    !(p->flags & SUB_PANEL_HIDDEN))
                  {
                    p->flags |= SUB_PANEL_HIDDEN;
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_UPDATE: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_KILL: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                subRubyUnloadSublet(p);
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_NEW: /* {{{ */
            if(ev->data.b)
              {
                int duplicate = False;

                if((t = subTagNew(ev->data.b, &duplicate)) && !duplicate)
                  {
                    subArrayPush(subtle->tags, (void *)t);
                    subTagPublish();

                    /* Hook: Create */
                    subHookCall((SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_CREATE),
                      (void *)t);
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_KILL: /* {{{ */
            if((t = TAG(subArrayGet(subtle->tags, (int)ev->data.l[0]))))
              {
                int i, reconf = False;

                /* Untag views */
                for(i = 0; i < subtle->views->ndata; i++) ///< Views
                  {
                    v      = VIEW(subtle->views->data[i]);
                    reconf = v->tags & (1L << ((int)ev->data.l[0] + 1));

                    EventUntag(CLIENT(v), (int)ev->data.l[0]);
                  }

                /* Untag clients */
                for(i = 0; i < subtle->clients->ndata; i++)
                  EventUntag(CLIENT(subtle->clients->data[i]),
                    (int)ev->data.l[0]);

                /* Remove tag */
                subArrayRemove(subtle->tags, (void *)t);
                subTagKill(t);
                subTagPublish();
                subViewPublish();

                if(reconf) subScreenConfigure();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_NEW: /* {{{ */
            if(ev->data.b && (v = subViewNew(ev->data.b, NULL)))
              {
                subArrayPush(subtle->views, (void *)v);
                subClientDimension(-1); ///< Grow
                subViewPublish();
                subScreenUpdate();
                subScreenRender();

                EventQueuePop(subtle->views->ndata - 1, SUB_TYPE_VIEW);

                /* Hook: Create */
                subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_CREATE),
                  (void *)v);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_TAGS: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views,
                (int)ev->data.l[0]))))
              {
                v->tags = (int)ev->data.l[1]; ///< Action

                subViewPublish();

                /* Reconfigure if view is visible */
                if(subtle->visible_views & (1L << (ev->data.l[0] + 1)))
                  subScreenConfigure();
              }
            else EventQueuePush(ev, SUB_TYPE_VIEW);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_STYLE: /* {{{ */
            if(ev->data.b)
              {
                int view_id = 0;
                char name[30] = { 0 };

                sscanf(ev->data.b, "%d#%s", &view_id, name);

                /* Find sublet and state */
                if((v = VIEW(subArrayGet(subtle->views, view_id))))
                  {
                    int style_id = -1;
                    subStyleFind(&subtle->styles.views, name, &style_id);

                    v->styleid = -1 != style_id ? style_id : -1;
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_KILL: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                int visible = !!(subtle->visible_views & (1L << (ev->data.l[0] + 1)));

                subArrayRemove(subtle->views, (void *)v);
                subClientDimension((int)ev->data.l[0]); ///< Shrink
                subViewKill(v);
                subViewPublish();
                subScreenUpdate();
                subScreenRender();

                if(visible)
                  subViewFocus(VIEW(subtle->views->data[0]), -1, False, True);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RENDER: /* {{{ */
            subScreenRender();
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RELOAD: /* {{{ */
            if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD;
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RESTART: /* {{{ */
            if(subtle)
              {
                subtle->flags &= ~SUB_SUBTLE_RUN;
                subtle->flags |= SUB_SUBTLE_RESTART;
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_QUIT: /* {{{ */
            if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;
            break; /* }}} */
          default: break;
        }
    } /* }}} */
  /* Messages for tray window {{{ */
  else if(ev->window == subtle->windows.tray)
    {
      switch(subEwmhFind(ev->message_type))
        {
          case SUB_EWMH_NET_SYSTEM_TRAY_OPCODE: /* {{{ */
            switch(ev->data.l[1])
              {
                case XEMBED_EMBEDDED_NOTIFY: /* {{{ */
                  if(!(r = TRAY(subSubtleFind(ev->window, TRAYID))))
                    {
                      if((r = subTrayNew(ev->data.l[2])))
                        {
                          subArrayPush(subtle->trays, (void *)r);
                          subTrayPublish();
                          subTrayUpdate();
                          subScreenUpdate();
                          subScreenRender();
                        }
                    }
                  break; /* }}} */
                case XEMBED_REQUEST_FOCUS: /* {{{ */
                  subEwmhMessage(r->win, SUB_EWMH_XEMBED, 0xFFFFFF,
                    CurrentTime, XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
                  break; /* }}} */
              }
            break; /* }}} */
          default: break;
        }
    } /* }}} */
  /* Messages for tray windows {{{ */
  else if((r = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      switch(subEwmhFind(ev->message_type))
        {
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subTrayClose(r);
            break; /* }}} */
          default: break;
        }
    } /* }}} */
  /* Messages for client windows {{{ */
  else if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      DEAD(c);

      switch(subEwmhFind(ev->message_type))
        {
          case SUB_EWMH_NET_WM_STATE: /* {{{ */
              {
                int flags = 0;

                /* Translate properties */
                subEwmhTranslateWMState(ev->data.l[1], &flags);
                subEwmhTranslateWMState(ev->data.l[2], &flags);

                /* Since we always toggle we need to be careful */
                switch(ev->data.l[0])
                  {
                    case _NET_WM_STATE_ADD:
                      flags = (~c->flags & flags);
                      break;
                    case _NET_WM_STATE_REMOVE:
                      flags = (c->flags & flags);
                      break;
                    case _NET_WM_STATE_TOGGLE: break;
                  }

                subClientToggle(c, flags, True);

                /* Update screen and focus */
                if(VISIBLE(c) ||
                    flags & SUB_CLIENT_MODE_STICK)
                  {
                    subScreenConfigure();

                    if(!VISIBLE(c))
                      {
                        c = subClientNext(c->screenid, False);
                        if(c) subClientFocus(c, True);
                      }

                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subClientClose(c);
            break; /* }}} */
          case SUB_EWMH_NET_MOVERESIZE_WINDOW: /* {{{ */
              {
                SubScreen *s = SCREEN(subtle->screens->data[c->screenid]);

                if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
                  subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);

                c->geom.x      = ev->data.l[1];
                c->geom.y      = ev->data.l[2];
                c->geom.width  = ev->data.l[3];
                c->geom.height = ev->data.l[4];

                subClientResize(c, &(s->geom), True);

                XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
                  c->geom.width, c->geom.height);

                if(VISIBLE(c))
                  {
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          default: break;
        }
    } /* }}} */

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->dpy, ev->message_type);

    subSubtleLogDebugEvents("ClientMessage: name=%s, type=%ld,"
      "format=%d, win=%#lx\n",
      name ? name : "n/a", ev->message_type, ev->format, ev->window);
    subSubtleLogDebugEvents("ClientMessage: [0]=%#lx, [1]=%#lx,"
      "[2]=%#lx, [3]=%#lx, [4]=%#lx\n",
      ev->data.l[0], ev->data.l[1], ev->data.l[2], 
      ev->data.l[3], ev->data.l[4]);

    if(name) XFree(name);
  }
#endif
} /* }}} */

/* EventProperty {{{ */
static void
EventProperty(XPropertyEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;
  int id = subEwmhFind(ev->atom);

  if(XA_WM_NAME == ev->atom) id = SUB_EWMH_WM_NAME;

  /* Supported properties */
  switch(id)
    {
      case SUB_EWMH_WM_NAME: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            if(c->name) free(c->name);
            subSharedPropertyName(subtle->dpy, c->win, &c->name, c->klass);

            if(subtle->windows.focus[0] == c->win)
              {
                subScreenUpdate();
                subScreenRender();
              }
          }
        break; /* }}} */
      case SUB_EWMH_WM_NORMAL_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            int flags = 0;

            subClientSetSizeHints(c, &flags);
            subClientToggle(c, (~c->flags & flags), True); ///< Only enable

            if(VISIBLE(c))
              {
                subScreenUpdate();
                subScreenRender();
              }
          }
        else if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
          {
            subTrayConfigure(t);
            subTrayUpdate();
            subScreenUpdate();
            subScreenRender();
          }
        break; /* }}} */
      case SUB_EWMH_WM_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            int flags = 0;

            /* Check changes */
            subClientSetWMHints(c, &flags);
            subClientToggle(c, (~c->flags & flags), True);

            /* Update and render when necessary */
            if(VISIBLE(c) ||
                flags & SUB_CLIENT_MODE_URGENT)
              {
                subScreenUpdate();
                subScreenRender();
              }
          }
        break; /* }}} */
      case SUB_EWMH_NET_WM_STRUT: /* {{{ */
         if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            subClientSetStrut(c);
            subScreenUpdate();
            subSubtleLogDebug("Hints: Updated strut hints\n");
          }
        break; /* }}} */
      case SUB_EWMH_MOTIF_WM_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          subClientSetMWMHints(c);
        break; /* }}} */
      case SUB_EWMH_XEMBED_INFO: /* {{{ */
        if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
          {
            subTraySetState(t);
            subTrayUpdate();
            subScreenUpdate();
            subScreenRender();
          }
        break; /* }}} */
    }

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->dpy, ev->atom);

    subSubtleLogDebugEvents("Property: name=%s, type=%ld, win=%#lx\n",
      name ? name : "n/a", ev->atom, ev->window);

    if(name) XFree(name);
  }
#endif
} /* }}} */

/* EventSelft on heection {{{ */
void
EventSelection(XSelectionClearEvent *ev)
{
  /* Handle selection clear events */
  if(ev->window == subtle->windows.tray) ///< Tray selection
    {
      subtle->flags &= ~SUB_SUBTLE_TRAY;
      subTrayDeselect();
    }
  else if(ev->window == subtle->windows.support) ///< Session selection
    {
      subSubtleLogWarn("Leaving the field\n");
      subtle->flags &= ~SUB_SUBTLE_RUN; ///< Exit
    }

  subSubtleLogDebugEvents("SelectionClear: win=%#lx, tray=%#lx, support=%#lx\n",
    ev->window, subtle->windows.tray, subtle->windows.support);
} /* }}} */

/* EventUnmap {{{ */
static void
EventUnmap(XUnmapEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      int sid = (subtle->windows.focus[0] == c->win ? c->screenid : -1); ///< Save

      /* Set withdrawn state (see ICCCM 4.1.4) */
      subEwmhSetWMState(c->win, WithdrawnState);

      /* Ignore our generated unmap events */
      if(c->flags & SUB_CLIENT_UNMAP)
        {
          c->flags &= ~SUB_CLIENT_UNMAP;

          return;
        }

      /*  Kill client */
      subArrayRemove(subtle->clients, (void *)c);
      subClientKill(c);
      subClientPublish(False);

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if((c = subClientNext(sid, False))) subClientFocus(c, True);
    }
  else if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      int focus = (subtle->windows.focus[0] == ev->window); ///< Save

      /* Set withdrawn state (see ICCCM 4.1.4) */
      subEwmhSetWMState(t->win, WithdrawnState);

      /* Ignore our own generated unmap events */
      if(t->flags & SUB_TRAY_UNMAP)
        {
          t->flags &= ~SUB_TRAY_UNMAP;

          return;
        }

      /*  Kill tray */
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subTrayPublish();

      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(focus && (c = subClientNext(0, False))) subClientFocus(c, True);
    }

  subSubtleLogDebugEvents("Unmap: win=%#lx\n", ev->window);
} /* }}} */

/* Public */

 /** subEventWatchAdd {{{
  * @brief Add descriptor to watch list
  * @param[in]  fd  File descriptor
  **/

void
subEventWatchAdd(int fd)
{
  /* Add descriptor to list */
  watches = (struct pollfd *)subSharedMemoryRealloc(watches,
    (nwatches + 1) * sizeof(struct pollfd));

  watches[nwatches].fd        = fd;
  watches[nwatches].events    = POLLIN;
  watches[nwatches++].revents = 0;
} /* }}} */

 /** subEventWatchDel {{{
  * @brief Del fd from watch list
  * @param[in]  fd  File descriptor
  **/

void
subEventWatchDel(int fd)
{
  int i, j;

  for(i = 0; i < nwatches; i++)
    {
      if(watches[i].fd == fd)
        {
          for(j = i; j < nwatches - 1; j++)
            watches[j] = watches[j + 1];
          break;
        }
    }

  nwatches--;
  watches = (struct pollfd *)subSharedMemoryRealloc(watches,
    nwatches * sizeof(struct pollfd));
} /* }}} */

 /** subEventLoop {{{
  * @brief Event all X events
  **/

void
subEventLoop(void)
{
  int i, timeout = 1, nevents = 0;
  XEvent ev;
  time_t now;
  SubPanel *p = NULL;
  SubClient *c = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  /* Update screens and panels */
  subScreenConfigure();
  subScreenUpdate();
  subScreenRender();
  subPanelPublish();

  /* Add watches */
  subEventWatchAdd(ConnectionNumber(subtle->dpy));
#ifdef HAVE_SYS_INOTIFY_H
  subEventWatchAdd(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  /* Set tray selection */
  if(subtle->flags & SUB_SUBTLE_TRAY) subTraySelect();

  subtle->flags |= SUB_SUBTLE_RUN;
  XSync(subtle->dpy, False); ///< Sync before going on

  /* Set grabs and focus first client if any */
  subGrabSet(ROOT, SUB_GRAB_KEY);
  c = subClientNext(0, False);
  if(c) subClientFocus(c, True);

  /* Hook: Start */
  subHookCall(SUB_HOOK_START, NULL);

  /* Start main loop */
  while(subtle && subtle->flags & SUB_SUBTLE_RUN)
    {
      now = subSubtleTime();

      /* Check if we need to reload */
      if(subtle->flags & SUB_SUBTLE_RELOAD)
        {
          int tray = subtle->flags & SUB_SUBTLE_TRAY;

          subtle->flags &= ~SUB_SUBTLE_RELOAD;
          subRubyReloadConfig();

          /* Update tray selection */
          if(tray && !(subtle->flags & SUB_SUBTLE_TRAY))
            subTrayDeselect();
          else if(!tray && subtle->flags & SUB_SUBTLE_TRAY)
            subTraySelect();
        }

      /* Data ready on any connection */
      if(0 < (nevents = poll(watches, nwatches, timeout * 1000)))
        {
          for(i = 0; i < nwatches; i++) ///< Find descriptor
            {
              if(0 != watches[i].revents)
                {
                  if(watches[i].fd == ConnectionNumber(subtle->dpy)) ///< X events {{{
                    {
                      while(XPending(subtle->dpy)) ///< X events
                        {
                          XNextEvent(subtle->dpy, &ev);
                          switch(ev.type)
                            {
                              case ColormapNotify:    EventColormap(&ev.xcolormap);                 break;
                              case ConfigureNotify:   EventConfigure(&ev.xconfigure);               break;
                              case ConfigureRequest:  EventConfigureRequest(&ev.xconfigurerequest); break;
                              case EnterNotify:
                              case LeaveNotify:       EventCrossing(&ev.xcrossing);                 break;
                              case DestroyNotify:     EventDestroy(&ev.xdestroywindow);             break;
                              case Expose:            EventExpose(&ev.xexpose);                     break;
                              case FocusIn:           EventFocus(&ev.xfocus);                       break;
                              case ButtonPress:
                              case KeyPress:          EventGrab(&ev);                               break;
                              case MapNotify:         EventMap(&ev.xmap);                           break;
                              case MappingNotify:     EventMapping(&ev.xmapping);                   break;
                              case MapRequest:        EventMapRequest(&ev.xmaprequest);             break;
                              case ClientMessage:     EventMessage(&ev.xclient);                    break;
                              case PropertyNotify:    EventProperty(&ev.xproperty);                 break;
                              case SelectionClear:    EventSelection(&ev.xselectionclear);          break;
                              case UnmapNotify:       EventUnmap(&ev.xunmap);                       break;
                              default: break;
                            }
                        }
                    } /* }}} */
#ifdef HAVE_SYS_INOTIFY_H
                  else if(watches[i].fd == subtle->notify) ///< Inotify {{{
                    {
                      if(0 < read(subtle->notify, buf, BUFLEN)) ///< Inotify events
                        {
                          struct inotify_event *event = (struct inotify_event *)&buf[0];

                          /* Skip unwatch events */
                          if(event && IN_IGNORED != event->mask)
                            {
                              if((p = PANEL(subSubtleFind(
                                  subtle->windows.support, event->wd))))
                                {
                                  subRubyCall(SUB_CALL_WATCH,
                                    p->sublet->instance, NULL);
                                  subScreenUpdate();
                                  subScreenRender();
                                }
                            }
                        }
                    } /* }}} */
#endif /* HAVE_SYS_INOTIFY_H */
                  else ///< Socket {{{
                    {
                      if((p = PANEL(subSubtleFind(subtle->windows.support,
                          watches[i].fd))))
                        {
                          subRubyCall(SUB_CALL_WATCH,
                            p->sublet->instance, NULL);
                          subScreenUpdate();
                          subScreenRender();
                        }
                    } /* }}} */
                }
            }
        }
      else if(0 == nevents) ///< Timeout waiting for data or error {{{
        {
          if(0 < subtle->sublets->ndata)
            {
              p = PANEL(subtle->sublets->data[0]);

              /* Update all pending sublets */
              while(p && p->sublet->flags & SUB_SUBLET_INTERVAL &&
                  p->sublet->time <= now)
                {
                  subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);

                  /* This may change during run */
                  if(p->sublet->flags & SUB_SUBLET_INTERVAL) 
                    {
                      p->sublet->time  = now + p->sublet->interval; ///< Adjust seconds
                      p->sublet->time -= p->sublet->time % p->sublet->interval;
                    }

                  subArraySort(subtle->sublets, subPanelCompare);
                }

              subScreenUpdate();
              subScreenRender();
            }
        } /* }}} */

      /* Set new timeout */
      if(0 < subtle->sublets->ndata)
        {
          p = PANEL(subtle->sublets->data[0]);

          timeout = p->sublet->flags & SUB_SUBLET_INTERVAL ?
            p->sublet->time - now : 60;
          if(0 >= timeout) timeout = 1; ///< Sanitize
        }
      else timeout = 60;
    }

  /* Drop tray selection */
  if(subtle->flags & SUB_SUBTLE_TRAY) subTrayDeselect();
} /* }}} */

 /** subEventFinish {{{
  * @brief Finish event processing
  **/

void
subEventFinish(void)
{

  if(watches) free(watches);
  if(queue)   free(queue);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
