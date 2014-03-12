
 /**
  * @package subtle
  *
  * @file Gravity functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/gravity.c,v 3213 2012/05/31 13:42:04 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <X11/Xresource.h>
#include "subtle.h"

/* Method */

 /** subGravityNew {{{
  * @brief Create new gravity
  * @param[in]  name  Gravity name
  * @param[in]  geom  Geometry of gravity
  * @return Returns a new #SubGravity or \p NULL
  **/

SubGravity *
subGravityNew(const char *name,
  XRectangle *geom)
{
  SubGravity *g = NULL;

  assert(name && geom);

  /* Create gravity */
  g = GRAVITY(subSharedMemoryAlloc(1, sizeof(SubGravity)));
  g->flags |= SUB_TYPE_GRAVITY;

  if(name) g->quark = XrmStringToQuark(name); ///< Create hash

  /* Sanitize values */
  g->geom.x      = MINMAX(geom->x,      0, 100);
  g->geom.y      = MINMAX(geom->y,      0, 100);
  g->geom.width  = MINMAX(geom->width,  1, 100);
  g->geom.height = MINMAX(geom->height, 1, 100);

  subSubtleLogDebugSubtle("New: name=%s, quark=%d, x=%d, y=%d,"
    "width=%d, height=%d\n",
    name, g->quark, geom->x, geom->y, geom->width, geom->height);

  return g;
} /* }}} */

 /** subGravityGeometry {{{
  * @brief Calculate geometry of gravity for bounds
  * @param[in]   g       A #SubGravity
  * @param[in]   bounds  A #XRectangle
  * @param[out]  geom    A #XRectangle
  **/

void
subGravityGeometry(SubGravity *g,
  XRectangle *bounds,
  XRectangle *geom)
{
  assert(g && bounds && geom);

  /* Calculate gravity size for bounds */
  geom->x      = bounds->x + (bounds->width * g->geom.x / 100);
  geom->y      = bounds->y + (bounds->height * g->geom.y / 100);
  geom->width  = (bounds->width * g->geom.width / 100);
  geom->height = (bounds->height * g->geom.height / 100);
} /* }}} */

 /** subGravityKill {{{
  * @brief Kill gravity
  * @param[in]  g  A #SubGravity
  **/

void
subGravityKill(SubGravity *g)
{
  assert(g);

  free(g);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subGravityFind {{{
  * @brief Find gravity
  * @param[in]   name   Gravity name
  * @param[in]   quark  Quark
  * @retval  -1  Not found
  * @retval  >0  Found id
  **/

int
subGravityFind(const char *name,
  int quark)
{
  int found = -1;

  if(0 < subtle->gravities->ndata)
    {
      int i, hash = 0;

      /* Get quark */
      if(name) hash = XrmStringToQuark(name);
      else hash = quark;

      for(i = 0; i < subtle->gravities->ndata; i++)
        {
          SubGravity *g = GRAVITY(subtle->gravities->data[i]);

          /* Compare quarks */
          if(g->quark == hash)
            {
              found = i;
              break;
            }
        }
    }

  return found;
} /* }}} */

 /** subGravityPublish {{{
  * @brief Publish gravities
  **/

void
subGravityPublish(void)
{
  int i;
  char **gravities = NULL, buf[30] = { 0 };
  SubGravity *g = NULL;

  assert(0 < subtle->gravities->ndata);

  /* Alloc space */
  gravities  = (char **)subSharedMemoryAlloc(subtle->gravities->ndata,
    sizeof(char *));

  for(i = 0; i < subtle->gravities->ndata; i++)
    {
      g = GRAVITY(subtle->gravities->data[i]);

      /* Add gravity to list */
      snprintf(buf, sizeof(buf), "%dx%d+%d+%d#%s", g->geom.x,
        g->geom.y, g->geom.width, g->geom.height,
        XrmQuarkToString(g->quark));

      gravities[i] = (char *)subSharedMemoryAlloc(strlen(buf) + 1,
        sizeof(char));
      strncpy(gravities[i], buf, strlen(buf));
    }

  /* EWMH: Gravity list and geometries */
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_GRAVITY_LIST), gravities,
    subtle->gravities->ndata);

  /* Tidy up */
  for(i = 0; i < subtle->gravities->ndata; i++)
    free(gravities[i]);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(gravities);

  subSubtleLogDebugSubtle("Publish: gravities=%d\n", subtle->gravities->ndata);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
