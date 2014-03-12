
 /**
  * @package subtle
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/shared/shared.h,v 3204 2012/05/22 21:15:06 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#ifndef SHARED_H
#define SHARED_H 1

/* Includes {{{ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>
#include <ruby/oniguruma.h>

#include "config.h"

#ifdef HAVE_X11_XFT_XFT_H
#include <X11/Xft/Xft.h>
#endif /* HAVE_X11_XFT_XFT_H */
/* }}} */

/* Macros {{{ */
#define SEPARATOR "^"                                             ///< Color separator
#define LENGTH(a) (sizeof(a) / sizeof(a[0]))                      ///< Array length
#define RINT(r)   printf("<%s:%d> %s: x=%d, y=%d, width=%d, height=%d\n", \
  __FILE__, __LINE__, #r, r.x, r.y, r.width, r.height);           ///< Print a XRectangle

#define DEFFONT   "-*-*-*-*-*-*-14-*-*-*-*-*-*-*"                 ///< Default font

#define DATA(d)   ((SubData)d)                                    ///< Cast to SubData
#define FONT(f)   ((SubFont *)f)                                  ///< Cast to SubFont
#define TEXT(t)   ((SubText *)t)                                  ///< Cast to SubText
#define ITEM(i)   ((SubTextItem *)i)                              ///< Cast to SubTextItem
/* }}} */

/* Flags {{{ */
/* View select flags */
#define SUB_VIEW_NEXT       0L                                    ///< View next
#define SUB_VIEW_PREV       1L                                    ///< View prev

/* EWMH flags */
#define SUB_EWMH_FULL       (1L << 0)                             ///< EWMH full flag
#define SUB_EWMH_FLOAT      (1L << 1)                             ///< EWMH float flag
#define SUB_EWMH_STICK      (1L << 2)                             ///< EWMH stick flag
#define SUB_EWMH_RESIZE     (1L << 3)                             ///< EWMH resize flag
#define SUB_EWMH_URGENT     (1L << 4)                             ///< EWMH urgent flag
#define SUB_EWMH_ZAPHOD     (1L << 5)                             ///< EWMH zaphod flag
#define SUB_EWMH_FIXED      (1L << 6)                             ///< EWMH fixed flag
#define SUB_EWMH_CENTER     (1L << 7)                             ///< EWMH center flag
#define SUB_EWMH_BORDERLESS (1L << 8)                             ///< EWMH fixed flag
#define SUB_EWMH_VISIBLE    (1L << 9)                             ///< EWMH visible flag
#define SUB_EWMH_HIDDEN     (1L << 10)                            ///< EWMH hidden flag
#define SUB_EWMH_HORZ       (1L << 11)                            ///< EWMH horizontal flag
#define SUB_EWMH_VERT       (1L << 12)                            ///< EWMH vertical flag

/* Match types flags */
#define SUB_MATCH_NAME      (1L << 0)                             ///< Match SUBTLE_NAME
#define SUB_MATCH_INSTANCE  (1L << 1)                             ///< Match instance of WM_CLASS
#define SUB_MATCH_CLASS     (1L << 2)                             ///< Match class of WM_CLASS
#define SUB_MATCH_GRAVITY   (1L << 3)                             ///< Match gravity
#define SUB_MATCH_ROLE      (1L << 4)                             ///< Match window role
#define SUB_MATCH_PID       (1L << 5)                             ///< Match pid
#define SUB_MATCH_EXACT     (1L << 6)                             ///< Match exact string
/* }}} */

/* Typedefs {{{ */
typedef union subdata_t /* {{{ */
{
  unsigned long num;                                              ///< Data number
  char          *string;                                          ///< Data string
} SubData; /* }}} */

typedef struct subfont_t /* {{{ */
{
  int      y, height;                                             ///< Font y, height
  XFontSet xfs;                                                   ///< Font set

#ifdef HAVE_X11_XFT_XFT_H
  XftFont  *xft;                                                  ///< Font XFT font
  XftDraw  *draw;                                                 ///< Font XFT draw
#endif /* HAVE_X11_XFT_XFT_H */
} SubFont; /* }}} */

typedef union submessagedata_t /* {{{ */
{
  char  b[20];                                                    ///< MessageData char
  short s[10];                                                    ///< MessageData short
  long  l[5];                                                     ///< MessageData long
} SubMessageData; /* }}} */
/* }}} */

/* Memory {{{ */
void *subSharedMemoryAlloc(size_t n, size_t size);                ///< Allocate memory
void *subSharedMemoryRealloc(void *mem, size_t size);             ///< Reallocate memory
/* }}} */

/* Regex {{{ */
regex_t *subSharedRegexNew(char *pattern);                          ///< Create new regex
int subSharedRegexMatch(regex_t *preg, char *string);             ///< Check if string matches preg
void subSharedRegexKill(regex_t *preg);                           ///< Kill regex
/* }}} */

/* Property {{{ */
char *subSharedPropertyGet(Display *disp, Window win,
  Atom type, Atom prop, unsigned long *size);                     ///< Get window property
char **subSharedPropertyGetStrings(Display *disp, Window win,
  Atom prop, int *nlist);                                         ///< Get window property list
void subSharedPropertySetStrings(Display *disp, Window win,
  Atom prop, char **list, int nlist);                             ///< Set window property list
void subSharedPropertyName(Display *disp, Window win,
  char **name, char *fallback);                                   ///< Get window name
void subSharedPropertyClass(Display *disp, Window win,
  char **inst, char **klass);                                     ///< Get window class
void subSharedPropertyGeometry(Display *disp, Window win,
  XRectangle *geometry);                                          ///< Get window geometry
void subSharedPropertyDelete(Display *disp, Window win,
  Atom prop);                                                     ///< Delete window property
/* }}} */

/* Draw {{{ */
void subSharedDrawIcon(Display *disp, GC gc, Window win,
  int x, int y, int width, int height, long fg, long bg,
  Pixmap pixmap, int bitmap);                                     ///< Draw icons
void subSharedDrawString(Display *disp, GC gc, SubFont *f,
  Window win, int x, int y, long fg, long bg,
  const char *text, int len);                                     ///< Draw text
/* }}} */

/* Font {{{ */
SubFont *subSharedFontNew(Display *disp, const char *name);       ///< Create font
void subSharedFontKill(Display *disp, SubFont *f);                ///< Kill font
/* }}} */

/* Misc {{{ */
unsigned long subSharedParseColor(Display *disp, char *name);     ///< Parse color
KeySym subSharedParseKey(Display *disp, const char *key,
  unsigned int *code, unsigned int *state, int *mouse);           ///< Parse keys
pid_t subSharedSpawn(char *cmd);                                  ///< Spawn command
int subSharedStringWidth(Display *disp, SubFont *f,
  const char *text, int len, int *left, int *right, int center);  ///< Get text width
/* }}} */

#ifndef SUBTLE

/* Message {{{ */
int subSharedMessage(Display *disp, Window win, char *type,
  SubMessageData data, int format, int xsync);                    ///< Send client message
/* }}} */

#endif /* SUBTLE */

#endif /* SHARED_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
