 /**
  * @package subtle
  *
  * @file Style functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/style.c,v 3205 2012/05/22 21:15:36 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

/* StyleInheritSides {{{ */
static void
StyleInheritSides(SubSides *s1,
  SubSides *s2,
  int merge)
{
  if(-1 == s1->top    || (merge && -1 != s2->top))    s1->top    = s2->top;
  if(-1 == s1->right  || (merge && -1 != s2->right))  s1->right  = s2->right;
  if(-1 == s1->bottom || (merge && -1 != s2->bottom)) s1->bottom = s2->bottom;
  if(-1 == s1->left   || (merge && -1 != s2->left))   s1->left   = s2->left;
} /* }}} */

/* StyleInherit {{{ */
static void
StyleInherit(SubStyle *s1,
  SubStyle *s2)
{
  assert(s1 && s2);

  /* Inherit unset colors */
  if(-1 == s1->fg)     s1->fg     = s2->fg;
  if(-1 == s1->bg)     s1->bg     = s2->bg;
  if(-1 == s1->icon)   s1->icon   = s2->icon;
  if(-1 == s1->top)    s1->top    = s2->top;
  if(-1 == s1->right)  s1->right  = s2->right;
  if(-1 == s1->bottom) s1->bottom = s2->bottom;
  if(-1 == s1->left)   s1->left   = s2->left;

  /* Inherit unset border, padding and margin */
  StyleInheritSides(&s1->border,  &s2->border,  False);
  StyleInheritSides(&s1->padding, &s2->padding, False);
  StyleInheritSides(&s1->margin,  &s2->margin,  False);

  /* Inherit font */
  if(NULL == s1->font) s1->font = s2->font;

  if(s1->font)
    {
      /* Check max height of style */
      if(s1 != &subtle->styles.clients || s1 != &subtle->styles.subtle)
        {
          int height = STYLE_HEIGHT((*s1)) + s1->font->height;

          if(height > subtle->ph) subtle->ph = height;
        }

      /* Update separator width after font */
      if(s1->separator)
        {
          s1->separator->width = subSharedStringWidth(subtle->dpy,
            s1->font, s1->separator->string,
            strlen(s1->separator->string), NULL, NULL, True);

          /* Add style width to calculate it only once */
          if(0 < s1->separator->width)
            s1->separator->width += STYLE_WIDTH((*s1));
        }
    }

  /* Check nested styles */
  if(s1->styles)
    {
      int i;

      /* Inherit unset values from parent style */
      for(i = 0; i < s1->styles->ndata; i++)
        {
          SubStyle *style = STYLE(s1->styles->data[i]);

          /* Don't inherit values, these styles are just
           * added to the selected view styles */
          if(subtle->styles.urgent == style ||
            subtle->styles.visible == style) continue;

          StyleInherit(style, s1);

          /* Sanitize icon */
          if(-1 == style->icon) style->icon = style->fg;
        }
    }
} /* }}} */

/* StyleFont {{{ */
static void
StyleFont(SubStyle *s,
  const char *name)
{
  /* Check if style exists and font is defined */
  if(s && !s->font)
    {
      subSubtleLogError("Cannot find a font definition for style `%s'\n", name); \
      subSubtleFinish();
      exit(-1); \
    }
} /* }}} */

/* Public */

 /** subStyleNew {{{
  * @brief Create new style
  * @return Returns a new #SubStyle or \p NULL
  **/

SubStyle *
subStyleNew(void)
{
  SubStyle *s = NULL;

  /* Create new style */
  s = STYLE(subSharedMemoryAlloc(1, sizeof(SubStyle)));
  s->flags |= SUB_TYPE_STYLE;

  /* Init style values */
  subStyleReset(s, -1);

  subSubtleLogDebugSubtle("New\n");

  return s;
} /* }}} */

 /** subStyleFind {{{
  * @brief Find style
  * @param[in]    s      A #SubStyle
  * @param[in]    name   Name of style state
  * @param[inout] idx    Index of found state
  * @return Returns found #SubStyle or \p NULL
  **/

SubStyle *
subStyleFind(SubStyle *s,
  char *name,
  int *idx)
{
  SubStyle *found = NULL;

  assert(s);

  if(s->styles && name)
    {
      int i;

      /* Check each state */
      for(i = 0; i < s->styles->ndata; i++)
        {
          SubStyle *style = STYLE(s->styles->data[i]);

          /* Compare state name */
          if(0 == strcmp(name, style->name))
            {
              found = style;
              if(idx) *idx = i;

              break;
            }
        }
    }

  subSubtleLogDebugSubtle("Find\n");

  return found;
} /* }}} */

 /** subStyleReset {{{
  * Reset style values
  * @param[in]  s  A #SubStyle
  **/

void
subStyleReset(SubStyle *s,
  int val)
{
  assert(s);

  /* Set value */
  s->fg = s->bg = s->top = s->right = s->bottom = s->left = val;
  s->border.top  = s->border.right  = s->border.bottom  = s->border.left  = val;
  s->padding.top = s->padding.right = s->padding.bottom = s->padding.left = val;
  s->margin.top  = s->margin.right  = s->margin.bottom  = s->margin.left  = val;

  /* Force value to prevent inheriting of 0 value from all */
  s->icon = -1;

  /* Reset font */
  if(s->flags & SUB_STYLE_FONT && s->font)
    {
      subSharedFontKill(subtle->dpy, s->font);
      s->flags &= ~SUB_STYLE_FONT;
    }

  s->font = NULL;

  /* Reset separator */
  if(s->flags & SUB_STYLE_SEPARATOR && s->separator)
    {
      free(s->separator->string);
      free(s->separator);
    }

  s->separator = NULL;

  /* Remove states */
  if(s->styles) subArrayKill(s->styles, True);
  s->styles = NULL;

  subSubtleLogDebugSubtle("Reset\n");
} /* }}} */

 /** subStyleMerge {{{
  * @brief Merge styles
  * @param[inout]  s1  Style to assign values to
  * @param[in]     s2  Style to copy values from
  **/

void
subStyleMerge(SubStyle *s1,
  SubStyle *s2)
{
  assert(s1 && s2);

  /* Merge set colors */
  if(-1 != s2->fg)     s1->fg     = s2->fg;
  if(-1 != s2->bg)     s1->bg     = s2->bg;
  if(-1 != s2->icon)   s1->icon   = s2->icon;
  if(-1 != s2->top)    s1->top    = s2->top;
  if(-1 != s2->right)  s1->right  = s2->right;
  if(-1 != s2->bottom) s1->bottom = s2->bottom;
  if(-1 != s2->left)   s1->left   = s2->left;

  /* Merge font */
  if(NULL != s2->font) s1->font = s2->font;

  /* Merge set border, padding and margin */
  StyleInheritSides(&s1->border,  &s2->border,  True);
  StyleInheritSides(&s1->padding, &s2->padding, True);
  StyleInheritSides(&s1->margin,  &s2->margin,  True);

  subSubtleLogDebugSubtle("Merge\n");
} /* }}} */

 /** subStyleKill {{{
  * @brief Kill a style
  * @param[in]  s  A #SubStyle
  **/

void
subStyleKill(SubStyle *s)
{
  assert(s);

  /* Free font */
  if(s->flags & SUB_STYLE_FONT && s->font)
    subSharedFontKill(subtle->dpy, s->font);

  /* Free separator */
  if(s->flags & SUB_STYLE_SEPARATOR && s->separator)
    {
      free(s->separator->string);
      free(s->separator);
    }

  if(s->name)   free(s->name);
  if(s->styles) subArrayKill(s->styles, True);
  free(s);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subStyleUpdate {{{
  * Inherit style values from all
  **/

void
subStyleUpdate(void)
{
  /* Inherit styles */
  StyleInherit(&subtle->styles.views,     &subtle->styles.all);
  StyleInherit(&subtle->styles.title,     &subtle->styles.all);
  StyleInherit(&subtle->styles.sublets,   &subtle->styles.all);
  StyleInherit(&subtle->styles.separator, &subtle->styles.all);

  /* Check font */
  StyleFont(&(subtle->styles.title),     "title");
  StyleFont(&(subtle->styles.separator), "separator");
  StyleFont(&(subtle->styles.sublets),   "sublets");
  StyleFont(subtle->styles.occupied,     "occupied");
  StyleFont(subtle->styles.focus,        "focus");
  StyleFont(subtle->styles.viewsep,      "view separator");
  StyleFont(subtle->styles.subletsep,    "sublet separator");

  subSubtleLogDebugSubtle("Update\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
