/*
	quakefs.h

	quake virtual filesystem definitions

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

	$Id$
*/

#ifndef __quakefs_h
#define __quakefs_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

//============================================================================

#define	MAX_OSPATH	128		// max length of a filesystem pathname

typedef struct searchpath_s {
	char        filename[MAX_OSPATH];
	struct pack_s *pack;	// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

typedef struct gamedir_s {
	const char *name;
	const char *path;
	const char *gamecode;
	const char *skinpath;
} gamedir_t;

extern searchpath_t *qfs_searchpaths;
extern gamedir_t *qfs_gamedir;

extern struct cvar_s *fs_userpath;
extern struct cvar_s *fs_sharepath;

extern int file_from_pak;
extern int qfs_filesize;

extern char	qfs_gamedir_path[MAX_OSPATH];
extern char	qfs_gamedir_file[MAX_OSPATH];

struct cache_user_s;

char *QFS_CompressPath (const char *pth);
void QFS_WriteFile (const char *filename, void *data, int len);
void QFS_WriteBuffers (const char *filename, int count, ...);

int _QFS_FOpenFile (const char *filename, QFile **gzfile, char *foundname, int zip);
int QFS_FOpenFile (const char *filename, QFile **gzfile);
void QFS_FileBase (const char *in, char *out);
void QFS_DefaultExtension (char *path, const char *extension);
const char *QFS_SkipPath (const char *pathname);
void QFS_StripExtension (const char *in, char *out);
int QFS_NextFilename (char *filename, const char *prefix, const char *ext);
const char *QFS_FileExtension (const char *in);



byte *QFS_LoadFile (const char *path, int usehunk);
byte *QFS_LoadStackFile (const char *path, void *buffer, int bufsize);
byte *QFS_LoadTempFile (const char *path);
byte *QFS_LoadHunkFile (const char *path);
void QFS_LoadCacheFile (const char *path, struct cache_user_s *cu);
void QFS_CreatePath (const char *path);
void QFS_Gamedir (const char *dir);
void QFS_Init (const char *game);
void QFS_CreateGameDirectory (const char *gamename);

#endif // __quakefs_h
