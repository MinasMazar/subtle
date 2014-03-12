
 /**
  * @package subtle
  *
  * @file Hook functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/hook.c,v 3168 2012/01/03 16:02:50 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

 /** subHookNew {{{
  * @brief Create new hook
  * @param[in]  type  Type of hook
  * @param[in]  proc  Hook proc
  * @return Returns a new #SubHook or \p NULL
  **/

SubHook *
subHookNew(int type,
  unsigned long proc)
{
  SubHook *h = NULL;

  assert(proc);

  /* Create new hook */
  h = HOOK(subSharedMemoryAlloc(1, sizeof(SubHook)));
  h->flags = (SUB_TYPE_HOOK|type);
  h->proc  = proc;

  subSubtleLogDebugSubtle("new=hook, type=%d, proc=%ld\n", type, proc);

  return h;
} /* }}} */

 /** subHookCall {{{
  * @brief Emit a hook
  * @param[in]  type  Type of hook
  * @param[in]  data  Hook data
  **/

void
subHookCall(int type,
  void *data)
{
  int i;

  /* Call matching hooks */
  for(i = 0; i < subtle->hooks->ndata; i++)
    {
      SubHook *h = HOOK(subtle->hooks->data[i]);

      if((h->flags & ~SUB_TYPE_HOOK) == type)
        {
          subRubyCall(SUB_CALL_HOOKS, h->proc, data);

          subSubtleLogDebug("call=hook, type=%d, proc=%ld, data=%p\n",
            type, h->proc, data);
        }
    }
} /* }}} */

 /** subHookKill {{{
  * @brief Kill a hook
  * @param[in]  h  A #SubHook
  **/

void
subHookKill(SubHook *h)
{
  assert(h);

  free(h);

  subSubtleLogDebugSubtle("kill=hook\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
