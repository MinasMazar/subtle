
 /**
  * @package subtle
  *
  * @file View functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/view.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

 /** subViewNew {{{
  * @brief Create a new view
  * @param[in]  name  Name of the view
  * @param[in]  tags  Tags for the view
  * @return Returns a #SubView or \p NULL
  **/

SubView *
subViewNew(char *name,
  char *tags)
{
  SubView *v = NULL;

  assert(name);

  /* Create new view */
  v = VIEW(subSharedMemoryAlloc(1, sizeof(SubView)));
  v->flags = SUB_TYPE_VIEW;
  v->styleid = -1;
  v->name  = strdup(name);

  /* Tags */
  if(tags && strncmp("", tags, 1))
    {
      int i;
      regex_t *preg = subSharedRegexNew(tags);

      for(i = 0; i < subtle->tags->ndata; i++)
        if(subSharedRegexMatch(preg, TAG(subtle->tags->data[i])->name))
          v->tags |= (1L << (i + 1));

      subSharedRegexKill(preg);
    }

  subSubtleLogDebugSubtle("New: name=%s\n", name);

  return v;
} /* }}} */

 /** subViewFocus {{{
  * @brief Jump to view on screen or swap both
  * @param[in]  v         A #SubView
  * @param[in]  screenid  Screen id
  * @param[in]  swap      Whether to swap views
  * @param[in]  focus     Whether to focus next client
  **/

void
subViewFocus(SubView *v,
  int screenid,
  int swap,
  int focus)
{
  int vid = 0;
  SubScreen *s1 = NULL;
  SubClient *c = NULL;

  assert(v);

  /* Select screen and find vid */
  s1  = SCREEN(subArrayGet(subtle->screens, screenid));
  vid = subArrayIndex(subtle->views, (void *)v);

  /* Swap only makes sense with more than one screen */
  if(swap && 1 < subtle->screens->ndata)
    {
      /* Check if view is visible on any screen */
      if(subtle->visible_views & (1L << (vid + 1)))
        {
          int i;

          /* Find screen with view and swap */
          for(i = 0; i < subtle->screens->ndata; i++)
            {
              SubScreen *s2 = SCREEN(subtle->screens->data[i]);

              if(s2->viewid == vid)
                {
                  s2->viewid = s1->viewid;

                  break;
                }
            }
        }
    }

  /* Set view and configure */
  s1->viewid = vid;

  subScreenConfigure();
  subScreenRender();
  subScreenPublish();

  /* Update focus */
  if(focus)
    {
      /* Restore focus on view */
      if(!((c = CLIENT(subSubtleFind(v->focus, CLIENTID))) &&
          VISIBLETAGS(c, v->tags)))
        {
          c        = subClientNext(screenid, False);
          v->focus = None;
        }

      if(c) subClientFocus(c, True);
    }

  /* Hook: Focus */
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS), (void *)v);

  subSubtleLogDebugSubtle("Focus: focus=%d\n", focus);
} /* }}} */

 /** SubViewKill {{{
  * @brief Kill a view
  * @param[in]  v  A #SubView
  **/

void
subViewKill(SubView *v)
{
  assert(v);

  /* Hook: Kill */
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_KILL),
    (void *)v);

  if(v->icon) free(v->icon);
  free(v->name);
  free(v);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subViewPublish {{{
  * @brief Update EWMH infos
  **/

void
subViewPublish(void)
{
  int i;
  long vid = 0, *tags = NULL, *icons = NULL;
  char **names = NULL;

  if(0 < subtle->views->ndata)
    {
      tags  = (long *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(long));
      icons = (long *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(long));
      names = (char **)subSharedMemoryAlloc(subtle->views->ndata, sizeof(char *));

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          tags[i]  = v->tags;
          icons[i] = v->icon ? v->icon->pixmap : -1;
          names[i] = v->name;
        }

      /* EWMH: Tags */
      subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VIEW_TAGS,
        tags, subtle->views->ndata);

      /* EWMH: Icons */
      subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VIEW_ICONS,
        icons, subtle->views->ndata);

      /* EWMH: Desktops */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
      subSharedPropertySetStrings(subtle->dpy, ROOT,
        subEwmhGet(SUB_EWMH_NET_DESKTOP_NAMES), names, subtle->views->ndata);

      /* EWMH: Current desktop */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

      XSync(subtle->dpy, False); ///< Sync all changes

      free(tags);
      free(icons);
      free(names);
    }

  subSubtleLogDebugSubtle("Publish: views=%d\n", subtle->views->ndata);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
