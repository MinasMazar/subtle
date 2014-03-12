
 /**
  * @package subtle
  *
  * @file Main functions
  * @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtle/subtle.c,v 3190 2012/03/16 22:54:45 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <stdarg.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include "subtle.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

SubSubtle *subtle = NULL;

/* SubtleSignal {{{ */
static void
SubtleSignal(int signum)
{
  switch(signum)
    {
      case SIGCHLD: wait(NULL);                                    break;
      case SIGHUP:  if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD; break;
      case SIGINT:  if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;   break;
      case SIGSEGV:
          {
#ifdef HAVE_EXECINFO_H
            int i, frames = 0;
            void *callstack[10] = { 0 };
            char **strings = NULL;

            frames  = backtrace(callstack, 10);
            strings = backtrace_symbols(callstack, frames);

            printf("\n\nLast %d stack frames:\n", frames);

            for(i = 0; i < frames; i++)
              printf("%s\n", strings[i]);

            free(strings);
#endif /* HAVE_EXECINFO_H */

            printf("\nPlease report this bug at %s\n", PKG_BUGREPORT);
            abort();
          }
        break;
    }
} /* }}} */

/* SubtleUsage {{{ */
static void
SubtleUsage(void)
{
  printf("Usage: %s [OPTIONS]\n\n" \
         "Options:\n" \
         "  -c, --config=FILE          Load config\n" \
         "  -d, --display=DISPLAY      Connect to DISPLAY\n" \
         "  -h, --help                 Show this help and exit\n" \
         "  -k, --check                Check config syntax\n" \
         "  -n, --no-randr             Disable RandR extension (required for Twinview)\n" \
         "  -r, --replace              Replace current window manager\n" \
         "  -s, --sublets=DIR          Load sublets from DIR\n" \
         "  -v, --version              Show version info and exit\n" \
         "  -l, --level=LEVEL[,LEVEL]  Set logging levels\n" \
         "  -D, --debug                Print debugging messages\n" \
         "\nPlease report bugs at %s\n",
         PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtleVersion {{{ */
static void
SubtleVersion(void)
{
  printf("%s %s - Copyright (c) 2005-2012 Christoph Kappel\n" \
         "Released under the GNU General Public License\n" \
         "Compiled for X%dR%d and Ruby %s\n",
         PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION, RUBY_VERSION);
} /* }}} */

#ifdef DEBUG
/* SubtleLevel {{{ */
static int
SubtleLevel(const char *str)
{
  int level = 0;
  char *tokens = NULL, *tok = NULL;

  tokens = strdup(str);
  tok    = strtok((char *)tokens, ",");

  /* Parse levels */
  while(tok)
    {
      if(0 == strncasecmp(tok, "warnings", 8))
        level |= SUB_LOG_WARN;
      else if(0 == strncasecmp(tok, "error", 5))
        level |= SUB_LOG_ERROR;
      else if(0 == strncasecmp(tok, "sublet", 6))
        level |= SUB_LOG_SUBLET;
      else if(0 == strncasecmp(tok, "depcrecated", 11))
        level |= SUB_LOG_DEPRECATED;
      else if(0 == strncasecmp(tok, "events", 6))
        level |= SUB_LOG_EVENTS;
      else if(0 == strncasecmp(tok, "ruby", 4))
        level |= SUB_LOG_RUBY;
      else if(0 == strncasecmp(tok, "xerror", 6))
        level |= SUB_LOG_XERROR;
      else if(0 == strncasecmp(tok, "subtle", 6))
        level |= SUB_LOG_SUBTLE;
      else if(0 == strncasecmp(tok, "debug", 4))
        level |= SUB_LOG_DEBUG;

      tok = strtok(NULL, ",");
    }

  free(tokens);

  return level;
} /* }}} */
#endif /* DEBUG */

/* Public */

 /** subSubtleFind {{{
  * @brief Find data with the context manager
  * @param[in]  win  A #Window
  * @param[in]  id   Context id
  * @return Returns found data pointer or \p NULL
  **/

XPointer *
subSubtleFind(Window win,
  XContext id)
{
  XPointer *data = NULL;

  return XCNOENT != XFindContext(subtle->dpy, win, id, (XPointer *)&data) ? data : NULL;
} /* }}} */

 /** subSubtleTime {{{
  * @brief Get the current time in seconds
  * @return Returns time in seconds
  **/

time_t
subSubtleTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return tv.tv_sec;
} /* }}} */

 /** subSubtleLog {{{
  * @brief Print messages depending on type
  * @param[in]  level   Message level
  * @param[in]  file    File name
  * @param[in]  line    Line number
  * @param[in]  format  Message format
  * @param[in]  ...     Variadic arguments
  **/

void
subSubtleLog(int level,
  const char *file,
  int line,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];

#ifdef DEBUG
  if(!(subtle->loglevel & level)) return;
#endif /* DEBUG */

  /* Get variadic arguments */
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  /* Print according to loglevel */
  if(level & SUB_LOG_WARN)
    fprintf(stdout, "<WARNING> %s", buf);
  else if(level & SUB_LOG_ERROR)
    fprintf(stderr, "<ERROR> %s", buf);
  else if(level & SUB_LOG_SUBLET)
    fprintf(stderr, "<WARNING SUBLET %s> %s", file, buf);
  else if(level & SUB_LOG_DEPRECATED)
    fprintf(stdout, "<DEPRECATED> %s", buf);
#ifdef DEBUG
  else if(level & SUB_LOG_EVENTS)
    fprintf(stderr, "<EVENTS> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_RUBY)
    fprintf(stderr, "<RUBY> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_XERROR)
    fprintf(stderr, "<XERROR> %s", buf);
  else if(level & SUB_LOG_SUBTLE)
    fprintf(stderr, "<SUBTLE> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_DEBUG)
    fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf);
#endif /* DEBUG */
} /* }}} */

 /** subSubtleFinish {{{
  * @brief Finish subtle
  **/

void
subSubtleFinish(void)
{
  if(subtle)
    {
      if(subtle->dpy)
        XSync(subtle->dpy, False); ///< Sync before going on

      /* Handle hooks first */
      if(subtle->hooks)
        {
          /* Hook: Exit */
          subHookCall(SUB_HOOK_EXIT, NULL);

          /* Clear hooks first to stop calling */
          subArrayClear(subtle->hooks, True);
        }

      /* Kill arrays */
      if(subtle->clients)   subArrayKill(subtle->clients,   True);
      if(subtle->grabs)     subArrayKill(subtle->grabs,     True);
      if(subtle->gravities) subArrayKill(subtle->gravities, True);
      if(subtle->screens)   subArrayKill(subtle->screens,   True);
      if(subtle->sublets)   subArrayKill(subtle->sublets,   False);
      if(subtle->tags)      subArrayKill(subtle->tags,      True);
      if(subtle->trays)     subArrayKill(subtle->trays,     True);
      if(subtle->views)     subArrayKill(subtle->views,     True);
      if(subtle->hooks)     subArrayKill(subtle->hooks,     False);

      /* Reset styles to free fonts and substyles */
      subStyleReset(&subtle->styles.all,       0);
      subStyleReset(&subtle->styles.views,     0);
      subStyleReset(&subtle->styles.title,     0);
      subStyleReset(&subtle->styles.sublets,   0);
      subStyleReset(&subtle->styles.separator, 0);
      subStyleReset(&subtle->styles.clients,   0);
      subStyleReset(&subtle->styles.subtle,    0);

      subEventFinish();
      subRubyFinish();
      subEwmhFinish();
      subDisplayFinish();

      free(subtle);
    }
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c;
  char *display = NULL;
  struct sigaction sa;
  const struct option long_options[] =
  {
    { "config",   required_argument, 0, 'c' },
    { "display",  required_argument, 0, 'd' },
    { "help",     no_argument,       0, 'h' },
    { "check",    no_argument,       0, 'k' },
    { "no-randr", no_argument,       0, 'n' },
    { "replace",  no_argument,       0, 'r' },
    { "sublets",  required_argument, 0, 's' },
    { "version",  no_argument,       0, 'v' },
    { "level",    required_argument, 0, 'l' },
    { "debug",    no_argument,       0, 'D' },
    { 0, 0, 0, 0}
  };

  /* Create subtle */
  subtle = (SubSubtle *)(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  subtle->flags    |= (SUB_SUBTLE_XRANDR|SUB_SUBTLE_XINERAMA);
  subtle->loglevel  = DEFAULT_LOGLEVEL;

  /* Parse arguments */
  while(-1 != (c = getopt_long(argc, argv, "c:d:hknrs:vl:D",
      long_options, NULL)))
    {
      switch(c)
        {
          case 'c': subtle->paths.config = optarg;        break;
          case 'd': display = optarg;                     break;
          case 'h': SubtleUsage();                        return 0;
          case 'k': subtle->flags |= SUB_SUBTLE_CHECK;    break;
          case 'n': subtle->flags &= ~SUB_SUBTLE_XRANDR;  break;
          case 'r': subtle->flags |= SUB_SUBTLE_REPLACE;  break;
          case 's': subtle->paths.sublets = optarg;       break;
          case 'v': SubtleVersion();                      return 0;
#ifdef DEBUG
          case 'l':
            subtle->loglevel = SubtleLevel(optarg);
            break;
          case 'D':
            subtle->flags    |= SUB_SUBTLE_DEBUG;
            subtle->loglevel |= DEBUG_LOGLEVEL;
            break;
#else /* DEBUG */
          case 'l':
          case 'D':
            printf("Please recompile %s with `debug=yes'\n", PKG_NAME);
            return 0;
#endif /* DEBUG */
          case '?':
            printf("Try `%s --help' for more information\n", PKG_NAME);
            return -1;
        }
    }

  /* Signal handler */
  sa.sa_handler = SubtleSignal;
  sa.sa_flags   = 0;
  memset(&sa.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigemptyset(&sa.sa_mask);

  sigaction(SIGHUP,  &sa, NULL);
  sigaction(SIGINT,  &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  /* Load and check config only */
  if(subtle->flags & SUB_SUBTLE_CHECK)
    {
      subRubyInit();
      subRubyLoadConfig();
      subRubyFinish();

      free(subtle); ///< We just need to free this

      return 0;
    }

  /* Alloc arrays */
  subtle->clients   = subArrayNew();
  subtle->grabs     = subArrayNew();
  subtle->gravities = subArrayNew();
  subtle->hooks     = subArrayNew();
  subtle->screens   = subArrayNew();
  subtle->sublets   = subArrayNew();
  subtle->tags      = subArrayNew();
  subtle->trays     = subArrayNew();
  subtle->views     = subArrayNew();

  /* Init */
  SubtleVersion();
  subDisplayInit(display);
  subEwmhInit();
  subScreenInit();
  subRubyInit();
  subGrabInit();

  /* Load */
  subRubyLoadConfig();
  subRubyLoadSublets();
  subRubyLoadPanels();

  /* Display */
  subDisplayConfigure();
  subDisplayScan();

  subEventLoop();

  /* Restart if necessary */
  if(subtle->flags & SUB_SUBTLE_RESTART)
    {
      subSubtleFinish();

      printf("Restarting\n");

      execvp(argv[0], argv);
    }
  else subSubtleFinish();

  printf("Exit\n");

  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
