
 /**
  * @package subtle
  *
  * @file Panel functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/panel.c,v 3205 2012/05/22 21:15:36 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

/* PanelRect {{{ */
static void
PanelRect(Drawable drawable,
  int x,
  int width,
  SubStyle *s)
{
  int mw = s->margin.left + s->margin.right;
  int mh = s->margin.top + s->margin.bottom;

  /* Filling */
  XSetForeground(subtle->dpy, subtle->gcs.draw, s->bg);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, width - mw, subtle->ph - mh);

  /* Borders */
  XSetForeground(subtle->dpy, subtle->gcs.draw, s->top);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, width - mw, s->border.top);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->right);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + width - s->border.right - s->margin.right, s->margin.top,
    s->border.right, subtle->ph - mh);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->bottom);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, subtle->ph - s->border.bottom - s->margin.bottom,
    width - mw, s->border.bottom);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->left);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, s->border.left, subtle->ph - mh);
} /* }}} */

/* PanelSeparator {{{ */
static void
PanelSeparator(int x,
  SubStyle *s,
  Drawable drawable)
{
  /* Set window background and border*/
  PanelRect(drawable, x, s->separator->width, s);

  subSharedDrawString(subtle->dpy, subtle->gcs.draw,
    s->font, drawable, x + STYLE_LEFT((*s)),
    s->font->y + STYLE_TOP((*s)), s->fg, s->bg,
    s->separator->string, strlen(s->separator->string));
} /* }}} */

/* PanelClientModes {{{ */
void
PanelClientModes(SubClient *c,
  char *buf,
  int *width)
{
  int x = 0;

  /* Collect window modes */
  if(c->flags & SUB_CLIENT_MODE_FULL)
    x += snprintf(buf + x, sizeof(buf), "%c", '+');
  if(c->flags & SUB_CLIENT_MODE_FLOAT)
    x += snprintf(buf + x, sizeof(buf), "%c", '^');
  if(c->flags & SUB_CLIENT_MODE_STICK)
    x += snprintf(buf + x, sizeof(buf), "%c", '*');
  if(c->flags & SUB_CLIENT_MODE_RESIZE)
    x += snprintf(buf + x, sizeof(buf), "%c", '~');
  if(c->flags & SUB_CLIENT_MODE_ZAPHOD)
    x += snprintf(buf + x, sizeof(buf), "%c", '=');
  if(c->flags & SUB_CLIENT_MODE_FIXED)
    x += snprintf(buf + x, sizeof(buf), "%c", '!');

  *width = subSharedStringWidth(subtle->dpy, subtle->styles.title.font,
    buf, strlen(buf), NULL, NULL, True);
} /* }}} */

/* PanelViewStyle {{{ */
static void
PanelViewStyle(SubView *v,
  int idx,
  int focus,
  SubStyle *s)
{
  /* Select style */
  if(subtle->styles.views.styles)
    {
      SubStyle *style = NULL;

      subStyleReset(s, -1);

      /* Pick base style */
      if(!(style = subArrayGet(subtle->styles.views.styles, v->styleid)))
        {
          if(subtle->styles.focus && focus)
            style = subtle->styles.focus;
          else if(subtle->styles.occupied && subtle->client_tags & v->tags)
            style = subtle->styles.occupied;
        }

      /* Merge base style or default */
      subStyleMerge(s, !style ? &subtle->styles.views : style);

      /* Apply modifiers */
      if(subtle->styles.urgent && subtle->urgent_tags & v->tags)
        subStyleMerge(s, subtle->styles.urgent);

      if(subtle->styles.visible)
        {
          if(subtle->visible_views & (1L << (idx + 1)))
            subStyleMerge(s, subtle->styles.visible);
        }

    }
  else subStyleMerge(s, &subtle->styles.views);
} /* }}} */

/* PanelSubletStyle {{{ */
static SubStyle *
PanelSubletStyle(SubPanel *p)
{
  SubStyle *s = NULL;

  /* Pick sublet style */
  if(subtle->styles.sublets.styles)
    s = subArrayGet(subtle->styles.sublets.styles, p->sublet->styleid);

  return s ? s : &subtle->styles.sublets;
} /* }}} */

/* Public */

 /** subPanelNew {{{
  * @brief Create a new panel
  * @param[in]  type  Type of the panel
  * @return Returns a #SubPanel or \p NULL
  **/

SubPanel *
subPanelNew(int type)
{
  SubPanel *p = NULL;

  /* Create new panel */
  p = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
  p->flags = (SUB_TYPE_PANEL|type);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_SUBLET|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        p->icon = ICON(subSharedMemoryAlloc(1, sizeof(SubIcon)));
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        p->sublet = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));

        /* Sublet specific */
        p->sublet->time    = subSubtleTime();
        p->sublet->text    = subTextNew();
        p->sublet->styleid = -1;
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->flags |= SUB_PANEL_DOWN;
        break; /* }}} */
    }

  subSubtleLogDebugSubtle("New: type=%d\n", type);

  return p;
} /* }}} */

 /** subPanelUpdate {{{
  * @brief Update panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelUpdate(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        p->width = p->icon->width + subtle->styles.separator.padding.left +
          subtle->styles.separator.padding.right + 4;
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        p->width = 0;

        if(p->keychain && p->keychain->keys)
          {
            /* Font offset, panel border and padding */
            p->width = subSharedStringWidth(subtle->dpy,
              subtle->styles.separator.font, p->keychain->keys,
              p->keychain->len, NULL, NULL, True) +
              subtle->styles.separator.padding.left +
              subtle->styles.separator.padding.right;
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
          {
            SubStyle *s = PanelSubletStyle(p);

            /* Ensure min width */
            p->width = MAX(s->min, p->sublet->width);
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        p->width = subtle->styles.clients.min;

        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            /* Find focus window */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
              {
                assert(c);
                DEAD(c);

                /* Exclude desktop type windows */
                if(!(c->flags & SUB_CLIENT_TYPE_DESKTOP))
                  {
                    char buf[5] = { 0 };
                    int width = 0, len = strlen(c->name);

                    PanelClientModes(c, buf, &width);

                    /* Font offset, panel border and padding */
                    p->width = subSharedStringWidth(subtle->dpy,
                      subtle->styles.title.font, c->name,
                      /* Limit string length */
                      len > subtle->styles.clients.right ?
                      subtle->styles.clients.right : len, NULL, NULL, True) +
                      width + STYLE_WIDTH(subtle->styles.title);

                    /* Ensure min width */
                    p->width = MAX(subtle->styles.clients.min, p->width);
                  }
              }
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->width = subtle->styles.views.min;

        if(0 < subtle->views->ndata)
          {
            int i;
            SubStyle s = { -1, .flags = SUB_TYPE_STYLE, .border = { -1 },
              .padding = { -1 }, .margin = { -1 }};

            /* Update for each view */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                SubView *v = VIEW(subtle->views->data[i]);

                /* Skip dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

                PanelViewStyle(v, i, (p->screen->viewid == i), &s);

                /* Update view width */
                if(v->flags & SUB_VIEW_ICON_ONLY)
                  v->width = v->icon->width + STYLE_WIDTH((s));
                else
                  {
                    v->width = subSharedStringWidth(subtle->dpy, s.font,
                      v->name, strlen(v->name), NULL, NULL, True) +
                      STYLE_WIDTH((s)) + (v->icon ? v->icon->width + 3 : 0);
                  }

                /* Ensure panel min width */
                p->width += MAX(s.min, v->width);
              }

            /* Add width of view separator if any */
            if(subtle->styles.viewsep)
              {
                p->width += (subtle->views->ndata - 1) *
                  subtle->styles.viewsep->separator->width;
              }
          }
        break; /* }}} */
    }

  subSubtleLogDebugSubtle("Update\n");
} /* }}} */

 /** subPanelRender {{{
  * @brief Render panel
  * @param[in]  p         A #SubPanel
  * @param[in]  drawable  Drawable for renderer
  **/

void
subPanelRender(SubPanel *p,
  Drawable drawable)
{
  assert(p);

  /* Draw separator before panel */
  if(p->flags & SUB_PANEL_SEPARATOR1 && subtle->styles.separator.separator)
    {
      PanelSeparator(p->x - subtle->styles.separator.separator->width,
        &subtle->styles.separator, drawable);
    }

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
          {
            int y = 0, icony = 0;

            y     = subtle->styles.separator.font->y +
              STYLE_TOP(subtle->styles.separator);
            icony = p->icon->height > y ? subtle->styles.separator.margin.top :
              y - p->icon->height;

            subSharedDrawIcon(subtle->dpy, subtle->gcs.draw,
              drawable, p->x + 2 + subtle->styles.separator.padding.left, icony,
              p->icon->width, p->icon->height, subtle->styles.sublets.fg,
              subtle->styles.sublets.bg, p->icon->pixmap, p->icon->bitmap);
          }
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        if(p->keychain && p->keychain->keys)
          {
            subSharedDrawString(subtle->dpy, subtle->gcs.draw,
              subtle->styles.separator.font, drawable,
              p->x + STYLE_LEFT(subtle->styles.separator),
              subtle->styles.separator.font->y +
              STYLE_TOP(subtle->styles.separator),
              subtle->styles.title.fg, subtle->styles.title.bg,
              p->keychain->keys, strlen(p->keychain->keys));
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
          {
            SubStyle *s = PanelSubletStyle(p);

            /* Set window background and border*/
            PanelRect(drawable, p->x, p->width, s);

            /* Render text parts */
            subTextRender(p->sublet->text, s->font, subtle->gcs.draw,
              drawable, p->x + STYLE_LEFT((*s)), s->font->y +
              STYLE_TOP((*s)), s->fg, s->icon, s->bg);
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP) && VISIBLE(c))
              {
                int x = 0, y = 0, width = 0, len = 0;
                char buf[5] = { 0 };

                DEAD(c);

                PanelClientModes(c, buf, &width);

                /* Set window background and border*/
                PanelRect(drawable, p->x, p->width, &subtle->styles.title);

                /* Draw modes and title */
                len = strlen(c->name);
                x   = p->x + STYLE_LEFT(subtle->styles.title);
                y   = subtle->styles.title.font->y +
                  STYLE_TOP(subtle->styles.title);

                subSharedDrawString(subtle->dpy, subtle->gcs.draw,
                  subtle->styles.title.font, drawable, x, y,
                  subtle->styles.title.fg, subtle->styles.title.bg,
                  buf, strlen(buf));

                subSharedDrawString(subtle->dpy, subtle->gcs.draw,
                  subtle->styles.title.font, drawable, x + width, y,
                  subtle->styles.title.fg, subtle->styles.title.bg, c->name,
                  /* Limit string length */
                  len > subtle->styles.clients.right ? 
                  subtle->styles.clients.right : len);
              }
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i, vx = p->x;
            SubStyle s = { -1, .flags = SUB_TYPE_STYLE, .border = { -1 },
              .padding = { -1 }, .margin = { -1 }};

            /* View buttons */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                int x = 0;
                SubView *v = VIEW(subtle->views->data[i]);

                /* Skip dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

                PanelViewStyle(v, i, (p->screen->viewid == i), &s);

                /* Set window background and border*/
                PanelRect(drawable, vx, v->width, &s);

                x += STYLE_LEFT((s));

                /* Draw view icon and/or text */
                if(v->flags & SUB_VIEW_ICON)
                  {
                    int y = 0, icony = 0;

                    y     = s.font->y + STYLE_TOP((s));
                    icony = v->icon->height > y ? s.margin.top :
                      y - v->icon->height;

                    subSharedDrawIcon(subtle->dpy, subtle->gcs.draw,
                      drawable, vx + x, icony, v->icon->width,
                      v->icon->height, s.icon, s.bg, v->icon->pixmap,
                      v->icon->bitmap);
                  }

                if(!(v->flags & SUB_VIEW_ICON_ONLY))
                  {
                    if(v->flags & SUB_VIEW_ICON) x += v->icon->width + 3;

                    subSharedDrawString(subtle->dpy, subtle->gcs.draw,
                      s.font, drawable, vx + x, s.font->y +
                      STYLE_TOP((s)), s.fg, s.bg, v->name, strlen(v->name));
                  }

                vx += v->width;

                /* Draw view separator if any */
                if(subtle->styles.viewsep && i < subtle->views->ndata - 1)
                  {
                    PanelSeparator(vx, subtle->styles.viewsep, drawable);

                    vx += subtle->styles.viewsep->separator->width;
                  }
              }
          }
        break; /* }}} */
    }

  /* Draw separator after panel */
  if(p->flags & SUB_PANEL_SEPARATOR2 && subtle->styles.separator.separator)
    {
      SubStyle *s = p->flags & SUB_PANEL_SUBLET && subtle->styles.subletsep ?
        subtle->styles.subletsep : &subtle->styles.separator;

      PanelSeparator(p->x + p->width, s, drawable);
    }

  subSubtleLogDebugSubtle("Render\n");
} /* }}} */

 /** subPanelCompare {{{
  * @brief Compare two panels
  * @param[in]  a  A #SubPanel
  * @param[in]  b  A #SubPanel
  * @return Returns the result of the comparison of both panels
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal
  * @retval  1   a is greater
  **/

int
subPanelCompare(const void *a,
  const void *b)
{
  SubPanel *p1 = *(SubPanel **)a, *p2 = *(SubPanel **)b;

  assert(a && b);

  /* Include only interval sublets */
  if(!(p1->sublet->flags & (SUB_SUBLET_INTERVAL))) return 1;
  if(!(p2->sublet->flags & (SUB_SUBLET_INTERVAL))) return -1;

  return p1->sublet->time < p2->sublet->time ? -1 :
    (p1->sublet->time == p2->sublet->time ? 0 : 1);
} /* }}} */

 /** subPanelAction {{{
  * @brief Handle panel action based on type
  * @param[in]  panels  A #SubArray
  * @param[in]  type    Action type
  * @param[in]  x       Pointer X position
  * @param[in]  y       Pointer Y position
  * @param[in]  button  Pointer button
  * @param[in]  bottom  Whether bottom panel or not
  **/

void
subPanelAction(SubArray *panels,
  int type,
  int x,
  int y,
  int button,
  int bottom)
{
  int i;

  /* FIXME: In order to find the correct panel item we
   * need to check all of them in a O(n) fashion and
   * check if they belong to the top or bottom panel.
   * Adding some kind of map for the x values would
   * be nice. */

  /* Check panel items */
  for(i = 0; i < panels->ndata; i++)
    {
      SubPanel *p = PANEL(panels->data[i]);

      /* Check if x is in panel rect */
      if(p->flags & type && x >= p->x && x <= p->x + p->width)
        {
          /* Check if action is for bottom panel */
          if((bottom && !(p->flags & SUB_PANEL_BOTTOM)) ||
            (!bottom && p->flags & SUB_PANEL_BOTTOM)) continue;

          /* Handle panel item type */
          switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS))
            {
              case SUB_PANEL_SUBLET: /* {{{ */
                /* Handle action type */
                switch(type)
                  {
                    case SUB_PANEL_OUT:
                      subRubyCall(SUB_CALL_OUT, p->sublet->instance, NULL);
                      break;
                    case SUB_PANEL_OVER:
                      subRubyCall(SUB_CALL_OVER, p->sublet->instance, NULL);
                      break;
                    case SUB_PANEL_DOWN:
                        {
                          int args[3] = { x - p->x, y, button };

                          subRubyCall(SUB_CALL_DOWN, p->sublet->instance,
                            (void *)&args);
                        }
                      break;
                  }

                subScreenUpdate();
                subScreenRender();
                break; /* }}} */
              case SUB_PANEL_VIEWS: /* {{{ */
                  {
                    int j, vx = p->x;

                    for(j = 0; j < subtle->views->ndata; j++)
                      {
                        SubView *v = VIEW(subtle->views->data[j]);

                        /* Skip dynamic views */
                        if(v->flags & SUB_VIEW_DYNAMIC &&
                            !(subtle->client_tags & v->tags))
                          continue;

                        /* Check if x is in view rect */
                        if(x >= vx && x <= vx + v->width)
                          {
                            /* FIXME */
                            int sid = subArrayIndex(subtle->screens,
                              (void *)p->screen);

                            subViewFocus(v, sid, True, False);

                            break;
                          }

                        /* Add view separator width if any */
                        if(subtle->styles.viewsep)
                          vx += v->width + subtle->styles.viewsep->separator->width;
                        else vx += v->width;
                      }
                  }
                break; /* }}} */
            }
        }
    }
} /* }}} */

 /** subPanelGeometry {{{
  * @brief Get geometry of panel for given style
  * @param[in]     p     A #SubPanel
  * @param[in]     s     A #SubStyle
  * @param[inout]  geom  A #XRectangle
  **/

void
subPanelGeometry(SubPanel *p,
  SubStyle *s,
  XRectangle *geom)
{
  assert(p && s && geom);

  /* Calculate panel geometry without style values */
  geom->x      = p->x + STYLE_LEFT((*s));
  geom->y      = p->flags & SUB_PANEL_BOTTOM ?
    p->screen->geom.y + p->screen->geom.height - subtle->ph : subtle->ph;
  geom->width  = 0 == p->width ? 1 : p->width - STYLE_WIDTH((*s));
  geom->height = subtle->ph - STYLE_HEIGHT((*s));
} /* }}} */

 /** subPanelPublish {{{
  * @brief Publish panels
  **/

void
subPanelPublish(void)
{
  int i = 0, j = 0, idx = 0;
  char **sublets = NULL, buf[30] = { 0 };
  XRectangle geom = { 0 };

  /* Alloc space */
  sublets = (char **)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(char *));

  /* We need to publish sublets here, because we cannot rely
   * on the sublets array which is reordered on every update */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(s->panels)
        {
          for(j = 0; j < s->panels->ndata; j++)
            {
              SubPanel *p = PANEL(s->panels->data[j]);

              /* Include sublets, exclude shallow copies */
              if(p->flags & SUB_PANEL_SUBLET && !(p->flags & SUB_PANEL_COPY))
                {
                  subPanelGeometry(p, PanelSubletStyle(p), &geom);

                  /* Add gravity to list */
                  snprintf(buf, sizeof(buf), "%dx%d+%d+%d#%s", geom.x,
                    geom.y, geom.width, geom.height, p->sublet->name);

                  sublets[idx] = (char *)subSharedMemoryAlloc(strlen(buf) + 1,
                    sizeof(char));
                  strncpy(sublets[idx++], buf, strlen(buf));
                }
            }
        }
    }

  /* EWMH: Sublet list and geometries */
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_SUBLET_LIST), sublets, subtle->sublets->ndata);

  /* Tidy up */
  for(i = 0; i < subtle->sublets->ndata; i++)
    free(sublets[i]);

  subSubtleLogDebugSubtle("Publish: sublets=%d\n", subtle->sublets->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(sublets);
} /* }}} */

 /** subPanelKill {{{
  * @brief Kill a panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelKill(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_COPY|SUB_PANEL_ICON|
      SUB_PANEL_KEYCHAIN|SUB_PANEL_SUBLET|SUB_PANEL_TRAY))
    {
      case SUB_PANEL_COPY: break;
      case SUB_PANEL_ICON: /* {{{ */
        if(p->icon) free(p->icon);
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        if(p->keychain)
          {
            if(p->keychain->keys) free(p->keychain->keys);
            free(p->keychain);
            p->keychain = NULL;
            p->screen   = NULL;
          }
        return; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        if(!(p->flags & SUB_PANEL_COPY))
          {
            /* Call unload */
            if(p->sublet->flags & SUB_SUBLET_UNLOAD)
              subRubyCall(SUB_CALL_UNLOAD, p->sublet->instance, NULL);

            subRubyRelease(p->sublet->instance);

            /* Remove socket watch */
            if(p->sublet->flags & SUB_SUBLET_SOCKET)
              {
                XDeleteContext(subtle->dpy, subtle->windows.support,
                  p->sublet->watch);
                subEventWatchDel(p->sublet->watch);
              }

#ifdef HAVE_SYS_INOTIFY_H
            /* Remove inotify watch */
            if(p->sublet->flags & SUB_SUBLET_INOTIFY)
              {
                XDeleteContext(subtle->dpy, subtle->windows.support,
                  p->sublet->watch);
                inotify_rm_watch(subtle->notify, p->sublet->interval);
              }
#endif /* HAVE_SYS_INOTIFY_H */

            if(p->sublet->name)
              {
                printf("Unloaded sublet (%s)\n", p->sublet->name);
                free(p->sublet->name);
              }
            if(p->sublet->text) subTextKill(p->sublet->text);

            free(p->sublet);
          }
        break; /* }}} */
      case SUB_PANEL_TRAY: /* {{{ */
        /* Reparent and return to avoid beeing destroyed */
        XReparentWindow(subtle->dpy, subtle->windows.tray, ROOT, 0, 0);
        p->screen = NULL;
        return; /* }}} */
    }

  free(p);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
