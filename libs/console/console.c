/*
	console.c

	console api

	Copyright (C) 2001       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/16

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/sys.h"

int         con_linewidth;				// characters across screen

plugin_t *con_module;

static con_buffer_t *(*const buffer) (size_t, int) = Con_CreateBuffer;
static void (*const complete)(inputline_t *) = Con_BasicCompleteCommandLine;
static inputline_t *(*const create)(int, int, char) = Con_CreateInputLine;
static void (*const display)(const char **, int) = Con_DisplayList;

void
Con_Init (const char *plugin_name)
{
	con_module = PI_LoadPlugin ("console", plugin_name);
	if (con_module) {
		con_module->functions->general->p_Init ();
		Sys_SetPrintf (con_module->functions->console->pC_Print);
	} else {
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
	}
}

void
Con_Init_Cvars (void)
{
}

void
Con_Shutdown (void)
{
	if (con_module) {
		con_module->functions->general->p_Shutdown ();
		PI_UnloadPlugin (con_module);
	}
}

void
Con_Printf (const char *fmt, ...)
{
	va_list		args;

	va_start (args, fmt);
	if (con_module)
		con_module->functions->console->pC_Print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
	va_end (args);
}

void
Con_Print (const char *fmt, va_list args)
{
	if (con_module)
		con_module->functions->console->pC_Print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
}

void
Con_DPrintf (const char *fmt, ...)
{
	if (developer && developer->int_val) {
		va_list     args;
		va_start (args, fmt);
		if (con_module)
			con_module->functions->console->pC_Print (fmt, args);
		else
			vfprintf (stdout, fmt, args);
		va_end (args);
	}
}

void
Con_ProcessInput (void)
{
	if (con_module) {
		con_module->functions->console->pC_ProcessInput ();
	} else {
		static int  been_there_done_that = 0;

		if (!been_there_done_that) {
			been_there_done_that = 1;
			Con_Printf ("no input for you\n");
		}
	}
}

void
Con_KeyEvent (knum_t key, short unicode, qboolean down)
{
	if (con_module)
		con_module->functions->console->pC_KeyEvent (key, unicode, down);
}

void
Con_SetOrMask (int mask)
{
	if (con_module)
		con_module->data->console->ormask = mask;
}

void
Con_DrawConsole (int lines)
{
	if (con_module)
		con_module->functions->console->pC_DrawConsole (lines);
}

void
Con_CheckResize (void)
{
	if (con_module)
		con_module->functions->console->pC_CheckResize ();
}

void
Con_NewMap (void)
{
	if (con_module)
		con_module->functions->console->pC_NewMap ();
}
