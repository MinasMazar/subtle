
 /**
  * @package subtle
  *
  * @file Array functions
  * @copyright 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/array.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

 /** subArrayNew {{{
  * @brief Create new array and init it
  * @return Returns a #SubArray or \p NULL
  **/

SubArray *
subArrayNew(void)
{
  return (SubArray *)subSharedMemoryAlloc(1, sizeof(SubArray));
} /* }}} */

 /** subArrayPush {{{
  * @brief Push element to array
  * @param[in]  a     A #SubArray
  * @param[in]  elem  New element
  **/

void
subArrayPush(SubArray *a,
  void *elem)
{
  assert(a);

  if(elem)
    {
      a->data = (void **)subSharedMemoryRealloc(a->data, (a->ndata + 1) * sizeof(void *));
      a->data[(a->ndata)++] = elem;
    }
} /* }}} */

/** subArrayInsert {{{
  * @brief Insert element at position
  * @param[in]  a     A #SubArray
  * @param[in]  pos   Position
  * @param[in]  elem  Array element
  **/

void
subArrayInsert(SubArray *a,
  int pos,
  void *elem)
{
  int i;

  assert(a && elem);

  /* Check boundaries */
  if(pos < a->ndata)
    {
      a->ndata++;
      a->data = (void **)subSharedMemoryRealloc(a->data, a->ndata * sizeof(void *));

      for(i = a->ndata - 1; i > pos; i--)
        a->data[i] = a->data[i - 1];

      a->data[pos] = elem;
    }
  else subArrayPush(a, elem);
} /* }}} */

 /** subArrayRemove {{{
  * @brief Remove element from array
  * @param[in]  a     A #SubArray
  * @param[in]  elem  Array element
  **/

void
subArrayRemove(SubArray *a,
  void *elem)
{
  int i, idx;

  assert(a && elem);

  if(0 <= (idx = subArrayIndex(a, elem)))
    {
      for(i = idx; i < a->ndata - 1; i++)
        a->data[i] = a->data[i + 1];

      a->ndata--;
      a->data = (void **)subSharedMemoryRealloc(a->data, a->ndata * sizeof(void *));
    }
} /* }}} */

 /** subArrayGet {{{
  * @brief Get id after boundary check
  * @param[in]  a    A #SubArray
  * @param[in]  id   Array index
  * @return Returns an element or \p NULL
  **/

void *
subArrayGet(SubArray *a,
  int id)
{
  assert(a);

  return 0 <= id && id < a->ndata ? a->data[id] : NULL;
} /* }}} */

 /** subArrayIndex {{{
  * @brief Find array id of element
  * @param[in]  a     A #SubArray
  * @param[in]  elem  Element
  * @return Returns found idx or \p -1
  **/

int
subArrayIndex(SubArray *a,
  void *elem)
{
  int i;

  assert(a && elem);

  for(i = 0; i < a->ndata; i++)
    if(a->data[i] == elem) return i;

  return -1;
} /* }}} */

  /** subArraySort {{{
   * @brief Sort array with given compare function
   * @param[in]  a       A #SubArray
   * @param[in]  compar  Compare function
   **/

void
subArraySort(SubArray *a,
  int(*compar)(const void *a, const void *b))
{
  assert(a && compar);

  if(0 < a->ndata) qsort(a->data, a->ndata, sizeof(void *), compar);
} /* }}} */

 /** subArrayClear {{{
  * @brief Delete all elements
  * @param[in]  a      A #SubArray
  * @param[in]  clean  Clean elements
  **/

void
subArrayClear(SubArray *a,
  int clean)
{
  if(a)
    {
      int i;
      SubClient *c = NULL;

      for(i = 0; clean && i < a->ndata; i++)
        {
          /* Check type and kill */
          if((c = CLIENT(a->data[i])))
            {
              if(c->flags & SUB_TYPE_CLIENT)       subClientKill(c);
              else if(c->flags & SUB_TYPE_GRAB)    subGrabKill(GRAB(c));
              else if(c->flags & SUB_TYPE_GRAVITY) subGravityKill(GRAVITY(c));
              else if(c->flags & SUB_TYPE_HOOK)    subHookKill(HOOK(c));
              else if(c->flags & SUB_TYPE_SCREEN)  subScreenKill(SCREEN(c));
              else if(c->flags & SUB_TYPE_STYLE)   subStyleKill(STYLE(c));
              else if(c->flags & SUB_TYPE_TAG)     subTagKill(TAG(c));
              else if(c->flags & SUB_TYPE_TRAY)    subTrayKill(TRAY(c));
              else if(c->flags & SUB_TYPE_VIEW)    subViewKill(VIEW(c));
              else if(c->flags & SUB_TYPE_PANEL)   subPanelKill(PANEL(c));
            }
        }

      if(a->data) free(a->data);

      a->data  = NULL;
      a->ndata = 0;
    }
} /* }}} */

 /** subArrayKill {{{
  * @brief Kill array with all elements
  * @param[in]  a      A #SubArray
  * @param[in]  clean  Clean to elements
  **/

void
subArrayKill(SubArray *a,
  int clean)
{
  if(a)
    {
      subArrayClear(a, clean);

      free(a);
      a = NULL;
    }
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
