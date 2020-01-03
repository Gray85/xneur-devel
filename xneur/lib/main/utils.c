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

#include <stdlib.h>
#include <string.h>

#include "window.h"
#include "bind_table.h"

#include "types.h"
#include "utils.h"

#define MAXSTR 10000

static Window get_root_window(Display *display, Window window)
{
	Window root_window, parent_window, *childrens;
	unsigned int childrens_count;

	if (!XQueryTree(display, window, &root_window, &parent_window, &childrens, &childrens_count))
		return None;

	if (childrens)
		XFree(childrens);

	if (root_window == parent_window)
		return window;

	return parent_window;
}

static Window find_window_with_atom(Display *display, Window window, Atom atom)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *data = NULL;

	int status = XGetWindowProperty(display, window, atom, 0, (MAXSTR + 3) / 4, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &data);
	if (status == Success)
	{
		XFree(data);
		if (data != NULL)
			return window;
	}

	Window root_window = get_root_window(display, window);
	if (root_window == None)
		return None;

	status = XGetWindowProperty(display, root_window, atom, 0, (MAXSTR + 3) / 4, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &data);
	if (status == Success)
	{
		XFree(data);
		if (data != NULL)
			return root_window;
	}

	return None;
}

unsigned char *get_win_prop(Display *display, Window window, Atom atom, long *nitems, Atom *type, int *size)
{
	unsigned long bytes_after; /* unused */
	unsigned char *prop;
	int status;

	status = XGetWindowProperty(display, window, atom, 0, (~0L),
                              FALSE, AnyPropertyType, type,
                              size, nitems, &bytes_after,
                              &prop);
	if (status != Success)
		return NULL;

	return prop;
}

char* get_wm_class_name(Display *display, Window window)
{
	if (window == None)
		return NULL;

	Window named_window = find_window_with_atom(display, window, XInternAtom(display, "WM_CLASS", True));
	if (named_window == None)
	{
		named_window = find_window_with_atom(display, window, XInternAtom(display, "WM_NAME", True));

		if (named_window == None)
			return NULL;

		Atom type;
		int size;
		long nitems;

		Atom request = XInternAtom(display, "WM_NAME", False);
		unsigned char *data = get_win_prop(display, named_window, request, &nitems, &type, &size);

		if (nitems > 0 && data != NULL) {
			// Returned string is freed by `free` function, but result from `get_win_prop` must be freed by `XFree` function
			// so just make copy
			char* result = strdup((char *)data);
			XFree(data);
			return result;
		}

		return NULL;
	}

	XClassHint *wm_class = XAllocClassHint();

	if (!XGetClassHint(display, named_window, wm_class))
	{
		XFree(wm_class);
		return NULL;
	}

	char *string = strdup(wm_class->res_class);

	XFree(wm_class->res_class);
	XFree(wm_class->res_name);
	XFree(wm_class);

	return string;
}
