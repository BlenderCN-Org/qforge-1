/*
	menu.c

	Menu support code and interface to QC

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2002/1/18

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>
#include <string.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/csqc.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/ruamoko.h"
#include "QF/sys.h"
#include "QF/view.h"

typedef struct menu_pic_s {
	struct menu_pic_s *next;
	int         x, y;
	int         srcx, srcy, width, height;
	const char *name;
} menu_pic_t;

typedef struct menu_item_s {
	struct menu_item_s *parent;
	struct menu_item_s **items;
	int         num_items;
	int         max_items;
	int         cur_item;
	int         x, y;
	func_t      func;
	func_t      cursor;
	func_t      keyevent;
	func_t      draw;
	func_t      enter_hook;
	func_t      leave_hook;
	unsigned    fadescreen:1;
	unsigned    allkeys:1;
	const char *text;
	menu_pic_t *pics;
} menu_item_t;

static cvar_t  *confirm_quit;

static progs_t  menu_pr_state;
static menu_item_t *menu;
static hashtab_t *menu_hash;
static func_t   menu_init;
static func_t   menu_quit;
static func_t   menu_draw_hud;
static const char *top_menu;

static int
menu_resolve_globals (progs_t *pr)
{
	const char *sym;
	ddef_t     *def;
	dfunction_t *f;

	if (!(f = PR_FindFunction (pr, sym = "menu_init")))
		goto error;
	menu_init = (func_t)(f - menu_pr_state.pr_functions);
	if (!(f = PR_FindFunction (pr, sym = "menu_draw_hud")))
		goto error;
	menu_draw_hud = (func_t)(f - pr->pr_functions);
	if (!(def = PR_FindGlobal (pr, sym = "time")))
		goto error;
	menu_pr_state.globals.time = &G_FLOAT (pr, def->ofs);
	return 1;
error:
	Con_Printf ("%s: undefined symbol %s\n", pr->progs_name, sym);
	return 0;
}

static const char *
menu_get_key (void *m, void *unused)
{
	return ((menu_item_t *)m)->text;
}

static void
menu_free (void *_m, void *unused)
{
	int         i;
	menu_item_t *m = (menu_item_t *)_m;

	if (m->text)
		free ((char*)m->text);
	if (m->parent) {
		// remove self from parent list to avoid double frees
		for (i = 0; i < m->parent->num_items; i++)
			if (m->parent->items[i] == m)
				m->parent->items[i] = 0;
	}
	if (m->items) {
		for (i = 0; i < m->num_items; i++)
			if (m->items[i]) {
				m->items[i]->parent = 0;
				if (m->items[i]->text)
					Hash_Del (menu_hash, m->items[i]->text);
				menu_free (m->items[i], 0);
			}
		free (m->items);
	}
	while (m->pics) {
		menu_pic_t *p = m->pics;
		m->pics = p->next;
		if (p->name)
			free ((char*)p->name);
		free (p);
	}
	free (m);
}

static void
menu_add_item (menu_item_t *m, menu_item_t *i)
{
	if (m->num_items == m->max_items) {
		m->items = realloc (m->items,
							(m->max_items + 8) * sizeof (menu_item_t *));
		m->max_items += 8;
	}
	i->parent = m;
	m->items[m->num_items++] = i;
}

static void
menu_pic (int x, int y, const char *name,
		  int srcx, int srcy, int width, int height)
{
	menu_pic_t *pic = malloc (sizeof (menu_pic_t));

	pic->x = x;
	pic->y = y;
	pic->name = strdup (name);
	pic->srcx = srcx;
	pic->srcy = srcy;
	pic->width = width;
	pic->height = height;

	pic->next = menu->pics;
	menu->pics = pic;
}

static void
bi_Menu_Begin (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	menu_item_t *m = calloc (sizeof (menu_item_t), 1);

	m->x = x;
	m->y = y;
	m->text = text && text[0] ? strdup (text) : 0;
	if (menu)
		menu_add_item (menu, m);
	menu = m;
	if (m->text)
		Hash_Add (menu_hash, m);
}

static void
bi_Menu_FadeScreen (progs_t *pr)
{
	menu->fadescreen = P_INT (pr, 0);
}

static void
bi_Menu_Draw (progs_t *pr)
{
	menu->draw = P_FUNCTION (pr, 0);
}

static void
bi_Menu_EnterHook (progs_t *pr)
{
	menu->enter_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_LeaveHook (progs_t *pr)
{
	menu->leave_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_Pic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);

	menu_pic (x, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_SubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	menu_pic (x, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_CenterPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	qpic_t     *qpic = Draw_CachePic (name, 1);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_CenterSubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	qpic_t     *qpic = Draw_CachePic (name, 1);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_Item (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	func_t      func = P_FUNCTION (pr, 3);
	int         allkeys = P_INT (pr, 4);
	menu_item_t *mi = calloc (sizeof (menu_item_t), 1);

	mi->x = x;
	mi->y = y;
	mi->text = text && text[0] ? strdup (text) : 0;
	mi->func = func;
	mi->parent = menu;
	mi->allkeys = allkeys;
	menu_add_item (menu, mi);
}

static void
bi_Menu_Cursor (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu->cursor = func;
}

static void
bi_Menu_KeyEvent (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu->keyevent = func;
}

static void
bi_Menu_End (progs_t *pr)
{
	menu = menu->parent;
}

static void
bi_Menu_TopMenu (progs_t *pr)
{
	const char *name = P_GSTRING (pr, 0);

	if (top_menu)
		free ((char*)top_menu);
	top_menu = strdup (name);
}

static void
bi_Menu_SelectMenu (progs_t *pr)
{
	const char *name = P_GSTRING (pr, 0);

	menu = 0;
	if (name && *name)
		menu = Hash_Find (menu_hash, name);
	if (menu) {
		key_dest = key_menu;
		game_target = IMT_CONSOLE;
		if (menu->enter_hook) {
			PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
		}
	} else {
		if (name && *name)
			Con_Printf ("no menu \"%s\"\n", name);
		if (con_data.force_commandline) {
			key_dest = key_console;
			game_target = IMT_CONSOLE;
		} else {
			key_dest = key_game;
			game_target = IMT_0;
		}
	}
}

static void
bi_Menu_SetQuit (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu_quit = func;
}

static void
bi_Menu_Quit (progs_t *pr)
{
	if (con_data.quit)
		con_data.quit ();
	Sys_Quit ();
}

static void
bi_Menu_GetIndex (progs_t *pr)
{
	if (menu) {
		R_INT (pr) = menu->cur_item;
	} else {
		R_INT (pr) = -1;
	}
}

static void
togglemenu_f (void)
{
	if (menu)
		Menu_Leave ();
	else
		Menu_Enter ();
}

static void
quit_f (void)
{
	if (confirm_quit->int_val && menu_quit) {
		PR_ExecuteProgram (&menu_pr_state, menu_quit);
		if (!R_INT (&menu_pr_state))
			return;
	}
	bi_Menu_Quit (&menu_pr_state);
}

static void *
menu_allocate_progs_mem (progs_t *pr, int size)
{
	return malloc (size);
}

static void
menu_free_progs_mem (progs_t *pr, void *mem)
{
	free (mem);
}

static void *
menu_load_file (progs_t *pr, const char *path)
{
	return QFS_LoadFile (path, 0);
}

static builtin_t builtins[] = {
	{"Menu_Begin",			bi_Menu_Begin,			-1},
	{"Menu_FadeScreen",		bi_Menu_FadeScreen,		-1},
	{"Menu_Draw",			bi_Menu_Draw,			-1},
	{"Menu_EnterHook",		bi_Menu_EnterHook,		-1},
	{"Menu_LeaveHook",		bi_Menu_LeaveHook,		-1},
	{"Menu_Pic",			bi_Menu_Pic,			-1},
	{"Menu_SubPic",			bi_Menu_SubPic,			-1},
	{"Menu_CenterPic",		bi_Menu_CenterPic,		-1},
	{"Menu_CenterSubPic",	bi_Menu_CenterSubPic,	-1},
	{"Menu_Item",			bi_Menu_Item,			-1},
	{"Menu_Cursor",			bi_Menu_Cursor,			-1},
	{"Menu_KeyEvent",		bi_Menu_KeyEvent,		-1},
	{"Menu_End",			bi_Menu_End,			-1},
	{"Menu_TopMenu",		bi_Menu_TopMenu,		-1},
	{"Menu_SelectMenu",		bi_Menu_SelectMenu,		-1},
	{"Menu_SetQuit",		bi_Menu_SetQuit,		-1},
	{"Menu_Quit",			bi_Menu_Quit,			-1},
	{"Menu_GetIndex",		bi_Menu_GetIndex,		-1},
	{0},
};

void
Menu_Init (void)
{
	menu_pr_state.progs_name = "menu.dat";
	menu_pr_state.allocate_progs_mem = menu_allocate_progs_mem;
	menu_pr_state.free_progs_mem = menu_free_progs_mem;
	menu_pr_state.load_file = menu_load_file;
	menu_pr_state.resolve = menu_resolve_globals;

	menu_hash = Hash_NewTable (61, menu_get_key, menu_free, 0);

	PR_RegisterBuiltins (&menu_pr_state, builtins);

	RUA_Init (&menu_pr_state, 1);

	InputLine_Progs_Init (&menu_pr_state);
	Key_Progs_Init (&menu_pr_state);
	GIB_Progs_Init (&menu_pr_state);
	PR_Cmds_Init (&menu_pr_state);
	R_Progs_Init (&menu_pr_state);

	confirm_quit = Cvar_Get ("confirm_quit", "1", CVAR_ARCHIVE, NULL,
							 "confirm quit command");

	Cmd_AddCommand ("togglemenu", togglemenu_f,
					"Toggle the display of the menu");
	Cmd_AddCommand ("quit", quit_f, "Exit the program");
}

void
Menu_Load (void)
{
	int         size;
	QFile      *file;

	Hash_FlushTable (menu_hash);
	menu = 0;
	top_menu = 0;

	menu_pr_state.progs = 0;
	if ((size = QFS_FOpenFile (menu_pr_state.progs_name, &file)) != -1) {
		PR_LoadProgsFile (&menu_pr_state, file, size, 0, 256 * 1024);
		Qclose (file);

		if (!PR_RunLoadFuncs (&menu_pr_state)) {
			free (menu_pr_state.progs);
			menu_pr_state.progs = 0;
		}
	}
	if (!menu_pr_state.progs) {
		// Not a fatal error, just means no menus
		Con_SetOrMask (0x80);
		Con_Printf ("Menu_Load: could not load %s\n",
					menu_pr_state.progs_name);
		Con_SetOrMask (0x00);
		return;
	}
	RUA_Cbuf_SetCbuf (&menu_pr_state, con_data.cbuf);
	InputLine_Progs_SetDraw (&menu_pr_state, C_DrawInputLine);
	PR_ExecuteProgram (&menu_pr_state, menu_init);
}

void
Menu_Draw (view_t *view)
{
	menu_pic_t *m_pic;
	int         i, x, y;
	menu_item_t *item;

	if (!menu)
		return;

	x = view->xabs;
	y = view->yabs;

	if (menu->fadescreen)
		Draw_FadeScreen ();

	*menu_pr_state.globals.time = *con_data.realtime;

	if (menu->draw) {
		P_INT (&menu_pr_state, 0) = x;
		P_INT (&menu_pr_state, 1) = y;
		PR_ExecuteProgram (&menu_pr_state, menu->draw);
		if (R_INT (&menu_pr_state))
			return;
	}


	for (m_pic = menu->pics; m_pic; m_pic = m_pic->next) {
		qpic_t     *pic = Draw_CachePic (m_pic->name, 1);
		if (!pic)
			continue;
		if (m_pic->width > 0 && m_pic->height > 0)
			Draw_SubPic (x + m_pic->x, y + m_pic->y, pic,
						 m_pic->srcx, m_pic->srcy,
						 m_pic->width, m_pic->height);
		else
			Draw_Pic (x + m_pic->x, y + m_pic->y, pic);
	}
	for (i = 0; i < menu->num_items; i++) {
		if (menu->items[i]->text) {
			Draw_String (x + menu->items[i]->x + 8, y + menu->items[i]->y,
						 menu->items[i]->text);
		}
	}
	if (!menu->items)
		return;
	item = menu->items[menu->cur_item];
	if (menu->cursor) {
		P_INT (&menu_pr_state, 0) = x + item->x;
		P_INT (&menu_pr_state, 1) = y + item->y;
		PR_ExecuteProgram (&menu_pr_state, menu->cursor);
	} else {
		Draw_Character (x + item->x, y + item->y,
						12 + ((int) (*con_data.realtime * 4) & 1));
	}
}

void
Menu_Draw_Hud (view_t *view)
{
	*menu_pr_state.globals.time = *con_data.realtime;

	PR_ExecuteProgram (&menu_pr_state, menu_draw_hud);
}

void
Menu_KeyEvent (knum_t key, short unicode, qboolean down)
{
	menu_item_t *item;

	if (!menu)
		return;
	if (menu->keyevent) {
		P_INT (&menu_pr_state, 0) = key;
		P_INT (&menu_pr_state, 1) = unicode;
		P_INT (&menu_pr_state, 2) = down;
		PR_ExecuteProgram (&menu_pr_state, menu->keyevent);
		if (R_INT (&menu_pr_state))
			return;
	} else if (menu->items && menu->items[menu->cur_item]->func
			   && menu->items[menu->cur_item]->allkeys) {
		PR_PushFrame (&menu_pr_state);
		item = menu->items[menu->cur_item];
		P_STRING (&menu_pr_state, 0) = PR_SetTempString (&menu_pr_state,
														 item->text);
		P_INT (&menu_pr_state, 1) = key;
		PR_ExecuteProgram (&menu_pr_state, item->func);
		PR_PopFrame (&menu_pr_state);
		if (R_INT (&menu_pr_state))
			return;
	}
	if (!menu || !menu->items)
		return;
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			menu->cur_item++;
			menu->cur_item %= menu->num_items;
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			menu->cur_item += menu->num_items - 1;
			menu->cur_item %= menu->num_items;
			break;
		case QFK_RETURN:
		case QFM_BUTTON1:
			{
				item = menu->items[menu->cur_item];
				if (item->func) {
					PR_PushFrame (&menu_pr_state);
					P_STRING (&menu_pr_state, 0) =
						PR_SetTempString (&menu_pr_state, item->text);
					P_INT (&menu_pr_state, 1) = key;
					PR_ExecuteProgram (&menu_pr_state, item->func);
					PR_PopFrame (&menu_pr_state);
				} else {
					menu = item;
					if (menu->enter_hook) {
						PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
					}
				}
			}
			break;
		default:
			break;
	}
}

void
Menu_Enter ()
{
	if (!top_menu) {
		key_dest = key_console;
		game_target = IMT_CONSOLE;
		return;
	}
	key_dest = key_menu;
	game_target = IMT_CONSOLE;
	menu = Hash_Find (menu_hash, top_menu);
	if (menu && menu->enter_hook) {
		PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
	}
}

void
Menu_Leave ()
{
	if (menu) {
		if (menu->leave_hook) {
			PR_ExecuteProgram (&menu_pr_state, menu->leave_hook);
		}
		menu = menu->parent;
		if (!menu) {
			if (con_data.force_commandline) {
				key_dest = key_console;
				game_target = IMT_CONSOLE;
			} else {
				key_dest = key_game;
				game_target = IMT_0;
			}
		}
	}
}
