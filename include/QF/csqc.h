/*
	csqc.h

	CSQC prototypes

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2002/1/19

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

#ifndef __QF_csqc_h
#define __QF_csqc_h

void BI_Init (void);

struct progs_s;
struct cbuf_s;
struct inputline_s;

void Cbuf_Progs_Init (struct progs_s *pr);
void Cbuf_Progs_SetCbuf (struct progs_s *pr, struct cbuf_s *cbuf);

void Cmd_Progs_Init (struct progs_s *pr);

void Cvar_Progs_Init (struct progs_s *pr);

void File_Progs_Init (struct progs_s *pr);

void Hash_Progs_Init (struct progs_s *pr);

void InputLine_Progs_Init (struct progs_s *pr);
void InputLine_Progs_SetDraw (struct progs_s *pr,
							  void (*draw)(struct inputline_s*));

void Key_Progs_Init (struct progs_s *pr);

void Plist_Progs_Init (struct progs_s *pr);

#include "QF/quakeio.h"
#define QFILE_MAX_HANDLES 20
typedef struct {
	QFile      *handles[QFILE_MAX_HANDLES];
} qfile_resources_t;

void QFile_Progs_Init (struct progs_s *pr, int secure);
QFile **QFile_AllocHandle (struct progs_s *pr, qfile_resources_t *res);
void QFS_Progs_Init (struct progs_s *pr);

#endif//__QF_csqc_h
