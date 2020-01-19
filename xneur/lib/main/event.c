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

#include <X11/XKBlib.h>
//#include <X11/Xlib.h>
//#include <X11/extensions/XTest.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "xnconfig.h"

#include "switchlang.h"
#include "buffer.h"
#include "window.h"
#include "utils.h"
#include "defines.h"

#include "types.h"
#include "log.h"
#include "conversion.h"
#include "list_char.h"

#include "event.h"

int get_key_state(Display* display, int key)
{
	if (display == NULL)
		return 0;

	KeyCode key_code = XKeysymToKeycode(display, key);
	if (key_code == NoSymbol)
		return 0;

	XModifierKeymap *map = XGetModifierMapping(display);

	int key_mask = 0;
	for (int i = 0; i < 8; i++)
	{
		if (map->modifiermap[map->max_keypermod * i] == key_code)
			key_mask = (1 << i);
	}

	XFreeModifiermap(map);

	if (key_mask == 0)
		return 0;

	Window wDummy;
	int iDummy;
	unsigned int mask;
	XQueryPointer(display, DefaultRootWindow(display), &wDummy, &wDummy, &iDummy, &iDummy, &iDummy, &iDummy, &mask);

	return ((mask & key_mask) != 0);
}

static int is_x11_server_time(Display* display, Window window)
{
	Atom prop, da;
	int di;
	int status;
	unsigned char *prop_ret = NULL;
	unsigned long dl;
	Window curr_window = window;
	prop = XInternAtom(display, "GDK_TIMESTAMP_PROP", True);

	while (TRUE)
	{
		unsigned int children_count;
		Window root_window, parent_window;
		Window *children_return = NULL;

		status = XGetWindowProperty(display, curr_window, prop, 0L, sizeof (Atom), False,
								AnyPropertyType, &da, &di, &dl, &dl, &prop_ret);

		if (status == Success && prop_ret)
		{
			char magic = 'a';
			if (prop_ret[0] == magic)
				return TRUE;
		}
		int is_same_screen = XQueryTree(display, curr_window, &root_window, &parent_window, &children_return, &children_count);
		if (children_return != NULL)
			XFree(children_return);
		if (!is_same_screen || parent_window == None || parent_window == root_window)
			break;
		curr_window = parent_window;
	}
	// FIXME >> Firefox and Thunderbird fix (some children windows not have GDK_TIMESTAMP_PROP atom)
	char *app_name = get_wm_class_name(display, window);
	if (app_name != NULL)
	{
		if ((strcmp("Firefox", app_name) == 0) || (strcmp("Thunderbird", app_name) == 0))
		{
			free(app_name);
			return TRUE;
		}
	}
	// FIXME <<
	return FALSE;
}

static unsigned long long current_timestamp(Display* display, Window window)
{
	if (is_x11_server_time(display, window))
	{
		return CurrentTime;
	}
	else
	{
		struct timeval te;
		gettimeofday(&te, NULL); // get current time
		unsigned long long microseconds = te.tv_sec*1000LLU + te.tv_usec; // caculate microseconds
		return microseconds;
	}
}

static void event_send_xkey(struct _event *p, Display* display, struct _xneur_config* config, KeyCode kc, int modifiers)
{
	char *app_name = NULL;
	app_name = get_wm_class_name(display, p->owner_window);

	int is_delay = config->delay_send_key_apps->exist(config->delay_send_key_apps, app_name, BY_PLAIN);
	if (is_delay)
	{
		usleep(config->send_delay * 1000);
	}

	p->event.type			= KeyPress;
	p->event.xkey.type		= KeyPress;
	p->event.xkey.window		= p->owner_window;
	p->event.xkey.root		= DefaultRootWindow(display);
	p->event.xkey.subwindow		= None;
	p->event.xkey.same_screen	= True;
	p->event.xkey.display		= display;
	p->event.xkey.state		= modifiers;
	p->event.xkey.keycode		= kc;
	//p->event.xkey.time		= CurrentTime;
	p->event.xkey.time		= current_timestamp(display, p->owner_window);

	if (config->dont_send_key_release_apps->exist(config->dont_send_key_release_apps, app_name, BY_PLAIN))
	{
		XSendEvent(display, p->owner_window, TRUE, NoEventMask, &p->event);
		XFlush(display);
		log_message(TRACE, _("The event KeyRelease is not sent to the window (ID %d) with name '%s'"), p->owner_window, app_name);
		free(app_name);
		return;
	}

	XSendEvent(display, p->owner_window, TRUE, NoEventMask, &p->event);
	XFlush(display);

	if (is_delay)
	{
		usleep(config->send_delay * 1000);
	}

	p->event.type			= KeyRelease;
	p->event.xkey.type		= KeyRelease;
	//p->event.xkey.time		= CurrentTime;
	p->event.xkey.time		= current_timestamp(display, p->owner_window);

	XSendEvent(display, p->owner_window, TRUE, NoEventMask, &p->event);
	XFlush(display);

	free(app_name);
}

static void common_send_xkey(struct _event *p, Display* display, struct _xneur_config* config, int count, int input_keycode, int Mask_modifier)
{
	for (int i = 0; i < count; i++)
		event_send_xkey(p, display, config, input_keycode, Mask_modifier);
}

static void event_send_backspaces(struct _event *p, Display* display, struct _xneur_config* config, int count)
{
	common_send_xkey(p, display, config, count, p->backspace, None);
}

static void event_send_spaces(struct _event *p, Display* display, struct _xneur_config* config, int count)
{
	common_send_xkey(p, display, config, count, p->space, None);
}

static void event_send_selection(struct _event *p, Display* display, struct _xneur_config* config, int count)
{
	common_send_xkey(p, display, config, count, p->left, None);
	common_send_xkey(p, display, config, count, p->right, ShiftMask);
}

static void event_send_string(struct _event *p, Display* display, struct _xneur_config* config, struct _buffer *str)
{
	for (int i = 0; i < str->cur_pos; i++)
		event_send_xkey(p, display, config, str->keycode[i], str->keycode_modifiers[i]);
}

static void event_set_owner_window(struct _event *p, Window window)
{
	p->owner_window = window;
}

static KeySym event_get_cur_keysym(struct _event *p, struct _window* window)
{
	//return XLookupKeysym(&p->event.xkey, 0);

	KeySym ks;

	/*int nbytes = 0;
    char str[256+1];
	XKeyEvent *e = (XKeyEvent *) &p->event;
    nbytes = XLookupString (e, str, 256, &ks, NULL);
	if (nbytes) {};
	return ks;*/

	XKeyEvent *e = (XKeyEvent *) &p->event;
	ks = XkbKeycodeToKeysym(window->display, e->keycode, window->keymap->latin_group, 0);
	if (ks == NoSymbol)
		ks = XkbKeycodeToKeysym(window->display, e->keycode, 0, 0);
	return ks;
}

static int event_get_cur_modifiers(struct _event *p)
{
	int mask = 0;

	if (p->event.xkey.state & ShiftMask)  // Shift
		mask += (1 << 0); // 1
	if (p->event.xkey.state & LockMask)   // CapsLock
		mask += (1 << 1); // 2
	if (p->event.xkey.state & ControlMask)// Control
		mask += (1 << 2); // 4
	if (p->event.xkey.state & Mod1Mask)   // Alt
		mask += (1 << 3); // 8
	if (p->event.xkey.state & Mod2Mask)   // NumLock
		mask += (1 << 4); // 16
	if (p->event.xkey.state & Mod3Mask)
		mask += (1 << 5); // 32
	if (p->event.xkey.state & Mod4Mask)   // Super (Win)
		mask += (1 << 6); // 64
	if (p->event.xkey.state & Mod5Mask)   // ISO_Level3_Shift
		mask += (1 << 7); // 128

	return mask;
}

static int event_get_next_event(struct _event *p, Display* display)
{
	XNextEvent(display, &(p->event));
	return p->event.type;
}

static void event_send_next_event(struct _event *p, Display* display)
{
	p->event.xkey.state = p->get_cur_modifiers(p) | groups[get_curr_keyboard_group(display)];
	XSendEvent(display, p->event.xany.window,FALSE, NoEventMask, &p->event);
}

static void event_uninit(struct _event *p)
{
	free(p);

	log_message(DEBUG, _("Event is freed"));
}

struct _event* event_init(Display* display)
{
	struct _event *p = (struct _event *) malloc(sizeof(struct _event));
	bzero(p, sizeof(struct _event));

	p->backspace = XKeysymToKeycode(display, XK_BackSpace);
	p->left      = XKeysymToKeycode(display, XK_Left);
	p->right     = XKeysymToKeycode(display, XK_Right);
	p->space     = XKeysymToKeycode(display, XK_space);

	// Functions mapping
	p->get_next_event	= event_get_next_event;
	p->send_next_event	= event_send_next_event;
	p->set_owner_window	= event_set_owner_window;
	p->send_xkey		= event_send_xkey;
	p->send_string		= event_send_string;
	p->get_cur_keysym	= event_get_cur_keysym;
	p->get_cur_modifiers	= event_get_cur_modifiers;
	p->send_backspaces	= event_send_backspaces;
	p->send_selection	= event_send_selection;
	p->send_spaces	= event_send_spaces;
	p->uninit		= event_uninit;

	return p;
}
