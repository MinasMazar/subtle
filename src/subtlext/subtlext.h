
 /**
  * @package subtlext
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
  * @version $Id: src/subtlext/subtlext.h,v 3216 2012/06/15 17:18:12 unexist $
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#ifndef SUBTLEXT_H
#define SUBTLEXT_H 1

/* Includes {{{ */
#include <ruby.h>
#include "shared.h"
/* }}} */

/* Macros {{{ */
#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

#define GET_ATTR(owner,name,value) \
  if(NIL_P(value = rb_iv_get(owner, name))) return Qnil;

#define ROOT DefaultRootWindow(display)
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT  0           ///< Client
#define SUB_TYPE_GRAVITY 1           ///< Gravity
#define SUB_TYPE_VIEW    2           ///< View
#define SUB_TYPE_TAG     3           ///< Tag
#define SUB_TYPE_TRAY    4           ///< Tray
#define SUB_TYPE_SCREEN  5           ///< Screen
#define SUB_TYPE_SUBLET  6           ///< Sublet
/* }}} */

/* Typedefs {{{ */
extern Display *display;
extern VALUE mod;

#ifdef DEBUG
extern int debug;
#endif /* DEBUG */
/* }}} */

/* client.c {{{ */
/* Singleton */
VALUE subClientSingSelect(VALUE self);                            ///< Select client
VALUE subClientSingFind(VALUE self, VALUE value);                 ///< Find client
VALUE subClientSingFirst(VALUE self, VALUE value);                ///< Find first client
VALUE subClientSingCurrent(VALUE self);                           ///< Get current client
VALUE subClientSingVisible(VALUE self);                           ///< Get all visible clients
VALUE subClientSingList(VALUE self);                              ///< Get all clients
VALUE subClientSingRecent(VALUE self);                            ///< Get recent active clients

/* Class */
VALUE subClientInstantiate(Window win);                           ///< Instantiate client
VALUE subClientInit(VALUE self, VALUE win);                       ///< Create client
VALUE subClientUpdate(VALUE self);                                ///< Update client
VALUE subClientViewList(VALUE self);                              ///< Get views clients is on
VALUE subClientFlagsAskFull(VALUE self);                          ///< Is client fullscreen
VALUE subClientFlagsAskFloat(VALUE self);                         ///< Is client floating
VALUE subClientFlagsAskStick(VALUE self);                         ///< Is client stick
VALUE subClientFlagsAskResize(VALUE self);                        ///< Is client resize
VALUE subClientFlagsAskUrgent(VALUE self);                        ///< Is client urgent
VALUE subClientFlagsAskZaphod(VALUE self);                        ///< Is client zaphod
VALUE subClientFlagsAskFixed(VALUE self);                         ///< Is client fixed
VALUE subClientFlagsAskBorderless(VALUE self);                    ///< Is client borderless
VALUE subClientFlagsToggleFull(VALUE self);                       ///< Toggle fullscreen
VALUE subClientFlagsToggleFloat(VALUE self);                      ///< Toggle floating
VALUE subClientFlagsToggleStick(VALUE self);                      ///< Toggle stick
VALUE subClientFlagsToggleResize(VALUE self);                     ///< Toggle resize
VALUE subClientFlagsToggleUrgent(VALUE self);                     ///< Toggle urgent
VALUE subClientFlagsToggleZaphod(VALUE self);                     ///< Toggle zaphod
VALUE subClientFlagsToggleFixed(VALUE self);                      ///< Toggle fixed
VALUE subClientFlagsToggleBorderless(VALUE self);                 ///< Toggle borderless
VALUE subClientFlagsWriter(VALUE self, VALUE value);              ///< Set multiple flags
VALUE subClientRestackRaise(VALUE self);                          ///< Raise client
VALUE subClientRestackLower(VALUE self);                          ///< Lower client
VALUE subClientAskAlive(VALUE self);                              ///< Is client alive
VALUE subClientGravityReader(VALUE self);                         ///< Get client gravity
VALUE subClientGravityWriter(VALUE self, VALUE value);            ///< Set client gravity
VALUE subClientGeometryReader(VALUE self);                        ///< Get client geometry
VALUE subClientGeometryWriter(int argc, VALUE *argv,
  VALUE self);                                                    ///< Set client geometry
VALUE subClientScreenReader(VALUE self);                          ///< Get client screen
VALUE subClientResizeWriter(VALUE self, VALUE value);             ///< Set Client resize
VALUE subClientToString(VALUE self);                              ///< Client to string
VALUE subClientKill(VALUE self);                                  ///< Kill client
/* }}} */

/* color.c {{{ */
unsigned long subColorPixel(VALUE red, VALUE green,
  VALUE blue, XColor *xcolor);                                    ///< Get pixel value
VALUE subColorInstantiate(unsigned long pixel);                   ///< Instantiate color
VALUE subColorInit(int argc, VALUE *argv, VALUE self);            ///< Create new color
VALUE subColorToHex(VALUE self);                                  ///< Convert to hex string
VALUE subColorToArray(VALUE self);                                ///< Color to array
VALUE subColorToHash(VALUE self);                                 ///< Color to hash
VALUE subColorToString(VALUE self);                               ///< Convert to string
VALUE subColorOperatorPlus(VALUE self, VALUE value);              ///< Concat string
VALUE subColorEqual(VALUE self, VALUE other);                     ///< Whether objects are equal
VALUE subColorEqualTyped(VALUE self, VALUE other);                ///< Whether objects are equal typed
/* }}} */

/* geometry.c {{{ */
VALUE subGeometryInstantiate(int x, int y, int width,
  int height);                                                    ///< Instantiate geometry
void subGeometryToRect(VALUE self, XRectangle *r);                ///< Geometry to rect
VALUE subGeometryInit(int argc, VALUE *argv, VALUE self);         ///< Create new geometry
VALUE subGeometryToArray(VALUE self);                             ///< Geometry to array
VALUE subGeometryToHash(VALUE self);                              ///< Geometry to hash
VALUE subGeometryToString(VALUE self);                            ///< Geometry to string
VALUE subGeometryEqual(VALUE self, VALUE other);                  ///< Whether objects are equal
VALUE subGeometryEqualTyped(VALUE self, VALUE other);             ///< Whether objects are equal typed
/* }}} */

/* gravity.c {{{ */
/* Singleton */
VALUE subGravitySingFind(VALUE self, VALUE value);                ///< Find gravity
VALUE subGravitySingFirst(VALUE self, VALUE value);               ///< Find first gravity
VALUE subGravitySingList(VALUE self);                             ///< Get all gravities

/* Class */
VALUE subGravityInstantiate(char *name);                          ///< Instantiate gravity
VALUE subGravityInit(int argc, VALUE *argv, VALUE self);          ///< Create new gravity
VALUE subGravitySave(VALUE self);                                 ///< Save gravity
VALUE subGravityClients(VALUE self);                              ///< List clients with gravity
VALUE subGravityGeometryFor(VALUE self, VALUE value);             ///< Get geometry gravity for screen
VALUE subGravityGeometryReader(VALUE self);                       ///< Get geometry gravity
VALUE subGravityGeometryWriter(int argc, VALUE *argv, VALUE self);///< Get geometry gravity
VALUE subGravityTilingWriter(VALUE self, VALUE value);            ///< Set gravity tiling
VALUE subGravityToString(VALUE self);                             ///< Gravity to string
VALUE subGravityToSym(VALUE self);                                ///< Gravity to symbol
VALUE subGravityKill(VALUE self);                                 ///< Kill gravity
/* }}} */

/* icon.c {{{ */
VALUE subIconAlloc(VALUE self);                                   ///< Allocate icon
VALUE subIconInit(int argc, VALUE *argv, VALUE self);             ///< Init icon
VALUE subIconDrawPoint(int argc, VALUE *argv, VALUE self);        ///< Draw a point
VALUE subIconDrawLine(int argc, VALUE *argv, VALUE self);         ///< Draw a line
VALUE subIconDrawRect(int argc, VALUE *argv, VALUE self);         ///< Draw a rect
VALUE subIconCopyArea(int argc, VALUE *argv, VALUE self);         ///< Copy icon area
VALUE subIconClear(int argc, VALUE *argv, VALUE self);            ///< Clear icon
VALUE subIconAskBitmap(VALUE self);                               ///< Whether icon is bitmap
VALUE subIconToString(VALUE self);                                ///< Convert to string
VALUE subIconOperatorPlus(VALUE self, VALUE value);               ///< Concat string
VALUE subIconOperatorMult(VALUE self, VALUE value);               ///< Concat string
VALUE subIconEqual(VALUE self, VALUE other);                      ///< Whether objects are equal
VALUE subIconEqualTyped(VALUE self, VALUE other);                 ///< Whether objects are equal typed
/* }}} */

/* screen.c {{{ */
/* Singleton */
VALUE subScreenSingFind(VALUE self, VALUE id);                    ///< Find screen
VALUE subScreenSingList(VALUE self);                              ///< Get all screens
VALUE subScreenSingCurrent(VALUE self);                           ///< Get current screen

/* Class */
VALUE subScreenInstantiate(int id);                               ///< Instantiate screen
VALUE subScreenInit(VALUE self, VALUE id);                        ///< Create new screen
VALUE subScreenUpdate(VALUE self);                                ///< Update screen
VALUE subScreenJump(VALUE self);                                  ///< Jump to this screen
VALUE subScreenViewReader(VALUE self);                            ///< Get screen view
VALUE subScreenViewWriter(VALUE self, VALUE value);               ///< Set screen view
VALUE subScreenAskCurrent(VALUE self);                            ///< Whether screen is current
VALUE subScreenToString(VALUE self);                              ///< Screen to string
/* }}} */

/* sublet.c {{{ */
/* Singleton */
VALUE subSubletSingFind(VALUE self, VALUE value);                 ///< Find sublet
VALUE subSubletSingFirst(VALUE self, VALUE value);                ///< Find first sublet
VALUE subSubletSingList(VALUE self);                              ///< Get all sublets

/* Class */
VALUE subSubletInit(VALUE self, VALUE name);                      ///< Create sublet
VALUE subSubletUpdate(VALUE self);                                ///< Update sublet
VALUE subSubletSend(VALUE self, VALUE value);                     ///< Send data to sublet
VALUE subSubletVisibilityShow(VALUE self);                        ///< Show sublet
VALUE subSubletVisibilityHide(VALUE self);                        ///< Hide sublet
VALUE subSubletGeometryReader(VALUE self);                        ///< Get sublet geometry
VALUE subSubletToString(VALUE self);                              ///< Sublet to string
VALUE subSubletKill(VALUE self);                                  ///< Kill sublet
/* }}} */

/* subtle.c {{{ */
/* Singleton */
VALUE subSubtleSingDisplayReader(VALUE self);                     ///< Get display
VALUE subSubtleSingDisplayWriter(VALUE self, VALUE display);      ///< Set display
VALUE subSubtleSingAskRunning(VALUE self);                        ///< Is subtle running
VALUE subSubtleSingSelect(VALUE self);                            ///< Select window
VALUE subSubtleSingRender(VALUE self);                            ///< Render panels
VALUE subSubtleSingReload(VALUE self);                            ///< Reload config and sublets
VALUE subSubtleSingRestart(VALUE self);                           ///< Restart subtle
VALUE subSubtleSingQuit(VALUE self);                              ///< Quit subtle
VALUE subSubtleSingColors(VALUE self);                            ///< Get colors
VALUE subSubtleSingFont(VALUE self);                              ///< Get font
VALUE subSubtleSingSpawn(VALUE self, VALUE cmd);                  ///< Spawn command
/* }}} */

/* subtlext.c {{{ */
void subSubtlextConnect(char *display_string);                    ///< Connect to display
void subSubtlextBacktrace(void);                                  ///< Print ruby backtrace
VALUE subSubtlextConcat(VALUE str1, VALUE str2);                  ///< Concat strings
VALUE subSubtlextParse(VALUE value, char *buf,
  int len, int *flags);                                           ///< Parse arguments
VALUE subSubtlextOneOrMany(VALUE value, VALUE prev);              ///< Return one or many
VALUE subSubtlextManyToOne(VALUE value);                          ///< Return one from many
Window *subSubtlextWindowList(char *prop_name, int *size);        ///< Get window list
int subSubtlextFindString(char *prop_name, char *source,
  char **name, int flags);                                        ///< Find string id
VALUE subSubtlextFindObjects(char *prop_name, char *class_name,
  char *source, int flags, int first);                            ///< Find objects
VALUE subSubtlextFindWindows(char *prop_name, char *class_name,
  char *source, int flags, int first);                            ///< Find objects
VALUE subSubtlextFindObjectsGeometry(char *prop_name,
  char *class_name, char *source, int flags, int first);          ///< Find objects with geometries
/* }}} */

/* tag.c {{{ */
/* Singleton */
VALUE subTagSingFind(VALUE self, VALUE value);                    ///< Find tag
VALUE subTagSingFirst(VALUE self, VALUE value);                   ///< Find first tag
VALUE subTagSingVisible(VALUE self);                              ///< Get all visible tags
VALUE subTagSingList(VALUE self);                                 ///< Get all tags

/* Class */
VALUE subTagInstantiate(char *name);                              ///< Instantiate tag
VALUE subTagInit(VALUE self, VALUE name);                         ///< Create tag
VALUE subTagSave(VALUE self);                                     ///< Save tag
VALUE subTagClients(VALUE self);                                  ///< Get clients with tag
VALUE subTagViews(VALUE self);                                    ///< Get views with tag
VALUE subTagToString(VALUE self);                                 ///< Tag to string
VALUE subTagKill(VALUE self);                                     ///< Kill tag
/* }}} */

/* tray.c {{{ */
/* Singleton */
VALUE subTraySingFind(VALUE self, VALUE name);                    ///< Find tray
VALUE subTraySingFirst(VALUE self, VALUE name);                   ///< Find first tray
VALUE subTraySingList(VALUE self);                                ///< Get all trays

/* Class */
VALUE subTrayInstantiate(Window win);                             ///< Instantiate tray
VALUE subTrayInit(VALUE self, VALUE win);                         ///< Create tray
VALUE subTrayUpdate(VALUE self);                                  ///< Update tray
VALUE subTrayToString(VALUE self);                                ///< Tray to string
VALUE subTrayKill(VALUE self);                                    ///< Kill tray
/* }}} */

/* view.c {{{ */
/* Singleton */
VALUE subViewSingFind(VALUE self, VALUE name);                    ///< Find view
VALUE subViewSingFirst(VALUE self, VALUE name);                   ///< Find first view
VALUE subViewSingCurrent(VALUE self);                             ///< Get current view
VALUE subViewSingVisible(VALUE self);                             ///< Get all visible views
VALUE subViewSingList(VALUE self);                                ///< Get all views

/* Class */
VALUE subViewInstantiate(char *name);                             ///< Instantiate view
VALUE subViewInit(VALUE self, VALUE name);                        ///< Create view
VALUE subViewUpdate(VALUE self);                                  ///< Update view
VALUE subViewSave(VALUE self);                                    ///< Save view
VALUE subViewClients(VALUE self);                                 ///< Get clients of view
VALUE subViewJump(VALUE self);                                    ///< Jump to view
VALUE subViewSelectNext(VALUE self);                              ///< Select next view
VALUE subViewSelectPrev(VALUE self);                              ///< Select next view
VALUE subViewAskCurrent(VALUE self);                              ///< Whether view the current
VALUE subViewIcon(VALUE self);                                    ///< View icon if any
VALUE subViewToString(VALUE self);                                ///< View to string
VALUE subViewKill(VALUE self);                                    ///< Kill view
/* }}} */

/* window.c {{{ */
/* Singleton */
VALUE subWindowSingOnce(VALUE self, VALUE geometry);              ///< Run window once

/* Class */
VALUE subWindowInstantiate(VALUE geometry);                       ///< Instantiate window
VALUE subWindowDispatcher(int argc, VALUE *argv, VALUE self);     ///< Window dispatcher
VALUE subWindowAlloc(VALUE self);                                 ///< Allocate window
VALUE subWindowInit(VALUE self, VALUE geometry);                  ///< Init window
VALUE subWindowSubwindow(VALUE self, VALUE geometry);             ///< Create a subwindow
VALUE subWindowNameWriter(VALUE self, VALUE value);               ///< Set name
VALUE subWindowFontWriter(VALUE self, VALUE value);               ///< Set font
VALUE subWindowFontYReader(VALUE self);                           ///< Get y offset of font
VALUE subWindowFontHeightReader(VALUE self);                      ///< Get height of font
VALUE subWindowFontWidth(VALUE self, VALUE string);               ///< Get string width for font
VALUE subWindowForegroundWriter(VALUE self, VALUE value);         ///< Set foreground
VALUE subWindowBackgroundWriter(VALUE self, VALUE value);         ///< Set background
VALUE subWindowBorderColorWriter(VALUE self, VALUE value);        ///< Set border color
VALUE subWindowBorderSizeWriter(VALUE self, VALUE value);         ///< Set border size
VALUE subWindowGeometryReader(VALUE self);                        ///< Get geometry
VALUE subWindowGeometryWriter(VALUE self, VALUE value);           ///< Set geometry
VALUE subWindowOn(int argc, VALUE *argv, VALUE self);             ///< Add event handler
VALUE subWindowDrawPoint(int argc, VALUE *argv, VALUE self);      ///< Draw a point
VALUE subWindowDrawLine(int argc, VALUE *argv, VALUE self);       ///< Draw a line
VALUE subWindowDrawRect(int argc, VALUE *argv, VALUE self);       ///< Draw a rect
VALUE subWindowDrawText(int arcg, VALUE *argv, VALUE self);       ///< Draw text
VALUE subWindowDrawIcon(int arcg, VALUE *argv, VALUE self);       ///< Draw icon
VALUE subWindowClear(int argc, VALUE *argv, VALUE self);          ///< Clear area or window
VALUE subWindowRedraw(VALUE self);                                ///< Redraw window
VALUE subWindowRaise(VALUE self);                                 ///< Raise window
VALUE subWindowLower(VALUE self);                                 ///< Lower window
VALUE subWindowShow(VALUE self);                                  ///< Show window
VALUE subWindowHide(VALUE self);                                  ///< Hide window
VALUE subWindowAskHidden(VALUE self);                             ///< Whether window is hidden
VALUE subWindowKill(VALUE self);                                  ///< Kill window
/* }}} */

#endif /* SUBTLEXT_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
