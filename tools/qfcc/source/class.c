/*
	function.c

	QC function support code

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/7

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

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pr_obj.h"
#include "QF/va.h"

#include "qfcc.h"

#include "class.h"
#include "method.h"
#include "struct.h"
#include "type.h"

static hashtab_t *class_hash;
static hashtab_t *category_hash;
static hashtab_t *protocol_hash;

static const char *
class_get_key (void *class, void *unused)
{
	return ((class_t *) class)->class_name;
}

static const char *
protocol_get_key (void *protocol, void *unused)
{
	return ((protocol_t *) protocol)->name;
}

class_t *
get_class (const char *name, int create)
{
	class_t    *c;

	if (!class_hash)
		class_hash = Hash_NewTable (1021, class_get_key, 0, 0);
	if (name) {
		c = Hash_Find (class_hash, name);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->class_name = name;
	if (name)
		Hash_Add (class_hash, c);
	return c;
}

void
class_add_methods (class_t *class, methodlist_t *methods)
{
	if (!methods)
		return;
	if (!class->methods)
		class->methods = new_methodlist ();
	*class->methods->tail = methods->head;
	class->methods->tail = methods->tail;
	free (methods);
}

void
class_add_protocol_methods (class_t *class, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0);
	if (!class->methods)
		class->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (class->methods, p->methods);
	}
}

void
class_add_protocol (class_t *class, protocol_t *protocol)
{
	if (!class->protocols)
		class->protocols = new_protocollist ();
	add_protocol (class->protocols, protocol);
}

void
class_finish (class_t *class)
{
	if (class->def)
		return;
	if (class->class_name && class->category_name) {
		def_t      *category_def;
		pr_category_t *category;

		category_def = PR_GetDef (type_category,
								  va ("_OBJ_CATEGORY_%s_%s",
									  class->class_name,
									  class->category_name),
								  0, &numpr_globals);
		category = &G_STRUCT (pr_category_t, category_def->ofs);
	} else if (class->class_name) {
		def_t      *meta_def;
		pr_class_t *meta;
		pr_class_t *cls;
		
		meta_def = PR_GetDef (type_Class.aux_type,
							  va ("_OBJ_METACLASS_%s", class->class_name),
							  0, &numpr_globals);
		meta = &G_STRUCT (pr_class_t, meta_def->ofs);
		memset (meta, 0, sizeof (*meta));
		meta->class_pointer  = ReuseString (class->class_name);
		meta->name = meta->class_pointer;
		meta->instance_size = type_size (type_Class.aux_type);
		meta->ivars = emit_struct (type_Class.aux_type, "Class");
		meta->methods = emit_methods (class->methods, class->class_name, 0);
		meta->protocols = emit_protocol_list (class->protocols,
											  class->class_name);

		class->def = PR_GetDef (type_Class.aux_type,
								va ("_OBJ_METACLASS_%s", class->class_name),
								0, &numpr_globals);
		cls = &G_STRUCT (pr_class_t, class->def->ofs);
		cls->class_pointer = meta_def->ofs;
		if (class->super_class)
			cls->super_class = class->super_class->def->ofs;
		cls->name = meta->name;
		cls->instance_size = type_size (class->ivars);
		cls->ivars = emit_struct (class->ivars, class->class_name);
		cls->methods = emit_methods (class->methods, class->class_name, 1);
		cls->protocols = meta->protocols;
	}
}

method_t *
class_find_method (class_t *class, method_t *method)
{
	method_t   *m;
	dstring_t  *sel;

	for (m = class->methods->head; m; m = m->next)
		if (method_compare (method, m))
			return m;
	sel = dstring_newstr ();
	selector_name (sel, (keywordarg_t *)method->selector);
	warning (0, "method %s not in %s%s", sel->str, class->class_name,
			 class->category_name ? va (" (%s)", class->category_name) : "");
	dstring_delete (sel);
	return method;
}

static unsigned long
category_get_hash (void *_c, void *unused)
{
	class_t    *c = (class_t *) _c;
	return Hash_String (c->class_name) ^ Hash_String (c->category_name);
}

static int
category_compare (void *_c1, void *_c2, void *unused)
{
	class_t    *c1 = (class_t *) _c1;
	class_t    *c2 = (class_t *) _c2;
	return strcmp (c1->class_name, c2->class_name) == 0
		   && strcmp (c1->category_name, c2->category_name) == 0;
}

void
class_check_ivars (class_t *class, struct type_s *ivars)
{
	if (!struct_compare_fields (class->ivars, ivars))
		warning (0, "instance variable missmatch for %s", class->class_name);
	class->ivars = ivars;
}

class_t *
get_category (const char *class_name, const char *category_name, int create)
{
	class_t    *c;

	if (!category_hash) {
		category_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (category_hash,
							 category_get_hash, category_compare);
	}
	if (class_name && category_name) {
		class_t     _c = {0, class_name, category_name};

		c = Hash_FindElement (category_hash, &_c);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->class_name = class_name;
	c->class_name = category_name;
	if (class_name && category_name)
		Hash_AddElement (category_hash, c);
	return c;
}

def_t *
class_def (class_t *class)
{
	return PR_GetDef (&type_Class, class->class_name, 0, &numpr_globals);
}

protocol_t *
get_protocol (const char *name, int create)
{
	protocol_t    *p;

	if (name) {
		p = Hash_Find (protocol_hash, name);
		if (p || !create)
			return p;
	}

	p = calloc (sizeof (protocol_t), 1);
	p->name = name;
	if (name)
		Hash_Add (protocol_hash, p);
	return p;
}

void
protocol_add_methods (protocol_t *protocol, methodlist_t *methods)
{
	if (!methods)
		return;
	if (!protocol->methods)
		protocol->methods = new_methodlist ();
	*protocol->methods->tail = methods->head;
	protocol->methods->tail = methods->tail;
	free (methods);
}

void
protocol_add_protocol_methods (protocol_t *protocol, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol->methods)
		protocol->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (protocol->methods, p->methods);
	}
}

def_t *
protocol_def (protocol_t *protocol)
{
	return PR_GetDef (&type_Protocol, protocol->name, 0, &numpr_globals);
}

protocollist_t *
new_protocollist (void)
{
	protocollist_t *protocollist = malloc (sizeof (protocollist_t));

	protocollist->count = 0;
	protocollist->list = 0;
	return protocollist;
}

void
add_protocol (protocollist_t *protocollist, protocol_t *protocol)
{
	protocollist->list = realloc (protocollist->list,
								  (protocollist->count + 1)
								  * sizeof (protocollist_t));
	protocollist->list[protocollist->count++] = protocol;
}

int
emit_protocol (protocol_t *protocol)
{
	def_t      *proto_def;
	pr_protocol_t *proto;

	proto_def = PR_GetDef (type_Protocol.aux_type,
						   va ("_OBJ_PROTOCOL_%s", protocol->name),
						   0, &numpr_globals);
	proto = &G_STRUCT (pr_protocol_t, proto_def->ofs);
	proto->class_pointer = 0;
	proto->protocol_name = ReuseString (protocol->name);
	proto->protocol_list =
		emit_protocol_list (protocol->protocols,
							va ("PROTOCOL_%s", protocol->name));
	proto->instance_methods = emit_methods (protocol->methods,
											protocol->name, 1);
	proto->class_methods = emit_methods (protocol->methods, protocol->name, 0);
	return proto_def->ofs;
}

int
emit_protocol_list (protocollist_t *protocols, const char *name)
{
	def_t      *proto_list_def;
	type_t     *protocol_list;
	pr_protocol_list_t *proto_list;
	int         i;

	if (!protocols)
		return 0;
	protocol_list = new_struct (0);
	new_struct_field (protocol_list, &type_pointer, "next", vis_public);
	new_struct_field (protocol_list, &type_integer, "count", vis_public);
	for (i = 0; i < protocols->count; i++)
		new_struct_field (protocol_list, &type_pointer, 0, vis_public);
	proto_list_def = PR_GetDef (type_Protocol.aux_type,
								va ("_OBJ_PROTOCOLS_%s", name),
								0, &numpr_globals);
	proto_list = &G_STRUCT (pr_protocol_list_t, proto_list_def->ofs);
	proto_list->next = 0;
	proto_list->count = protocols->count;
	for (i = 0; i < protocols->count; i++)
		proto_list->list[i] = emit_protocol (protocols->list[i]);
	return proto_list_def->ofs;
}
