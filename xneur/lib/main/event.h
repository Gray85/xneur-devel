/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Copyright (C) 2006-2016 XNeur Team
 *
 */

#ifndef _EVENT_H_
#define _EVENT_H_

#include <X11/XKBlib.h>

#include "buffer.h"

int get_key_state(Display* display, int key);

struct _event
{
	Window owner_window;		// Window that receives/sends event
	XEvent event;			// Event to process
	XEvent default_event;
	KeyCode backspace;		// Backspace key code
	KeyCode left;			// Left Arrow key code
	KeyCode right;			// Right Arrow key code
	KeyCode space;

	KeySym (*get_cur_keysym) (struct _event *p, struct _window* window);
	int (*get_cur_modifiers) (struct _event *p);

	int  (*get_next_event) (struct _event *p, Display* display);
	void (*send_next_event) (struct _event *p, Display* display);
	void (*set_owner_window) (struct _event *p, Window window);
	void (*send_xkey) (struct _event *p, Display* display, struct _xneur_config* config, KeyCode kc, int modifiers);
	void (*send_string) (struct _event *p, Display* display, struct _xneur_config* config, struct _buffer *str);
	void (*send_backspaces) (struct _event *p, Display* display, struct _xneur_config* config, int n);
	void (*send_selection) (struct _event *p, Display* display, struct _xneur_config* config, int n);
	void (*send_spaces) (struct _event *p, Display* display, struct _xneur_config* config, int n);
	void (*uninit) (struct _event *p);
};

struct _event* event_init(Display* display);

#endif /* _EVENT_H_ */
