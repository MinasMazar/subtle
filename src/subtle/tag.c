
 /**
  * @package subtle
  *
  * @file Tag functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/tag.c,v 3209 2012/05/22 23:44:10 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

#define MATCHER(m) ((TagMatcher *)m)

/* Typedef {{{ */
typedef struct tagmatcher_t
{
  FLAGS               flags;
  struct tagmatcher_t *and;
  regex_t             *regex;
} TagMatcher;
/* }}} */

/* Private */

/* TagClear {{{ */
static void
TagClear(SubTag *t)
{
  int i;

  assert(t);

  /* Clear matcher */
  for(i = 0; t->matcher &&  i < t->matcher->ndata; i++)
    {
      TagMatcher *m = (TagMatcher *)t->matcher->data[i];

      if(m->regex) subSharedRegexKill(m->regex);

      free(m);
    }

  subArrayClear(t->matcher, False);
} /* }}} */

/* TagFind {{{ */
static SubTag *
TagFind(char *name)
{
  int i;
  SubTag *t = NULL;

  assert(name);

  /* Linear search */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      t = TAG(subtle->tags->data[i]);

      if(0 == strcmp(t->name, name)) return t;
    }

  return NULL;
} /* }}} */

/* TagMatch {{{ */
static int
TagMatch(TagMatcher *m,
  SubClient *c)
{
  /* Complex matching */
  if((m->regex &&
      /* Check WM_NAME */
      ((m->flags & SUB_TAG_MATCH_NAME && c->name &&
        subSharedRegexMatch(m->regex, c->name)) ||
      /* Check instance part of WM_CLASS */
      (m->flags & SUB_TAG_MATCH_INSTANCE && c->instance &&
        subSharedRegexMatch(m->regex, c->instance)) ||
      /* Check class part of WM_CLASS */
      (m->flags & SUB_TAG_MATCH_CLASS && c->klass &&
        subSharedRegexMatch(m->regex, c->klass)) ||
      /* Check WM_ROLE */
      (m->flags & SUB_TAG_MATCH_ROLE && c->role &&
        subSharedRegexMatch(m->regex, c->role)))) ||
      /* Check _NET_WM_WINDOW_TYPE */
      (m->flags & SUB_TAG_MATCH_TYPE &&
        c->flags & (m->flags & (SUB_CLIENT_TYPE_NORMAL|TYPES_ALL))))
    return True;

  return False;
} /* }}} */

/* Public */

 /** subTagNew {{{
  * @brief Create new tag
  * @param[in]   name       Name of the tag
  * @param[out]  duplicate  Added twice
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagNew(char *name,
  int *duplicate)
{
  SubTag *t = NULL;

  assert(name);

  /* Check if tag already exists */
  if(duplicate && (t = TagFind(name)))
    {
      *duplicate = True;
    }
  else
    {
      /* Create new tag */
      t = TAG(subSharedMemoryAlloc(1, sizeof(SubTag)));
      t->flags = SUB_TYPE_TAG;
      t->name  = strdup(name);
      if(duplicate) *duplicate = False;
    }

  subSubtleLogDebugSubtle("New: name=%s\n", name);

  return t;
} /* }}} */

 /** subTagMatcherAdd {{{
  * @brief Add a matcher to a tag
  * @param[in]  t        A #SubTag
  * @param[in]  type     Matcher type
  * @param[in]  pattern  Regex
  * @param[in]  and      Logical AND with last matcher
  **/

void
subTagMatcherAdd(SubTag *t,
  int type,
  char *pattern,
  int and)
{
  TagMatcher *m = NULL;
  regex_t *regex = NULL;

  assert(t);

  /* Prevent emtpy regex */
  if(pattern && 0 != strlen(pattern))
    regex = subSharedRegexNew(pattern);

  /* Remove matcher types that need a regexp */
  if(!regex)
    type &= ~(SUB_TAG_MATCH_NAME|SUB_TAG_MATCH_INSTANCE|
      SUB_TAG_MATCH_CLASS|SUB_TAG_MATCH_ROLE);

  /* Check if anything is left for matching */
  if(0 < type)
    {
      /* Create new matcher */
      m = MATCHER(subSharedMemoryAlloc(1, sizeof(TagMatcher)));
      m->flags = type;
      m->regex = regex;

      /* Create on demand to safe memory */
      if(NULL == t->matcher) t->matcher = subArrayNew();
      else if(and && 0 < t->matcher->ndata)
        {
          /* Link matcher */
          MATCHER(t->matcher->data[t->matcher->ndata - 1])->and = m;

          m->flags |= SUB_TAG_MATCH_AND;
        }

      subArrayPush(t->matcher, (void *)m);
    }
} /* }}} */

 /** subTagMatcherCheck {{{
  * @brief Check if client matches tag
  * @param[in]  t  A #SubTag
  * @param[in]  c  A #SubClient
  * @retval  True  Client matches
  * @retval  False Client doesn't match
  **/

int
subTagMatcherCheck(SubTag *t,
  SubClient *c)
{
  int i;

  assert(t && c);

  /* Check if a matcher and client fit together */
  for(i = 0; t->matcher && i < t->matcher->ndata; i++)
    {
      TagMatcher *m = MATCHER(t->matcher->data[i]);

      /* Exclude AND linked matcher */
      if(!(m->flags & SUB_TAG_MATCH_AND))
        {
          int and = True;
          TagMatcher *cur = m;

          /* Check current matcher and chain */
          while(and && cur)
            {
              and = TagMatch(cur, c);
              cur = cur->and;
            }

          if(and) return True;
        }
    }

  return False;
} /* }}} */

 /** subTagKill {{{
  * @brief Delete tag
  * @param[in]  t  A #SubTag
  **/

void
subTagKill(SubTag *t)
{
  assert(t);

  /* Hook: Kill */
  subHookCall((SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_KILL),
    (void *)t);

  /* Remove matcher */
  if(t->matcher)
    {
      TagClear(t);
      subArrayKill(t->matcher, False);
    }

  /* Remove proc */
  if(t->flags & SUB_TAG_PROC)
    subRubyRelease(t->proc);

  free(t->name);
  free(t);

  subSubtleLogDebugSubtle("Kill\n");
} /* }}} */

/* All */

 /** subTagPublish {{{
  * @brief Publish tags
  **/

void
subTagPublish(void)
{
  int i;
  char **names = NULL;

  assert(0 < subtle->tags->ndata);

  names = (char **)subSharedMemoryAlloc(subtle->tags->ndata, sizeof(char *));

  for(i = 0; i < subtle->tags->ndata; i++)
    names[i] = TAG(subtle->tags->data[i])->name;

  /* EWMH: Tag list */
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_TAG_LIST), names, i);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(names);

  subSubtleLogDebugSubtle("Publish: tags=%d\n", i);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
