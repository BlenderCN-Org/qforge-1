/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.

	$Id$
*/

#ifndef qfbsp_draw_h
#define qfbsp_draw_h

#include "QF/mathlib.h"

struct visfacet_s;
struct portal_s;
struct node_s;
struct brush_s;
struct winding_s;

void Draw_ClearBounds (void);
void Draw_AddToBounds (vec3_t v);
void Draw_DrawFace (struct visfacet_s *f);
void Draw_ClearWindow (void);
void Draw_SetRed (void);
void Draw_SetGrey (void);
void Draw_SetBlack (void);
void DrawPoint (vec3_t v);

void Draw_SetColor (int c);
void SetColor (int c);
void DrawPortal (struct portal_s *p);
void DrawLeaf (struct node_s *l, int color);
void DrawBrush (struct brush_s *b);

void DrawWinding (struct winding_s *w);
void DrawTri (vec3_t p1, vec3_t p2, vec3_t p3);

#endif//qfbsp_draw_h
