/*	$OpenBSD: p_new.c,v 1.2 1998/07/24 17:08:13 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1995                    *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/* p_new.c
 * Creation of a new panel 
 */
#include "panel.priv.h"

MODULE_ID("$From: p_new.c,v 1.2 1998/02/11 12:14:01 tom Exp $")

/*+-------------------------------------------------------------------------
  Get root (i.e. stdscr's) panel.
  Establish the pseudo panel for stdscr if necessary.
--------------------------------------------------------------------------*/
static PANEL*
root_panel(void)
{
  if(_nc_stdscr_pseudo_panel == (PANEL*)0)
    {
      
      assert(stdscr && !_nc_bottom_panel && !_nc_top_panel);
      _nc_stdscr_pseudo_panel = (PANEL*)malloc(sizeof(PANEL));
      if (_nc_stdscr_pseudo_panel != 0) {
	PANEL* pan  = _nc_stdscr_pseudo_panel;
	WINDOW* win = stdscr;
	pan->win = win;
	getbegyx(win, pan->wstarty, pan->wstartx);
	pan->wendy   = pan->wstarty + getmaxy(win);
	pan->wendx   = pan->wstartx + getmaxx(win);
	pan->below   = (PANEL*)0;
	pan->above   = (PANEL*)0;
        pan->obscure = (PANELCONS*)0;
#ifdef TRACE
	pan->user = "stdscr";
#else
	pan->user = (void*)0;
#endif
	_nc_panel_link_bottom(pan);
      }
    }
  return _nc_stdscr_pseudo_panel;
}

PANEL *
new_panel(WINDOW *win)
{
  PANEL *pan = (PANEL*)0;

  (void)root_panel();
  assert(_nc_stdscr_pseudo_panel);

  if (!(win->_flags & _ISPAD) && (pan = (PANEL*)malloc(sizeof(PANEL))))
    {
      pan->win = win;
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      getbegyx(win, pan->wstarty, pan->wstartx);
      pan->wendy = pan->wstarty + getmaxy(win);
      pan->wendx = pan->wstartx + getmaxx(win);
#ifdef TRACE
      pan->user = "new";
#else
      pan->user = (char *)0;
#endif
      pan->obscure = (PANELCONS *)0;
      (void)show_panel(pan);
    }
  return(pan);
}
