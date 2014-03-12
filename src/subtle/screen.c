
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/screen.c,v 3201 2012/04/12 14:01:20 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

/* ScreenPublish {{{ */
static void
ScreenPublish(void)
{
  int i;
  long *workareas = NULL, *panels = NULL, *viewports = NULL;

  assert(subtle);

  /* EWMH: Workarea and screen panels */
  workareas = (long *)subSharedMemoryAlloc(4 * subtle->screens->ndata,
    sizeof(long));
  panels = (long *)subSharedMemoryAlloc(2 * subtle->screens->ndata,
    sizeof(long));

  /* Collect data*/
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Set workareas */
      workareas[i * 4 + 0] = s->geom.x;
      workareas[i * 4 + 1] = s->geom.y;
      workareas[i * 4 + 2] = s->geom.width;
      workareas[i * 4 + 3] = s->geom.height;

      /* Set panels */
      panels[i * 2 + 0] = s->flags & SUB_SCREEN_PANEL1 ? subtle->ph : 0;
      panels[i * 2 + 1] = s->flags & SUB_SCREEN_PANEL2 ? subtle->ph : 0;
    }

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_WORKAREA, workareas,
    4 * subtle->screens->ndata);
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_SCREEN_PANELS, panels,
    2 * subtle->screens->ndata);

  /* EWMH: Desktop viewport */
  viewports = (long *)subSharedMemoryAlloc(2 * subtle->screens->ndata,
    sizeof(long)); ///< Calloc inits with zero - great

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_VIEWPORT, viewports,
    2 * subtle->screens->ndata);

  free(workareas);
  free(panels);
  free(viewports);

  XSync(subtle->dpy, False); ///< Sync all changes

  subSubtleLogDebugSubtle("Publish: screens=%d\n",
    subtle->screens->ndata);
} /* }}} */

/* ScreenClear {{{ */
static void
ScreenClear(SubScreen *s,
  unsigned long col)
{
  /* Clear pixmap */
  XSetForeground(subtle->dpy, subtle->gcs.draw, col);
  XFillRectangle(subtle->dpy, s->drawable, subtle->gcs.draw,
    0, 0, s->base.width, subtle->ph);

   /* Draw stipple on panels */
  if(s->flags & SUB_SCREEN_STIPPLE)
    {
      XGCValues gvals;

      gvals.stipple = s->stipple;
      XChangeGC(subtle->dpy, subtle->gcs.stipple, GCStipple, &gvals);

      XFillRectangle(subtle->dpy, s->drawable, subtle->gcs.stipple,
        0, 0, s->base.width, subtle->ph);
    }
} /* }}} */

/* Public */

 /** subScreenInit {{{
  * @brief Init screens
  **/

void
subScreenInit(void)
{
  SubScreen *s = NULL;

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
  /* Check both but prefer xrandr */
  if(subtle->flags & SUB_SUBTLE_XRANDR)
    {
      XRRScreenResources *res = NULL;

      if((res = XRRGetScreenResourcesCurrent(subtle->dpy, ROOT)))
        {
          int i;
          XRRCrtcInfo *crtc = NULL;

          /* Query screens */
          for(i = 0; i < res->ncrtc; i++)
            {
              if((crtc = XRRGetCrtcInfo(subtle->dpy, res, res->crtcs[i])))
                {
                  /* Create new screen if crtc is enabled */
                  if(None != crtc->mode && (s = subScreenNew(crtc->x,
                      crtc->y, crtc->width, crtc->height)))
                    subArrayPush(subtle->screens, (void *)s);

                  XRRFreeCrtcInfo(crtc);
                }
            }

          XRRFreeScreenResources(res);
        }
    }
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  if(subtle->flags & SUB_SUBTLE_XINERAMA && 0 == subtle->screens->ndata &&
      XineramaIsActive(subtle->dpy))
    {
      int i, n = 0;
      XineramaScreenInfo *info = NULL;

      /* Query screens */
      if((info = XineramaQueryScreens(subtle->dpy, &n)))
        {
          for(i = 0; i < n; i++)
            {
              /* Create new screen */
              if((s = subScreenNew(info[i].x_org, info[i].y_org,
                  info[i].width, info[i].height)))
                subArrayPush(subtle->screens, (void *)s);
            }

          XFree(info);
        }
    }
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Create default screen */
  if(0 == subtle->screens->ndata)
    {
      /* Create new screen */
      if((s = subScreenNew(0, 0, subtle->width, subtle->height)))
        subArrayPush(subtle->screens, (void *)s);
    }

  printf("Running on %d screen(s)\n", subtle->screens->ndata);

  ScreenPublish();
  subScreenPublish();

  subSubtleLogDebugSubtle("init=screen\n");
} /* }}} */

 /** subScreenNew {{{
  * @brief Create a new view
  * @param[in]  x       X position of screen
  * @param[in]  y       y position of screen
  * @param[in]  width   Width of screen
  * @param[in]  height  Height of screen
  * @return Returns a #SubScreen or \p NULL
  **/

SubScreen *
subScreenNew(int x,
  int y,
  unsigned int width,
  unsigned int height)
{
  SubScreen *s = NULL;
  XSetWindowAttributes sattrs;
  unsigned long mask = 0;

  /* Create screen */
  s = SCREEN(subSharedMemoryAlloc(1, sizeof(SubScreen)));
  s->flags       = SUB_TYPE_SCREEN;
  s->geom.x      = x;
  s->geom.y      = y;
  s->geom.width  = width;
  s->geom.height = height;
  s->base        = s->geom; ///< Backup size
  s->viewid      = subtle->screens->ndata; ///< Init

  /* Create panel windows */
  sattrs.event_mask        = ButtonPressMask|EnterWindowMask|
    LeaveWindowMask|ExposureMask;
  sattrs.override_redirect = True;
  sattrs.background_pixmap = ParentRelative;
  mask                     = CWEventMask|CWOverrideRedirect|CWBackPixmap;

  s->panel1 = XCreateWindow(subtle->dpy, ROOT, 0, 1, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);
  s->panel2 = XCreateWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);

  XSaveContext(subtle->dpy, s->panel1, SCREENID, (void *)s);
  XSaveContext(subtle->dpy, s->panel2, SCREENID, (void *)s);

  subSubtleLogDebugSubtle("New: x=%d, y=%d, width=%u, height=%u\n",
    s->geom.x, s->geom.y, s->geom.width, s->geom.height);

  return s;
} /* }}} */

 /** subScreenFind {{{
  * @brief Find screen by coordinates
  * @param[in]     x    X coordinate
  * @param[in]     y    Y coordinate
  * @param[inout]  sid  Screen id
  * @return Returns a #SubScreen
  **/

SubScreen *
subScreenFind(int x,
  int y,
  int *sid)
{
  int i;
  SubScreen *ret = SCREEN(subtle->screens->data[0]);

  /* Check screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Check if coordinates are in screen rects */
      if((x >= s->base.x && x < s->base.x + s->base.width) &&
          (y >= s->base.y && y < s->base.y + s->base.height))
        {
          ret = s;
          if(sid) *sid = i;

          break;
        }
    }

  subSubtleLogDebugSubtle("Find\n");

  return ret;
} /* }}} */

 /** subScreenCurrent {{{
  * @brief Find screen by coordinates
  * @param[inout]  sid  Screen id
  * @return Current #SubScreen or \p NULL
  **/

SubScreen *
subScreenCurrent(int *sid)
{
  SubScreen *ret = NULL;

  /* Check if there is only one screen */
  if(1 == subtle->screens->ndata)
    {
      if(sid) *sid = 0;

      ret = SCREEN(subtle->screens->data[0]);
    }
  else
    {
      int rx = 0, ry = 0, x = 0, y = 0;
      Window root = None, win = None;
      unsigned int mask = 0;

      /* Get current screen */
      XQueryPointer(subtle->dpy, ROOT, &root, &win, &rx, &ry, &x, &y, &mask);

      ret = subScreenFind(rx, ry, sid);
    }

  subSubtleLogDebugSubtle("Current\n");

  return ret;
} /* }}} */

 /** subScreenConfigure {{{
  * @brief Configure screens
  **/

void
subScreenConfigure(void)
{
  int i;
  SubScreen *s = NULL;
  SubView *v = NULL;

  /* Reset visible tags, views and available clients */
  subtle->visible_tags  = 0;
  subtle->visible_views = 0;
  subtle->client_tags   = 0;

  /* Either check each client or just get visible clients */
  if(0 < subtle->clients->ndata)
    {
      int j;

      /* Check each client */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);
          int gravityid = 0, screenid = 0, viewid = 0, visible = 0;

          /* Ignore dead or just iconified clients */
          if(c->flags & SUB_CLIENT_DEAD) continue;

          /* Set available client tags to ease lookups */
          subtle->client_tags |= c->tags;

          /* Check view of each screen */
          for(j = 0; j < subtle->screens->ndata; j++)
            {
              s = SCREEN(subtle->screens->data[j]);
              v = VIEW(subtle->views->data[s->viewid]);

              /* Set visible tags and views to ease lookups */
              subtle->visible_tags  |= v->tags;
              subtle->visible_views |= (1L << (s->viewid + 1));

              /* Find visible clients */
              if(VISIBLETAGS(c, v->tags))
                {
                  /* Keep screen when sticky */
                  if(c->flags & SUB_CLIENT_MODE_STICK)
                    {
                      /* Keep gravity from sticky screen/view and not the one
                       * of the current screen/view in loop */
                      s = SCREEN(subtle->screens->data[c->screenid]);

                      screenid = c->screenid;
                    }
                  else screenid = j;

                  viewid    = s->viewid;
                  gravityid = c->gravities[s->viewid];
                  visible++;
                }
            }

          /* After all screens are checked.. */
          if(0 < visible)
            {
              /* Update client */
              subClientArrange(c, gravityid, screenid);
              XMapWindow(subtle->dpy, c->win);
              subEwmhSetWMState(c->win, NormalState);

              /* Warp after gravity and screen have been set if not disabled */
              if(c->flags & SUB_CLIENT_MODE_URGENT &&
                  !(subtle->flags & SUB_SUBTLE_SKIP_URGENT_WARP) &&
                  !(subtle->flags & SUB_SUBTLE_SKIP_WARP))
                subClientWarp(c);

              /* EWMH: Desktop, screen */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP,
                (long *)&viewid, 1);
              subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_SCREEN,
                (long *)&screenid, 1);
            }
          else ///< Unmap other windows
            {
              c->flags |= SUB_CLIENT_UNMAP; ///< Ignore next unmap
              subEwmhSetWMState(c->win, WithdrawnState);
              XUnmapWindow(subtle->dpy, c->win);
            }
        }
    }
  else
    {
      /* Check views of each screen */
      for(i = 0; i < subtle->screens->ndata; i++)
        {
          s = SCREEN(subtle->screens->data[i]);
          v = VIEW(subtle->views->data[s->viewid]);

          /* Set visible tags and views to ease lookups */
          subtle->visible_tags  |= v->tags;
          subtle->visible_views |= (1L << (s->viewid + 1));
        }
    }

  /* EWMH: Visible tags, views */
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VISIBLE_TAGS,
    (long *)&subtle->visible_tags, 1);
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VISIBLE_VIEWS,
    (long *)&subtle->visible_views, 1);

  XSync(subtle->dpy, False); ///< Sync before going on

  /* Hook: Configure */
  subHookCall(SUB_HOOK_TILE, NULL);

  subSubtleLogDebugSubtle("Configure\n");
} /* }}} */

 /** subScreenUpdate {{{
  * @brief Update screens
  **/

void
subScreenUpdate(void)
{
  int i;

  /* Update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);
      SubPanel *p = NULL;
      int j, npanel = 0, center = False, offset = 0;
      int x[4] = { 0 }, nspacer[4] = { 0 }; ///< Waste ints but it's easier for the algo
      int sw[4] = { 0 }, fix[4] = { 0 }, width[4] = { 0 }, spacer[4] = { 0 };

      /* Pass 1: Collect width for spacer sizes */
      for(j = 0; s->panels && j < s->panels->ndata; j++)
        {
          p = PANEL(s->panels->data[j]);

          subPanelUpdate(p);

          /* Check flags */
          if(p->flags & SUB_PANEL_HIDDEN)  continue;
          if(0 == npanel && p->flags & SUB_PANEL_BOTTOM)
            {
              npanel = 1;
              center = False;
            }
          if(p->flags & SUB_PANEL_CENTER) center = !center;

          /* Offset selects panel variables for either center or not */
          offset = center ? npanel + 2 : npanel;

          if(p->flags & SUB_PANEL_SPACER1) spacer[offset]++;
          if(p->flags & SUB_PANEL_SPACER2) spacer[offset]++;
          if(p->flags & SUB_PANEL_SEPARATOR1 &&
              subtle->styles.separator.separator)
            width[offset] += subtle->styles.separator.separator->width;
          if(p->flags & SUB_PANEL_SEPARATOR2 &&
              subtle->styles.separator.separator)
            width[offset] += subtle->styles.separator.separator->width;

          width[offset] += p->width;
        }

      /* Calculate spacer and fix sizes */
      for(j = 0; j < 4; j++)
        {
          if(0 < spacer[j])
            {
              sw[j]  = (s->base.width - width[j]) / spacer[j];
              fix[j] = s->base.width - (width[j] + spacer[j] * sw[j]);
            }
        }

      /* Pass 2: Move and resize windows */
      for(j = 0, npanel = 0, center = False;
          s->panels && j < s->panels->ndata; j++)
        {
          p = PANEL(s->panels->data[j]);

          /* Check flags */
          if(p->flags & SUB_PANEL_HIDDEN) continue;
          if(0 == npanel && p->flags & SUB_PANEL_BOTTOM)
            {
              /* Reset for new panel */
              npanel     = 1;
              nspacer[0] = 0;
              nspacer[2] = 0;
              x[0]       = 0;
              x[2]       = 0;
              center     = False;
            }
          if(p->flags & SUB_PANEL_CENTER) center = !center;

          /* Offset selects panel variables for either center or not */
          offset = center ? npanel + 2 : npanel;

          /* Set start position of centered panel items */
          if(center && 0 == x[offset])
            x[offset] = (s->base.width - width[offset]) / 2;

          /* Add separator before panel item */
          if(p->flags & SUB_PANEL_SEPARATOR1 &&
              subtle->styles.separator.separator)
            x[offset] += subtle->styles.separator.separator->width;

          /* Add spacer before item */
          if(p->flags & SUB_PANEL_SPACER1)
            {
              x[offset] += sw[offset];

              /* Increase last spacer size by rounding fix */
              if(++nspacer[offset] == spacer[offset])
                x[offset] += fix[offset];
            }

          /* Set panel position */
          if(p->flags & SUB_PANEL_TRAY)
            XMoveWindow(subtle->dpy, subtle->windows.tray, x[offset], 0);
          p->x = x[offset];

          /* Add separator after panel item */
          if(p->flags & SUB_PANEL_SEPARATOR2 &&
              subtle->styles.separator.separator)
            x[offset] += subtle->styles.separator.separator->width;

          /* Add spacer after item */
          if(p->flags & SUB_PANEL_SPACER2)
            {
              x[offset] += sw[offset];

              /* Increase last spacer size by rounding fix */
              if(++nspacer[offset] == spacer[offset])
                x[offset] += fix[offset];
            }

          x[offset] += p->width;
        }
    }

  subSubtleLogDebugSubtle("Update\n");
} /* }}} */

 /** subScreenRender {{{
  * @brief Render screens
  **/

void
subScreenRender(void)
{
  int i, j;

  /* Render all screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);
      Window panel = s->panel1;

      ScreenClear(s, subtle->styles.subtle.top);

      /* Render panel items */
      for(j = 0; s->panels && j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          if(p->flags & SUB_PANEL_HIDDEN) continue;
          if(panel != s->panel2 && p->flags & SUB_PANEL_BOTTOM)
            {
              XCopyArea(subtle->dpy, s->drawable, panel, subtle->gcs.draw,
                0, 0, s->base.width, subtle->ph, 0, 0);

              ScreenClear(s, subtle->styles.subtle.bottom);
              panel = s->panel2;
            }

          subPanelRender(p, s->drawable);
        }

      XCopyArea(subtle->dpy, s->drawable, panel, subtle->gcs.draw,
        0, 0, s->base.width, subtle->ph, 0, 0);
    }

  XSync(subtle->dpy, False); ///< Sync before going on

  subSubtleLogDebugSubtle("Render\n");
} /* }}} */

 /** subScreenResize {{{
  * @brief Resize screens
  **/

void
subScreenResize(void)
{
  int i;

  assert(subtle);

  /* Update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Add strut */
      s->geom.x      = s->base.x + subtle->styles.subtle.padding.left;
      s->geom.y      = s->base.y + subtle->styles.subtle.padding.top;
      s->geom.width  = s->base.width - subtle->styles.subtle.padding.left -
        subtle->styles.subtle.padding.right;
      s->geom.height = s->base.height - subtle->styles.subtle.padding.top -
        subtle->styles.subtle.padding.bottom;

      /* Update panels */
      if(s->flags & SUB_SCREEN_PANEL1)
        {
          XMoveResizeWindow(subtle->dpy, s->panel1, s->base.x, s->base.y,
            s->base.width, subtle->ph);
          XMapRaised(subtle->dpy, s->panel1);

          /* Update height */
          s->geom.y      += subtle->ph;
          s->geom.height -= subtle->ph;
        }
      else XUnmapWindow(subtle->dpy, s->panel1);

      if(s->flags & SUB_SCREEN_PANEL2)
        {
          XMoveResizeWindow(subtle->dpy, s->panel2, s->base.x,
            s->base.y + s->base.height - subtle->ph, s->base.width, subtle->ph);
          XMapRaised(subtle->dpy, s->panel2);

          /* Update height */
          s->geom.height -= subtle->ph;
        }
      else XUnmapWindow(subtle->dpy, s->panel2);

      /* Create/update drawable for double buffering */
      if(s->drawable) XFreePixmap(subtle->dpy, s->drawable);
      s->drawable = XCreatePixmap(subtle->dpy, ROOT, s->base.width, subtle->ph,
        XDefaultDepth(subtle->dpy, DefaultScreen(subtle->dpy)));
    }

  ScreenPublish();

  subSubtleLogDebugSubtle("Resize\n");
} /* }}} */

 /** subScreenWarp {{{
  * @brief warp pointer to screen
  * @param[in]  s  A #SubScreen
  **/

void
subScreenWarp(SubScreen *s)
{
  assert(s);

  /* Move pointer to screen center */
  XWarpPointer(subtle->dpy, None, ROOT, 0, 0, s->geom.x, s->geom.y,
    s->geom.x + s->geom.width / 2, s->geom.y + s->geom.height / 2);

  subSubtleLogDebugSubtle("Warp\n");
} /* }}} */

 /** SubScreenKill {{{
  * @brief Kill a screen
  * @param[in]  s  A #SubScreem
  **/

void
subScreenKill(SubScreen *s)
{
  assert(s);

  if(s->panels) subArrayKill(s->panels, True);

  /* Destroy panel windows */
  if(s->panel1)
    {
      XDeleteContext(subtle->dpy, s->panel1, SCREENID);
      XDestroyWindow(subtle->dpy, s->panel1);
    }
  if(s->panel2)
    {
      XDeleteContext(subtle->dpy, s->panel2, SCREENID);
      XDestroyWindow(subtle->dpy, s->panel2);
    }

  /* Destroy drawable */
  if(s->drawable) XFreePixmap(subtle->dpy, s->drawable);

  free(s);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subScreenPublish {{{
  * @brief Publish screens
  **/

void
subScreenPublish(void)
{
  int i;
  long *views = NULL;

  assert(subtle);

  /* EWMH: Views per screen */
  views = (long *)subSharedMemoryAlloc(subtle->screens->ndata,
    sizeof(long));

  /* Collect views */
  for(i = 0; i < subtle->screens->ndata; i++)
    views[i] = SCREEN(subtle->screens->data[i])->viewid;

  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_SCREEN_VIEWS,
    views, subtle->screens->ndata);

  free(views);

  XSync(subtle->dpy, False); ///< Sync all changes

  subSubtleLogDebugSubtle("Publish: screens=%d\n",
    subtle->screens->ndata);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
