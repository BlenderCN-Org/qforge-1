/*
	cmd.c

	script command processing module

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	const char *name;
	const char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

cvar_t     *cmd_warncmd;

static hashtab_t  *cmd_alias_hash;
static hashtab_t  *cmd_hash;

cbuf_args_t *cmd_args;
static cbuf_t *cmd_cbuf;
cmd_source_t cmd_source;


/* Command parsing functions */

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	const char *name;
	xcommand_t  function;
	const char *description;
	qboolean    pure;
} cmd_function_t;

static cmd_function_t *cmd_functions;	// possible commands to execute

int
Cmd_Argc (void)
{
	return cmd_args->argc;
}

const char *
Cmd_Argv (int arg)
{
	if (arg >= cmd_args->argc)
		return "";
	return cmd_args->argv[arg]->str;
}

const char *
Cmd_Args (int start)
{
	if (start >= cmd_args->argc)
		return "";
	return cmd_args->args[start];
}

int
Cmd_Command (cbuf_args_t *args)
{
	cmd_function_t *cmd;

	cmd_args = args;
	//cmd_source = src;

	if (!args->argc)
		return 0;							// no tokens

	// check functions
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, args->argv[0]->str);
	if (cmd) {
		if (cmd->function) {
			cmd->function ();
		}
		return 0;
	}
	// check cvars
	if (Cvar_Command ())
		return 0;
	if (cbuf_active->strict)
		return -1;
	else if (cmd_warncmd->int_val || developer->int_val)
		Sys_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
	return 0;
}

/* Registers a command and handler function */
int
Cmd_AddCommand (const char *cmd_name, xcommand_t function,
				const char *description)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	// fail if the command already exists
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		Sys_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return 0;
	}

	cmd = calloc (1, sizeof (cmd_function_t));
	SYS_CHECKMEM (cmd);
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->description = description;
	Hash_Add (cmd_hash, cmd);
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if (strcmp ((*c)->name, cmd->name) >= 0)
			break;
	cmd->next = *c;
	*c = cmd;
	return 1;
}

/* Unregisters a command */
int
Cmd_RemoveCommand (const char *name)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	cmd = (cmd_function_t *) Hash_Del (cmd_hash, name);
	if (!cmd)
		return 0;
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if ((*c)->next == cmd)
			break;
	(*c)->next = cmd->next;
	free (cmd);
	return 1;
}

/* Checks for the existance of a command */
qboolean
Cmd_Exists (const char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		return true;
	}

	return false;
}


/* Command completion functions */

const char *
Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t *cmd;
	int         len;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcasecmp (partial, cmd->name))
			return cmd->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			return cmd->name;

	return NULL;
}

/*
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
int
Cmd_CompleteCountPossible (const char *partial)
{
	cmd_function_t *cmd;
	int         len;
	int         h;

	h = 0;
	len = strlen (partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
const char **
Cmd_CompleteBuildList (const char *partial)
{
	cmd_function_t *cmd;
	int         len = 0;
	int         bpos = 0;
	int         sizeofbuf;
	const char **buf;

	sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	len = strlen (partial);
	buf = malloc (sizeofbuf + sizeof (char *));

	SYS_CHECKMEM (buf);
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/* Hash table functions for aliases and commands */
static void
cmd_alias_free (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	free ((char *) a->name);
	free ((char *) a->value);
	free (a);
}

static const char *
cmd_alias_get_key (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	return a->name;
}

static const char *
cmd_get_key (void *c, void *unused)
{
	cmd_function_t *cmd = (cmd_function_t *) c;

	return cmd->name;
}

static void
Cmd_Runalias_f (void)
{
	cmdalias_t *a;

	a = (cmdalias_t *) Hash_Find (cmd_alias_hash, Cmd_Argv (0));

	if (a) {
		Cbuf_InsertText (cbuf_active, a->value);
		return;
	} else {
		Sys_Printf
			("BUG: No alias found for registered command.  Please report this to the QuakeForge development team.");
	}
}

static void
Cmd_Alias_f (void)
{
	cmdalias_t *alias;
	char       *cmd;
	int         i, c;
	const char *s;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("Current alias commands:\n");
		for (alias = cmd_alias; alias; alias = alias->next)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}

	s = Cmd_Argv (1);
	// if the alias already exists, reuse it
	alias = (cmdalias_t *) Hash_Find (cmd_alias_hash, s);
	if (Cmd_Argc () == 2) {
		if (alias)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}
	if (alias)
		free ((char *) alias->value);
	else if (!Cmd_Exists (s)) {
		cmdalias_t **a;

		alias = calloc (1, sizeof (cmdalias_t));
		SYS_CHECKMEM (alias);
		alias->name = strdup (s);
		Hash_Add (cmd_alias_hash, alias);
		for (a = &cmd_alias; *a; a = &(*a)->next)
			if (strcmp ((*a)->name, alias->name) >= 0)
				break;
		alias->next = *a;
		*a = alias;
		Cmd_AddCommand (alias->name, Cmd_Runalias_f, "Alias command.");
	} else {
		Sys_Printf ("alias: a command with the name \"%s\" already exists.\n", s);
		return;
	}
	// copy the rest of the command line
	cmd = malloc (strlen (Cmd_Args (1)) + 2);	// can never be longer
	SYS_CHECKMEM (cmd);
	cmd[0] = 0;							// start out with a null string
	c = Cmd_Argc ();
	for (i = 2; i < c; i++) {
		strcat (cmd, Cmd_Argv (i));
		if (i != c - 1)
			strcat (cmd, " ");
	}

	alias->value = cmd;
}

static void
Cmd_UnAlias_f (void)
{
	cmdalias_t *alias;
	const char *s;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv (1);
	alias = Hash_Del (cmd_alias_hash, s);

	if (alias) {
		cmdalias_t **a;

		Cmd_RemoveCommand (alias->name);

		for (a = &cmd_alias; *a != alias; a = &(*a)->next)
			;
		*a = alias->next;

		free ((char *) alias->name);
		free ((char *) alias->value);
		free (alias);
	} else {
		Sys_Printf ("Unknown alias \"%s\"\n", s);
	}
}

static void
Cmd_CmdList_f (void)
{
	cmd_function_t *cmd;
	int         i;
	int         show_description = 0;

	if (Cmd_Argc () > 1)
		show_description = 1;
	for (cmd = cmd_functions, i = 0; cmd; cmd = cmd->next, i++) {
		if (show_description) {
			Sys_Printf ("%-20s :\n%s\n", cmd->name, cmd->description);
		} else {
			Sys_Printf ("%s\n", cmd->name);
		}
	}

	Sys_Printf ("------------\n%d commands\n", i);
}

static void
Cmd_Help_f (void)
{
	const char *name;
	cvar_t     *var;
	cmd_function_t *cmd;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: help <cvar/command>\n");
		return;
	}

	name = Cmd_Argv (1);

	for (cmd = cmd_functions; cmd && strcasecmp (name, cmd->name);
		 cmd = cmd->next);
	if (cmd) {
		Sys_Printf ("%s\n", cmd->description);
		return;
	}

	var = Cvar_FindVar (name);
	if (!var)
		var = Cvar_FindAlias (name);
	if (var) {
		Sys_Printf ("%s\n", var->description);
		return;
	}

	Sys_Printf ("variable/command not found\n");
}
static void
Cmd_Exec_f (void)
{
	char       *f;
	int         mark;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark ();
	f = (char *) QFS_LoadHunkFile (Cmd_Argv (1));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command ()
		&& (cmd_warncmd->int_val || (developer && developer->int_val)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));
	Cbuf_InsertText (cbuf_active, f);
	Hunk_FreeToLowMark (mark);
}

/*
	Cmd_Echo_f

	Just prints the rest of the line to the console
*/
static void
Cmd_Echo_f (void)
{
	if (Cmd_Argc () == 2)
		Sys_Printf ("%s\n", Cmd_Argv (1));
	else
		Sys_Printf ("%s\n", Cmd_Args (1));
}

/* Pauses execution of the current stack until
next call of Cmd_Execute (usually next frame) */
static void
Cmd_Wait_f (void)
{
	cbuf_active->state = CBUF_STATE_WAIT;
}

/* Pauses execution for a specified number
of seconds */
static void
Cmd_Sleep_f (void)
{
	double waittime;
	cbuf_t *p;
	cbuf_active->state = CBUF_STATE_WAIT;
	waittime = atof (Cmd_Argv (1));
	for (p = cbuf_active; p->up; p = p->up); // Get to top of stack
	p->resumetime = Sys_DoubleTime() + waittime;
}

void
Cmd_StuffCmds (cbuf_t *cbuf)
{
	int         i, j;
	int         s;
	char       *build, c;
	char       *cmdline;

	s = strlen (com_cmdline);
	if (!s)
		return;

	cmdline = strdup (com_cmdline);
	// pull out the commands
	build = malloc (s + 1);
	SYS_CHECKMEM (build);
	build[0] = 0;

	for (i = 0; i < s - 1; i++) {
		if (cmdline[i] == '+') {
			i++;

			for (j = i; !((cmdline[j] == '+')
						  || (cmdline[j] == '-'
							  && (j == 0 || cmdline[j - 1] == ' '))
						  || (cmdline[j] == 0)); j++);

			c = cmdline[j];
			cmdline[j] = 0;

			strncat (build, cmdline + i, s - strlen (build));
			strncat (build, "\n", s - strlen (build));
			cmdline[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText (cbuf, build);

	free (build);
	free (cmdline);
}

static void
Cmd_StuffCmds_f (void)
{
	Cmd_StuffCmds (cbuf_active);
}

void
Cmd_Init_Hash (void)
{
	cmd_hash = Hash_NewTable (1021, cmd_get_key, 0, 0);
	cmd_alias_hash = Hash_NewTable (1021, cmd_alias_get_key, cmd_alias_free, 0);
}

void
Cmd_Init (void)
{
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f, "Execute the commands given "
					"at startup again");
	Cmd_AddCommand ("alias", Cmd_Alias_f, "Used to create a reference to a "
					"command or list of commands.\n"
					"When used without parameters, displays all current "
					"aliases.\n"
					"Note: Enclose multiple commands within quotes and "
					"seperate each command with a semi-colon.");
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "Remove the selected alias");
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f, "List all commands");
	Cmd_AddCommand ("help", Cmd_Help_f, "Display help for a command or "
					"variable");
	Cmd_AddCommand ("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddCommand ("echo", Cmd_Echo_f, "Print text to console");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "Wait a game tic");
	Cmd_AddCommand ("sleep", Cmd_Sleep_f, "Wait for a certain number of seconds.");
	cmd_warncmd = Cvar_Get ("cmd_warncmd", "0", CVAR_NONE, NULL, "Toggles the "
							"display of error messages for unknown commands");
	cmd_cbuf = Cbuf_New (&id_interp);
}

int
Cmd_ExecuteString (const char *text, cmd_source_t src)
{
	cmd_source = src;
	COM_TokenizeString (text, cmd_cbuf->args);
	Cmd_Command (cmd_cbuf->args);
	return 0;
}

void
Cmd_Exec_File (cbuf_t *cbuf, const char *path, int qfs)
{
	char       *f;
	int         len;
	QFile      *file;

	if (!path || !*path)
		return;
	if (qfs) {
		QFS_FOpenFile (path, &file);
	} else {
		file = Qopen (path, "r");
	}
	if (file) {
		len = Qfilesize (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Cbuf_InsertText (cbuf, f);
			free (f);
		}
		Qclose (file);
	}
}

