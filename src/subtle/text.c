
 /**
  * @package subtle
  *
  * @file Text functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/text.c,v 3205 2012/05/22 21:15:36 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

 /** subTextNew {{{
  * @brief Create new text
  **/

SubText *
subTextNew(void)
{
  return TEXT(subSharedMemoryAlloc(1, sizeof(SubText)));
} /* }}} */

 /** subTextParse {{{
  * @brief Parse a string and store it in given text
  * @param]inout]  t     A #SubText
  * @param[inout]  f     A #SubFont
  * @param[in]     text  Text to parse
  * @return Returns the length of the text in chars
  **/

int
subTextParse(SubText *t,
  SubFont *f,
  char *text)
{
  int i = 0, left = 0, right = 0;
  char *tok = NULL;
  long color = -1, pixmap = 0;
  SubTextItem *item = NULL;

  assert(f && t);

  t->width = 0;

  /* Split and iterate over tokens */
  while((tok = strsep(&text, SEPARATOR)))
    {
      if('#' == *tok) color = strtol(tok + 1, NULL, 0); ///< Color
      else if('\0' != *tok) ///< Text or icon
        {
          /* Re-use items to save alloc cycles */
          if(i < t->nitems && (item = ITEM(t->items[i])))
            {
              if(!(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) &&
                  item->data.string)
                free(item->data.string);

              item->flags &= ~(SUB_TEXT_EMPTY|SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP);
            }
          else if((item = ITEM(subSharedMemoryAlloc(1, sizeof(SubTextItem)))))
            {
              /* Add icon to array */
              t->items = (SubTextItem **)subSharedMemoryRealloc(t->items,
                (t->nitems + 1) * sizeof(SubTextItem *));
              t->items[(t->nitems)++] = item;
            }

          /* Get geometry of bitmap/pixmap */
          if(('!' == *tok || '&' == *tok) &&
              (pixmap = strtol(tok + 1, NULL, 0)))
            {
              XRectangle geometry = { 0 };

              subSharedPropertyGeometry(subtle->dpy, pixmap, &geometry);

              item->flags    |= ('!' == *tok ? SUB_TEXT_BITMAP :
                SUB_TEXT_PIXMAP);
              item->data.num  = pixmap;
              item->width     = geometry.width;
              item->height    = geometry.height;

              /* Add spacing and check if icon is first */
              t->width += item->width + (0 == i ? 3 : 6);

              item->color = color;
            }
          else ///< Ordinary text
            {
              item->data.string = strdup(tok);
              item->width       = subSharedStringWidth(subtle->dpy, f, tok,
                strlen(tok), &left, &right, False);

              /* Remove left bearing from first text item */
              t->width += item->width - (0 == i ? left : 0);

              item->color = color;
            }

          i++;
        }
    }

  /* Mark other items a clean */
  for(; i < t->nitems; i++)
    ITEM(t->items[i])->flags |= SUB_TEXT_EMPTY;

  /* Fix spacing of last item */
  if(item)
    {
      if(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP))
        t->width -= 2;
      else
        {
          t->width    -= right;
          item->width -= right;
        }
    }

  return t->width;
} /* }}} */

 /** subTextRender {{{
  * @brief Render text on window at given position
  * @param[inout]  t     A #SubText
  * @param[inout]  f     A #SubFont
  * @param[in]     gc    A #GC
  * @param[in]     win   Window to draw on
  * @param[in]     x     X position
  * @param]in]     y     Y position
  * @param[in]     fg    Foreground color
  * @param[in]     icon  Icon color
  * @param[in]     bg    Background color
  **/

void
subTextRender(SubText *t,
  SubFont *f,
  GC gc,
  Window win,
  int x,
  int y,
  long fg,
  long icon,
  long bg)
{
  int i, width = x;

  assert(t);

  /* Render text items */
  for(i = 0; i < t->nitems; i++)
    {
      SubTextItem *item = ITEM(t->items[i]);

      if(item->flags & SUB_TEXT_EMPTY) ///< Empty text
        {
          break; ///< Break loop
        }
      else if(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) ///< Icons
        {
          int icony = 0, dx = (0 == i) ? 0 : 3; ///< Add spacing when icon isn't first

          icony = item->height > f->height ?
            y - f->y - ((item->height - f->height) / 2): y - item->height;

          subSharedDrawIcon(subtle->dpy, gc, win, width + dx, icony,
            item->width, item->height, (-1 == item->color) ? icon : item->color,
            bg, (Pixmap)item->data.num, (item->flags & SUB_TEXT_BITMAP));

          /* Add spacing when icon isn't last */
          width += item->width + dx + (i != t->nitems - 1 ? 3 : 0);
        }
      else ///< Text
        {
          subSharedDrawString(subtle->dpy, gc, f, win, width, y,
            (-1 == item->color) ? fg : item->color, bg, 
            item->data.string, strlen(item->data.string));

          width += item->width;
        }
    }
} /* }}} */

 /** subTextKill {{{
  * @brief Delete text
  * @param[in]  t  A #SubText
  **/

void
subTextKill(SubText *t)
{
  int i;

  assert(t);

  for(i = 0; i < t->nitems; i++)
    {
      SubTextItem *item = (SubTextItem *)t->items[i];

      if(!(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) &&
          item->data.string)
        free(item->data.string);

      free(t->items[i]);
    }

  free(t->items);
  free(t);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
