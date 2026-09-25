#define __AZO_RESOLVE_REFERENCES_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <az/field.h>
#include <az/function.h>
#include <az/string.h>

#include <az/classes/attrib-dict.h>
#include <azo/compiler/compiler.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/compiler/resolver.h>

#define noVERBOSE

#ifdef VERBOSE
#define DBG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DBG_REPLACE(...) describe(stdout, __VA_ARGS__)
#else
#define DBG_PRINTF(...)
#define DBG_REPLACE(S, args...)
#endif

static void
describe(FILE *ofs, const char *text, const AZString *name, const AZImplementation *impl, AZValue *val)
{
	if (impl) {
		uint8_t buf[1024];
		az_instance_to_string(impl, az_value_get_inst(impl, val), buf, 1024);
		fprintf(ofs, text, name->str, buf);
	} else {
		fprintf(ofs, text, name->str);
	}
}

static unsigned int
resolve_member_class (AZOCompiler *comp, AZONode *member, const AZClass *klass, unsigned int flags)
{
	return 0;
}

static unsigned int
resolve_member_impl (AZOCompiler *comp, AZONode *member, const AZClass *klass, const AZImplementation *impl, unsigned int flags)
{
	return 0;
}

#define noDEBUG_MEMBER_INST

static int
resolve_member_inst (AZOFrame *frame, AZONode *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str, unsigned int flags)
{
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	/**
	 * @brief Try to get property from parent
	 * 
	 * REFERENCE -> CONSTANT
	 * 
	 */
	int idx = az_class_lookup_property (klass, impl, inst, str, &def_class, &def_impl, &def_inst);
	if (idx >= 0) {
		AZField *field = &def_class->props_self[idx];
		if (!inst && (field->spec == AZ_FIELD_INSTANCE)) return 0;
		if (!impl && (field->spec == AZ_FIELD_IMPLEMENTATION)) return 0;
		if (AZ_FIELD_IS_FINAL(field) && !AZ_FIELD_IS_FUNCTION(field)) {
			az_packed_value_clear(&expr->value);
			if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &expr->value.impl, &expr->value.v, 16, NULL)) {
				fprintf (stderr, "resolve_member: Property %s is not readable\n", str->str);
				return 1;
			}
			expr->term.type = AZO_TERM_CONSTANT;
			/* Final undefined value is not normal but we have to handle it */
			expr->term.subtype = (expr->value.impl) ? AZ_IMPL_TYPE(expr->value.impl) : 0;
			azo_node_clear_children(expr);
			DBG_REPLACE("resolve_member: Replaced final property %s with '%s'\n", str, expr->value.impl, &expr->value.v);
			return 0;
		}
	} else if (inst && az_type_implements(AZ_IMPL_TYPE(impl), AZ_TYPE_ATTRIBUTE_DICT)) {
		/**
		 * @brief Try to get attribute from parent
		 * 
		 * REFERENCE -> CONSTANT
		 * 
		 */
		void *attrd_inst;
		const AZAttribDictImplementation *attrd_impl = (AZAttribDictImplementation *) az_instance_get_interface (impl, inst, AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
		AZValue64 attr_val;
		unsigned int attr_flags;
		const AZImplementation *attr_impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, str, &attr_val.value, 64, &attr_flags);
		if (attr_flags & AZ_ATTRIB_ARRAY_IS_FINAL) {
			az_packed_value_set_from_impl_value (&expr->value, attr_impl, &attr_val.value);
			expr->term.type = AZO_TERM_CONSTANT;
			if (attr_impl) {
				expr->term.subtype = AZ_IMPL_TYPE(attr_impl);
				az_value_clear (attr_impl, &attr_val.value);
			} else {
				// Final undefined value, not normal but we have to handle it
				expr->term.subtype = 0;
			}
			azo_node_clear_children(expr);
			DBG_REPLACE("resolve_member: Replaced final attribute %s with '%s'\n", str, expr->value.impl, &expr->value.v);
			return 0;
		}
	}
	return 0;
}

/*
 * REFERENCE_PROPERTY
 *   AZO_TERM_REFERENCE_VARIABLE | AZO_TERM_REFERENCE_PROPERTY | CONSTANT
 *   AZO_TERM_REFERENCE_MEMBER
 */

static int
resolve_attribute (AZOFrame *frame, AZONode *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str, unsigned int flags)
{
	if (inst && az_type_implements(AZ_IMPL_TYPE(impl), AZ_TYPE_ATTRIBUTE_DICT)) {
		void *attrd_inst;
		const AZAttribDictImplementation *attrd_impl = (AZAttribDictImplementation *) az_instance_get_interface (impl, inst, AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
		AZValue64 attr_val;
		unsigned int attr_flags;
		const AZImplementation *attr_impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, str, &attr_val.value, 64, &attr_flags);
		if (attr_flags & AZ_ATTRIB_ARRAY_IS_FINAL) {
			az_packed_value_set_from_impl_value (&expr->value, attr_impl, &attr_val.value);
			expr->term.type = AZO_TERM_CONSTANT;
			if (attr_impl) {
				expr->term.subtype = AZ_IMPL_TYPE(attr_impl);
				az_value_clear (attr_impl, &attr_val.value);
			} else {
				expr->term.subtype = 0;
			}
			azo_node_clear_children(expr);
			DBG_REPLACE("resolve_attribute: Replaced final attribute %s with '%s'\n", str, expr->value.impl, &expr->value.v);
			return 0;
		}
	}
	return 0;
}

static unsigned int
resolve_attribute_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	AZONode *parent, *member;
	parent = expr->children;
	unsigned int result = azo_compiler_resolve_node (comp, parent, flags);
	if (result) return result;
	member = parent->next;
	result = azo_compiler_resolve_node (comp, member, flags);
	if (result) return result;
	if (parent->term.type == AZO_TERM_CONSTANT) {
		if (AZO_NODE_IS(member, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER)) {
			void *inst;
			const AZImplementation *impl = az_packed_value_get_inst_autobox(&parent->value, &inst);
			return resolve_attribute (comp->current, expr, AZ_CLASS_FROM_IMPL(impl), impl, inst, member->value.v.string, flags);
		}
	}
	return 0;
}

/*
 * REFERENCE_PROPERTY
 *   AZO_TERM_REFERENCE_VARIABLE | AZO_TERM_REFERENCE_PROPERTY | CONSTANT
 *   AZO_TERM_REFERENCE_MEMBER
 */

static unsigned int
resolve_member (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	AZONode *parent, *member;
	parent = expr->children;
	unsigned int result = azo_compiler_resolve_node (comp, parent, flags);
	if (result) return result;
	member = parent->next;
	result = azo_compiler_resolve_node (comp, member, flags);
	if (result) return result;
	if (parent->term.type == AZO_TERM_CONSTANT) {
		if (AZO_NODE_IS(member, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER)) {
			void *inst;
			const AZImplementation *impl = az_packed_value_get_inst_autobox(&parent->value, &inst);
			return resolve_member_inst (comp->current, expr, AZ_CLASS_FROM_IMPL(impl), impl, inst, member->value.v.string, flags);
		}
	}
	return 0;
}

/*
 * AZO_TERM_REFERENCE_VARIABLE
 */

static unsigned int
resolve_this_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	assert(!expr->children);
	const AZClass *klass = AZ_CLASS_FROM_IMPL(comp->current->this_impl);
	int result = resolve_member_inst (comp->current, expr, klass, comp->current->this_impl, comp->current->this_inst, expr->value.v.string, flags);
	if (result) return result;
	if (expr->term.subtype == AZO_TERM_REFERENCE_VARIABLE) {
		/* Did not resolve to constant, replace with this reference */
		AZONode *this_node = azo_node_new(AZO_TERM_KEYWORD, AZO_KEYWORD_THIS, expr->term.start, expr->term.end);
		AZONode *prop_node = azo_node_new(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER, expr->term.start, expr->term.end);
		az_packed_value_set_string(&prop_node->value, expr->value.v.string);
		expr->term.subtype = AZO_TERM_REFERENCE_PROPERTY;
		expr->children = this_node;
		this_node->next = prop_node;
	}
	return 0;
}

#define DEBUG_RESOLVE_VARIABLE

static unsigned int
resolve_variable (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	assert(AZO_NODE_IS(expr, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
	assert (!expr->children);
	/*
	 * Try global context
	 *
	 * REFERENCE -> CONSTANT
	 */
#ifdef DEBUG_RESOLVE_VARIABLE
	AZString *str = expr->value.v.string;
#endif
	AZPackedValue val = {0};
	val.impl = azo_context_lookup (comp->ctx->globals, expr->value.v.string, &val.v, AZ_PACKED_VALUE_MAX_SIZE);
	if (val.impl) {
		expr->term.type = AZO_TERM_CONSTANT;
		expr->term.subtype = AZ_IMPL_TYPE(val.impl);
		az_packed_value_transfer(&expr->value, &val);
		/* If string resolved to global variable we can be sure it has at least reference left */
		DBG_REPLACE("resolve_variable: %s replaced with global object [%s]\n", str, expr->value.impl, &expr->value.v);
		return 0;
	}
	/*
	 * Try local
	 *
	 * REFERENCE -> VARIABLE, local
	 */
	AZOVariable *var = azo_frame_lookup_local_var (comp->current, expr->value.v.string);
	if (var) {
		DBG_PRINTF("resolve_variable: Local %s at pos %u\n", expr->value.v.string->str, var->pos);
		expr->term.type = AZO_TERM_VARIABLE;
		expr->term.subtype = AZO_TERM_VARIABLE_LOCAL;
		az_packed_value_clear (&expr->value);
		expr->var_pos = var->pos;
		return 0;
	}
	/*
	 * Try already defined parent variables in current frame
	 *
	 * REFERENCE -> VARIABLE, parent
	 */
	var = azo_frame_lookup_parent_var (comp->current, expr->value.v.string);
	if (var) {
		DBG_PRINTF("resolve_variable: Parent %s at pos %u\n", expr->value.v.string->str, var->pos);
		expr->term.type = AZO_TERM_VARIABLE;
		expr->term.subtype = AZO_TERM_VARIABLE_PARENT;
		az_packed_value_clear (&expr->value);
		expr->var_pos = var->pos;
		return 0;
	}
	/*
	 * Look up parent frames and define if needed
	 *
	 * REFERENCE -> VARIABLE, parent
	 */
	if (comp->current->parent) {
		/*
		 * The variable was not found neither in local nor already known parent variables
		 * Try chained lookup through all parent frames
		 */
		if (azo_frame_lookup_chained (comp->current->parent, expr->value.v.string)) {
			/*
			 * Ensure that variable is defined (as parent) in this and all intermediate frames
			 * so it's value is passed through function calls to current frame
			 */
			var = azo_frame_ensure_variable (comp->current, expr->value.v.string);
			assert(var != NULL);
			DBG_PRINTF("resolve_variable: Created parent variable %s at pos %u\n", expr->value.v.string->str, var->pos);
			expr->term.type = AZO_TERM_VARIABLE;
			expr->term.subtype = AZO_TERM_VARIABLE_PARENT;
			az_packed_value_clear (&expr->value);
			expr->var_pos = var->pos;
			return 0;
		}
	}
	/* Try this reference */
	if (comp->current->this_impl) {
		return resolve_this_reference (comp, expr, flags);
	}
	fprintf(stderr, "ERROR: resolve_variable: Not resolved: ");
	azo_source_print_token(comp->src, expr->term.start, expr->term.end, stderr);
	fprintf(stderr, "\n");
	azo_source_print_lines_of_token(comp->src, expr->term.start, expr->term.end, stderr);
	return 1;
}

int
azo_compiler_resolve_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	assert(expr->term.type == AZO_TERM_REFERENCE);
	//fprintf(stderr, "Resolving: ");
	//azo_source_print_token(comp->src, expr->term.start, expr->term.end, stderr);
	//fprintf(stderr, "\n");
	if (expr->term.subtype == AZO_TERM_REFERENCE_VARIABLE) {
		return resolve_variable (comp, expr, flags);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_PROPERTY) {
		return resolve_member (comp, expr, flags);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_ATTRIBUTE) {
		return resolve_attribute_reference (comp, expr, flags);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_MEMBER) {
		/* No-op */
		return 0;
	}
	assert(0);
	return 1;
}

unsigned int
azo_compiler_resolve_node_to_class(AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	unsigned int result;
	result = azo_compiler_resolve_node (comp, expr, flags);
	if (result) return result;
	if (expr->term.type != AZO_TERM_CONSTANT) {
		fprintf (stderr, "ERROR: azo_compiler_resolve_node_to_class: reference is not a constant\n");
		return 1;
	}
	if (expr->term.subtype != AZ_TYPE_CLASS) {
		fprintf (stderr, "ERROR: azo_compiler_resolve_node_to_class: reference is not a class\n");
		return 1;
	}
	return 0;
}
