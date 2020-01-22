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
#include <X11/extensions/XInput2.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xnconfig.h"

#include "utils.h"
#include "window.h"
#include "defines.h"

#include "types.h"
#include "list_char.h"
#include "log.h"

#include "focus.h"

const char *verbose_forced_mode[]	= {"Default", "Manual", "Automatic"};
const char *verbose_focus_status[]	= {"Processed", "Excluded"};


// Private
int focus_get_focused_window(struct _focus *p, Display* display)
{
	if (p) {};
	Window new_window;
	int revert_to;
	XGetInputFocus(display, &new_window, &revert_to);

	return new_window;
}

static int get_focus(struct _focus *p, Display* display, struct _xneur_config *config, int *forced_mode, int *focus_status, int *autocompletion_mode)
{
	*forced_mode	= FORCE_MODE_NORMAL;
	*focus_status	= FOCUS_PROCESSED;
	*autocompletion_mode	= AUTOCOMPLETION_INCLUDED;

	char *new_app_name = NULL;

	// Clear masking on unfocused window
	//p->update_grab_events(p, display, config, LISTEN_DONTGRAB_INPUT);

	Window new_window;
	int show_message = TRUE;
	while (TRUE)
	{
		// Wait for new window was focused.
		usleep(500000);

		// This code commented be cause function XGrabKey for _NET_ACTIVE_WINDOW
		// dont process modifier keys (see utils.h)
		/*if (main_window->_NET_SUPPORTED)
		{
			Atom type;
			int size;
			long nitems;

			Atom request = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
			Window root = XDefaultRootWindow(display);
			unsigned char *data = get_win_prop(display, root, request, &nitems, &type, &size);

			if (nitems > 0)
				new_window = *((Window*)data);
			else
				new_window = None;

			free(data);
		}
		else
		{*/
			int revert_to;
			XGetInputFocus(display, &new_window, &revert_to);
		//}

		// Catch not empty and not system window
		if (new_window != None && new_window > 1000)
		{
			new_app_name = get_wm_class_name(display, new_window);
			if (new_app_name != NULL)
				break;
		}

		if (show_message)
		{
			log_message(DEBUG, _("New window empty"));
			show_message = FALSE;
		}
		usleep(100000);
	}

	//char *new_app_name = get_wm_class_name(display, new_window);
	//if (new_app_name != NULL)
	//{
		if (config->excluded_apps->exist(config->excluded_apps, new_app_name, BY_PLAIN))
			*focus_status = FOCUS_EXCLUDED;

		if (new_app_name != NULL)
		{
			if (strcmp("Gxneur", new_app_name) == 0)
				*focus_status = FOCUS_EXCLUDED;

			if (strcmp("Kdeneur", new_app_name) == 0)
				*focus_status = FOCUS_EXCLUDED;
		}

		if (config->auto_apps->exist(config->auto_apps, new_app_name, BY_PLAIN))
			*forced_mode = FORCE_MODE_AUTO;
		else if (config->manual_apps->exist(config->manual_apps, new_app_name, BY_PLAIN))
			*forced_mode = FORCE_MODE_MANUAL;

		if (config->autocompletion_excluded_apps->exist(config->autocompletion_excluded_apps, new_app_name, BY_PLAIN))
			*autocompletion_mode	= AUTOCOMPLETION_EXCLUDED;
	//}
	//else
	//	*focus_status = FOCUS_EXCLUDED;

	Window old_window = p->owner_window;
	if (new_window == old_window)
	{
		if (new_app_name != NULL)
			free(new_app_name);
		if (config->troubleshoot_full_screen)
		{
			Window root_return;
			int x_return, y_return, root_x_return, root_y_return;
			unsigned int width_return, height_return, root_width_return, root_height_return;
			unsigned int border_width_return;
			unsigned int depth_return;
			XGetGeometry(display, p->parent_window, &root_return, &x_return, &y_return, &width_return,
							&height_return, &border_width_return, &depth_return);
			XGetGeometry(display, root_return, &root_return, &root_x_return, &root_y_return, &root_width_return,
							&root_height_return, &border_width_return, &depth_return);
			if ((x_return == 0) && (y_return == 0) &&
			    (width_return == root_width_return) && (height_return == root_height_return))
				*forced_mode = FORCE_MODE_MANUAL;
		}
		return FALSE;
	}

	log_message(DEBUG, _("Focused window %d"), new_window);

	// Up to heighted window
	p->parent_window = new_window;
	while (TRUE)
	{
		unsigned int children_count;
		Window root_window, parent_window;
		Window *children_return = NULL;

		int is_same_screen = XQueryTree(display, p->parent_window, &root_window, &parent_window, &children_return, &children_count);
		if (children_return != NULL)
			XFree(children_return);
		if (!is_same_screen || parent_window == None || parent_window == root_window)
			break;

		p->parent_window = parent_window;
	}

	// Replace unfocused window to focused window
	p->owner_window = new_window;

	if (config->troubleshoot_full_screen)
	{
		Window root_return;
		int x_return, y_return, root_x_return, root_y_return;
		unsigned int width_return, height_return, root_width_return, root_height_return;
		unsigned int border_width_return;
		unsigned int depth_return;
		XGetGeometry(display, p->parent_window, &root_return, &x_return, &y_return, &width_return,
						&height_return, &border_width_return, &depth_return);
		XGetGeometry(display, root_return, &root_return, &root_x_return, &root_y_return, &root_width_return,
						&root_height_return, &border_width_return, &depth_return);
		if ((x_return == 0) && (y_return == 0) &&
			(width_return == root_width_return) && (height_return == root_height_return))
			*forced_mode = FORCE_MODE_MANUAL;
	}

	log_message(DEBUG, _("Process new window (ID %d) with name '%s' (status %s, mode %s)"), new_window, new_app_name, _(verbose_focus_status[*focus_status]), _(verbose_forced_mode[*forced_mode]));

	if (new_app_name != NULL)
		free(new_app_name);
	return TRUE;
}

static void grab_button(Display* display, int is_grab)
{
	if (is_grab)
	{
		XIEventMask mask;
		mask.deviceid = XIAllMasterDevices;
		mask.mask_len = XIMaskLen(XI_RawButtonPress);
		mask.mask = (void *)calloc(mask.mask_len, sizeof(char));
		XISetMask(mask.mask, XI_RawButtonPress);
		XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
		free(mask.mask);
	}
	else
	{
		XIEventMask mask;
		mask.deviceid = XIAllMasterDevices;
		mask.mask_len = XIMaskLen(XI_RawButtonPress);
		mask.mask = (void *)calloc(mask.mask_len, sizeof(char));
		XISetMask(mask.mask, 0);
		XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
		free(mask.mask);
	}
}

static void grab_all_keys(Display* display, Window window, int use_x_input_api, int is_grab)
{
	if (is_grab)
	{
		// Grab all keys...
		if (use_x_input_api) {
			XIEventMask mask;
			mask.deviceid = XIAllDevices;
			mask.mask_len = XIMaskLen(XI_KeyPress)+
							XIMaskLen(XI_KeyRelease);
			mask.mask = (void *)calloc(mask.mask_len, sizeof(char));
			XISetMask(mask.mask, XI_KeyPress);
			XISetMask(mask.mask, XI_KeyRelease);
			XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
			free(mask.mask);
		}
		else {
			XGrabKey(display, AnyKey, AnyModifier, window, FALSE, GrabModeAsync, GrabModeAsync);
		}
	}
	else
	{
		if (use_x_input_api) {
			XIEventMask mask;
			mask.deviceid = XIAllMasterDevices;
			mask.mask_len = XIMaskLen(XI_KeyPress);
			mask.mask = (void *)calloc(mask.mask_len, sizeof(char));
			XISetMask(mask.mask, 0);
			XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
			free(mask.mask);
		}
		else {
			// Ungrab all keys in app window...
			XUngrabKey(display, AnyKey, AnyModifier, window);
		}
	}

	XSelectInput(display, window, FOCUS_CHANGE_MASK);
}

static int focus_get_focus_status(struct _focus *p, Display* display, struct _xneur_config *config, int *forced_mode, int *focus_status, int *autocompletion_mode)
{
	int focus = get_focus(p, display, config, forced_mode, focus_status, autocompletion_mode);

	p->last_focus = config->tracking_input ? *focus_status : FOCUS_EXCLUDED;

	return focus;
}

static void focus_update_grab_events(struct _focus *p, Display* display, struct _xneur_config *config, int use_x_input_api, int mode)
{
	char *owner_window_name = get_wm_class_name(display, p->owner_window);

	if ((mode == LISTEN_DONTGRAB_INPUT) || (p->last_focus == FOCUS_EXCLUDED))
	{
		grab_button(display, FALSE);
		grab_all_keys(display, p->owner_window, use_x_input_api, FALSE);
	}
	else
	{
		if (config->tracking_mouse)
			grab_button(display, TRUE);
		grab_all_keys(display, p->owner_window, use_x_input_api, TRUE);
	}

	/*
	if (mode == LISTEN_DONTGRAB_INPUT)
	{
		log_message (DEBUG, _("Interception of events in the window (ID %d) with name '%s' OFF"), p->owner_window, owner_window_name);

		// Event unmasking
		grab_button(display, p->owner_window, FALSE);
		grab_all_keys(display, p->owner_window, use_x_input_api, FALSE);
	}
	else
	{
		log_message (DEBUG, _("Interception of events in the window (ID %d) with name '%s' ON"), p->owner_window, owner_window_name);

		// Event masking
		// Grabbing key and button
		if (p->last_focus != FOCUS_EXCLUDED)
		{
			if (config->tracking_mouse)
			  grab_button(display, p->parent_window, TRUE);
			grab_all_keys(display, p->owner_window, use_x_input_api, TRUE);
		}
		else
		{
			grab_button(display, p->owner_window, FALSE);
			grab_all_keys(display, p->owner_window, use_x_input_api, FALSE);
		}
	}
	*/

	p->last_parent_window = p->parent_window;

	if (owner_window_name != NULL)
		free(owner_window_name);
}

static void focus_uninit(struct _focus *p)
{
	if (p != NULL)
		free(p);

	log_message(DEBUG, _("Focus is freed"));
}

struct _focus* focus_init(void)
{
	struct _focus *p = (struct _focus *) malloc(sizeof(struct _focus));
	bzero(p, sizeof(struct _focus));

	// Functions mapping
	p->get_focus_status	= focus_get_focus_status;
	p->get_focused_window = focus_get_focused_window;
	p->update_grab_events	= focus_update_grab_events;
	p->uninit		= focus_uninit;

	return p;
}
