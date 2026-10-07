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

static const AZImplementation *
resolve_type_chain(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node, AZValue *val)
{
	if (AZO_NODE_IS(node, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE)) {
		const AZImplementation *impl = azo_context_lookup (comp->globals, node->value.v.string, val, AZ_VALUE_MAX_SIZE);
		if (!impl) {
			fprintf(stderr, "resolve_type_chain: Global variable %s is not defined\n", node->value.v.string->str);
		}
		return impl;
	} else if (AZO_NODE_IS(node, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_PROPERTY)) {
		AZONode *parent = node->children;
		AZONode *member = parent->next;
		AZValue parent_val;
		const AZImplementation *parent_impl = resolve_type_chain(comp, rctx, parent, &parent_val);
		if (!parent_impl) return NULL;
		void *adict_inst;
		const AZImplementation *adict_impl = az_instance_get_interface(parent_impl, az_value_get_inst(parent_impl, &parent_val), AZ_TYPE_ATTRIBUTE_DICT, &adict_inst);
		if (!adict_impl) {
			fprintf(stderr, "resolve_type_chain: parent is not attribute dictionary\n");
			az_value_clear(parent_impl, &parent_val);
			return NULL;
		}
		unsigned int adict_flags;
		const AZImplementation *impl = az_attrib_dict_lookup((const AZAttribDictImplementation *) adict_impl, (AZAttribDict *) adict_inst, member->value.v.string, val, AZ_PACKED_VALUE_MAX_SIZE, &adict_flags);
		az_value_clear(parent_impl, &parent_val);
		if (!impl) {
			fprintf(stderr, "resolve_type_chain: property %s is not defined\n", member->value.v.string->str);
		}
		return impl;
	}
	fprintf(stderr, "resolve_type_chain: node type not supported\n");
	return NULL;
}

int
azo_compiler_resolve_type_expression(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZValue val;
	const AZImplementation *impl = resolve_type_chain(comp, rctx, node, &val);
	if (!impl) return 1;
	if (AZ_IMPL_TYPE(impl) != AZ_TYPE_CLASS) {
		fprintf(stderr, "resolve_type_chain: Expression is not a class type\n");
		az_value_clear(impl, &val);
		return 1;
	}

	node->term.type = AZO_TERM_TYPE;
	node->term.subtype = AZ_CLASS_TYPE((AZClass *) val.block);
	az_packed_value_clear(&node->value);

	azo_node_clear_children(node);
	az_value_clear(impl, &val);

	return 0;
}

/*
 * REFERENCE_PROPERTY
 *   AZO_TERM_REFERENCE_VARIABLE | AZO_TERM_REFERENCE_PROPERTY | CONSTANT
 *   AZO_TERM_REFERENCE_MEMBER
 */

static int
resolve_attribute (AZOFrame *frame, AZOResolveCtx *rctx, AZONode *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str)
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
resolve_attribute_reference (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *parent, *member;
	parent = expr->children;
	unsigned int result = azo_compiler_resolve_node (comp, rctx, parent);
	if (result) return result;
	member = parent->next;
	result = azo_compiler_resolve_node (comp, rctx, member);
	if (result) return result;
	if (parent->term.type == AZO_TERM_CONSTANT) {
		if (AZO_NODE_IS(member, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER)) {
			void *inst;
			const AZImplementation *impl = az_packed_value_get_inst_autobox(&parent->value, &inst);
			return resolve_attribute (rctx->frame, rctx, expr, AZ_CLASS_FROM_IMPL(impl), impl, inst, member->value.v.string);
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
resolve_property (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *parent, *member;
	parent = expr->children;
	unsigned int result = azo_compiler_resolve_node (comp, rctx, parent);
	if (result) return result;
	member = parent->next;
	return azo_compiler_resolve_node (comp, rctx, member);
}

#define DEBUG_RESOLVE_VARIABLE

static unsigned int
resolve_variable (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
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
	val.impl = azo_context_lookup (comp->globals, expr->value.v.string, &val.v, AZ_PACKED_VALUE_MAX_SIZE);
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
	AZOVariable *var = azo_frame_lookup_local_var (rctx->frame, expr->value.v.string);
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
	var = azo_frame_lookup_parent_var (rctx->frame, expr->value.v.string);
	if (var) {
		DBG_PRINTF("resolve_variable: Parent %s at pos %u\n", expr->value.v.string->str, var->pos);
		expr->term.type = AZO_TERM_VARIABLE;
		expr->term.subtype = AZO_TERM_VARIABLE_CAPTURE;
		az_packed_value_clear (&expr->value);
		expr->var_pos = var->pos;
		return 0;
	}
	/*
	 * Look up parent frames and define if needed
	 *
	 * REFERENCE -> VARIABLE, parent
	 */
	if (rctx->frame->parent) {
		/*
		 * The variable was not found neither in local nor already known parent variables
		 * Try chained lookup through all parent frames
		 */
		if (azo_frame_lookup_chained (rctx->frame->parent, expr->value.v.string)) {
			/*
			 * Ensure that variable is defined (as parent) in this and all intermediate frames
			 * so it's value is passed through function calls to current frame
			 */
			var = azo_frame_ensure_variable (rctx->frame, expr->value.v.string);
			assert(var != NULL);
			DBG_PRINTF("resolve_variable: Created parent variable %s at pos %u\n", expr->value.v.string->str, var->pos);
			expr->term.type = AZO_TERM_VARIABLE;
			expr->term.subtype = AZO_TERM_VARIABLE_CAPTURE;
			az_packed_value_clear (&expr->value);
			expr->var_pos = var->pos;
			return 0;
		}
	}
	/* Either attribute or member of this */
	if (rctx->this_variant != AZO_COMPILER_NO_THIS) {
		// fixme: It is either property or attribute, need a special node for this
		AZONode *this_node = azo_node_new(AZO_TERM_KEYWORD, AZO_KEYWORD_THIS, expr->term.start, expr->term.end);
		unsigned int lresult = azo_compiler_resolve_node(comp, rctx, this_node);
		if (lresult) {
			azo_node_free(this_node);
			return lresult;
		}
		AZONode *prop_node = azo_node_new(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER, expr->term.start, expr->term.end);
		az_packed_value_set_string(&prop_node->value, expr->value.v.string);
		expr->term.subtype = AZO_TERM_REFERENCE_PROPERTY_OR_ATTRIBUTE;
		expr->children = this_node;
		this_node->next = prop_node;
		return 0;
	}
	fprintf(stderr, "ERROR: resolve_variable: Not resolved: ");
	azo_source_print_token(comp->src, expr->term.start, expr->term.end, stderr);
	fprintf(stderr, "\n");
	azo_source_print_lines_of_token(comp->src, expr->term.start, expr->term.end, stderr);
	return 1;
}

int
azo_compiler_resolve_reference (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	assert(expr->term.type == AZO_TERM_REFERENCE);
	//fprintf(stderr, "Resolving: ");
	//azo_source_print_token(comp->src, expr->term.start, expr->term.end, stderr);
	//fprintf(stderr, "\n");
	if (expr->term.subtype == AZO_TERM_REFERENCE_VARIABLE) {
		return resolve_variable (comp, rctx, expr);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_PROPERTY) {
		return resolve_property (comp, rctx, expr);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_ATTRIBUTE) {
		return resolve_attribute_reference (comp, rctx, expr);
	} else if (expr->term.subtype == AZO_TERM_REFERENCE_MEMBER) {
		/* No-op */
		return 0;
	}
	assert(0);
	return 1;
}
