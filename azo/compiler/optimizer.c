#define __AZO_OPTIMIZER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/


#define debug_optimizer 0
#define debug_literals 0
#define debug_calculate 0
#define debug_references 0

typedef struct _AZOOptimizer AZOOptimizer;

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <arikkei/arikkei-strlib.h>

#include <az/class.h>
#include <az/complex.h>
#include <az/field.h>
#include <az/function.h>
#include <az/string.h>
#include <az/primitives.h>
#include <az/classes/value-array.h>

#include <azo/keyword.h>
#include <azo/compiler/optimizer.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/variable.h>

#define noVERBOSE

#ifdef VERBOSE
#define DBG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DBG_REPLACE(...) describe(stdout, __VA_ARGS__)
#else
#define DBG_PRINTF(...)
#define DBG_REPLACE(S, args...)
#endif

static int optimize_node(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags);

void
azo_optimizer_setup(AZOOptimizer *opt, AZOCompiler *comp)
{
	opt->comp = comp;
}

void
azo_optimizer_release(AZOOptimizer *opt)
{
	opt->comp = NULL;
}

/**
 * @brief Collect all variable references that change value in given node
 * 
 * We mark all assigns, ++ and -- operations.
 * Skip function bodies because these create a new frame.
 * 
 * @param opt The optimizer
 * @param node The node to analyze
 * @param vars Existing list of variables
 * @return Updated list of variables 
 */
static AZOVariableList *
tag_assigns(AZOOptimizer *opt, AZONode *node, AZOVariableList *vars)
{
	switch (node->term.type) {
		case AZO_TERM_FUNCTION:
			/* Return type has to be already resolved to constant class */
			if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_STATIC_OLD)) {
				AZONode *ret = node->children;
				AZONode *args = ret->next;
				AZONode *body = args->next;
				vars = tag_assigns(opt, args, vars);
				break;
			} else if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER_OLD)) {
				AZONode *ret = node->children;
				AZONode *this = ret->next;
				AZONode *args = this->next;
				AZONode *body = args->next;
				vars = tag_assigns(opt, this, vars);
				vars = tag_assigns(opt, args, vars);
				break;
			} else if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_LAMBDA)) {
				AZONode *ret = node->children;
				AZONode *args = ret->next;
				AZONode *body = args->next;
				vars = tag_assigns(opt, args, vars);
				break;
			} else {
				fprintf(stderr, "tag_assigns: Invalid function subtype %u\n", node->term.subtype);
				break;
			}
		case AZO_TERM_PREFIX:
			if ((node->term.subtype != AZO_TERM_PREFIX_INCREMENT) && (node->term.subtype != AZO_TERM_PREFIX_DECREMENT)) {
				for (AZONode *child = node->children; child; child = child->next) {
					vars = tag_assigns(opt, child, vars);
				}
				break;
			}
			/* fall through */
		case AZO_TERM_SUFFIX: {
			AZONode *ref = node->children;
			if (ref->term.type == AZO_TERM_VARIABLE) {
				fprintf(stderr, "tag_assigns: %s is trashed by ++/--\n", ref->value.v.string->str);
				vars = azo_var_list_set(vars, ref->value.v.string, ref->var_pos, NULL);
			}
			break;
		}
		case AZO_TERM_ASSIGN: {
			for (AZONode *ref = node->children; ref->next; ref = ref->next) {
				if (ref->term.type == AZO_TERM_VARIABLE) {
					fprintf(stderr, "tag_assigns: %s is trashed by assign\n", ref->value.v.string->str);
					vars = azo_var_list_set(vars, ref->value.v.string, ref->var_pos, NULL);
				}
			}
			break;
		}
		default:
			for (AZONode *child = node->children; child; child = child->next) {
				vars = tag_assigns(opt, child, vars);
			}
			break;
	}
	return vars;
}

static AZOVariableList *
optimize_const_assign(AZOOptimizer *opt, AZONode *node, AZOVariableList *vars)
{
	switch(node->term.type) {
		case AZO_TERM_KEYWORD:
			if (node->term.subtype == AZO_KEYWORD_FOR) {
#if 1
				AZONode *init = node->children;
				AZONode *cond = init->next;
				AZONode *step = cond->next;
				AZONode *body = step->next;

				/* Process init with parent list */
				vars = optimize_const_assign(opt, init, vars);

				/* Changed list of cond, step and body */
				AZOVariableList *trashed = tag_assigns(opt, cond, NULL);
				trashed = tag_assigns(opt, step, trashed);
				trashed = tag_assigns(opt, body, trashed);

				/* Remove trashed from vars */
				vars = azo_var_list_remove_all(vars, trashed);

				/* Process body for local const-ness */
				vars = optimize_const_assign(opt, body, vars);
				/* fixme: In theory step can contain local const-ness but it is really a corner-case */

				/* Remove again in case they were added */
				vars = azo_var_list_remove_all(vars, trashed);
				azo_var_list_free(trashed);
#endif
			} else if (node->term.subtype == AZO_KEYWORD_WHILE) {
#if 1
				AZONode *cond = node->children;
				AZONode *body = cond->next;

				/* Changed list of cond and body */
				AZOVariableList *trashed = tag_assigns(opt, cond, NULL);
				trashed = tag_assigns(opt, body, trashed);

				/* Remove trashed from vars */
				vars = azo_var_list_remove_all(vars, trashed);

				/* Process body for local const-ness */
				vars = optimize_const_assign(opt, body, vars);
				/* fixme: In theory step can contain local const-ness but it is really a corner-case */

				/* Remove again in case they were added */
				vars = azo_var_list_remove_all(vars, trashed);
				azo_var_list_free(trashed);
#endif
			} else if (node->term.subtype == AZO_KEYWORD_DO) {
#if 1
				AZONode *block = node->children;
				AZONode *cond = block->next;
				
				AZOVariableList *trashed = tag_assigns(opt, block, NULL);
				trashed = tag_assigns(opt, cond, trashed);

				/* Remove trashed from vars */
				vars = azo_var_list_remove_all(vars, trashed);

				/* Process body for local const-ness */
				vars = optimize_const_assign(opt, block, vars);
				/* fixme: In theory step can contain local const-ness but it is really a corner-case */

				/* Remove again in case they were added */
				vars = azo_var_list_remove_all(vars, trashed);
				azo_var_list_free(trashed);
#endif
			} else if (node->term.subtype == AZO_KEYWORD_IF) {
#if 1
				AZONode *cond = node->children;
				AZONode *if_true = cond->next;
				AZONode *if_false = if_true->next;
				
				/* Process init with parent list */
				vars = optimize_const_assign(opt, cond, vars);

				AZOVariableList *trashed = tag_assigns(opt, if_true, NULL);
				if (if_false) trashed = tag_assigns(opt, if_false, trashed);

				/* Process both bodies with copies of parent list */
				AZOVariableList *copy = azo_var_list_duplicate(vars);
				copy = optimize_const_assign(opt, if_true, copy);
				azo_var_list_free(copy);
				if (if_false) {
					copy = azo_var_list_duplicate(vars);
					copy = optimize_const_assign(opt, if_false, copy);
					azo_var_list_free(copy);
				}

				/* Remove trashed from the original */
				vars = azo_var_list_remove_all(vars, trashed);
				azo_var_list_free(trashed);
#endif
			} else {
				for (AZONode *child = node->children; child; child = child->next) {
					vars = optimize_const_assign(opt, child, vars);
				}
			}
			break;
		case AZO_TERM_DECLARATION: {
			/* If const add to list, if not const remove from list (overwriting can only happen in function body) */
			AZONode *name = node->children;
			/* Variable declaration is the only place where variable reference survives */
			assert(AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
			AZONode *init = name->next;
			if (init && init->term.type == AZO_TERM_CONSTANT) {
				vars = azo_var_list_set(vars, name->value.v.string, name->var_pos, init);
			} else {
				vars = azo_var_list_remove(vars, name->value.v.string);
			}
			break;
		}
		case AZO_TERM_FUNCTION: {
#if 1
			/* Proceed args with existing list, duplicate list and proceed body */
			/* Return type has to be already resolved to constant class */
			AZONode *ret, *this, *args, *body;
			if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_STATIC_OLD)) {
				ret = node->children;
				this = NULL;
				args = ret->next;
				body = args->next;
				break;
			} else if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER_OLD)) {
				ret = node->children;
				this = ret->next;
				args = this->next;
				body = args->next;
				break;
			} else if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_LAMBDA)) {
				ret = node->children;
				this = NULL;
				args = ret->next;
				body = args->next;
				break;
			} else {
				fprintf(stderr, "optimize_const_assign: Invalid function subtype %u\n", node->term.subtype);
				break;
			}
			if (this) vars = optimize_const_assign(opt, this, vars);
			vars = optimize_const_assign(opt, args, vars);
			AZOVariableList *dupl = azo_var_list_duplicate(vars);
			dupl = optimize_const_assign(opt, body, dupl);
			azo_var_list_free(dupl);
			break;
#endif
		}
		case AZO_TERM_SUFFIX:
#if 1
			/* Remove from list */
			/* fixme: Could calculate value */
			if (node->children->term.type == AZO_TERM_VARIABLE) {
				vars = azo_var_list_remove(vars, node->children->value.v.string);
			} else {
				vars = optimize_const_assign(opt, node->children, vars);
			}
#endif
			break;
		case AZO_TERM_PREFIX:
#if 1
			if ((node->term.subtype == AZO_TERM_PREFIX_INCREMENT) || (node->term.subtype == AZO_TERM_PREFIX_DECREMENT)) {
				/* Remove from list */
				/* fixme: Could calculate value */
				if (node->children->term.type == AZO_TERM_VARIABLE) {
					vars = azo_var_list_remove(vars, node->children->value.v.string);
				} else {
					vars = optimize_const_assign(opt, node->children, vars);
				}
			}
#endif
			break;
		case AZO_TERM_ASSIGN: {
			/* Optimize variable list, except DO NOT replace LValue references */
			//azo_node_print_info(node, stderr, opt->comp->src, 0);
			AZONode *value = NULL;
			for (AZONode *child = node->children; child; child = child->next) {
				if (child->next) {
					if (child->term.type == AZO_TERM_VARIABLE) continue;
					vars = optimize_const_assign(opt, child, vars);
				} else {
					vars = optimize_const_assign(opt, child, vars);
					value = child;
				}
			}
			//azo_node_print_info(value, stderr, opt->comp->src, 0);
			for (AZONode *child = node->children; child->next; child = child->next) {
				if (value->term.type == AZO_TERM_CONSTANT) {
					if (child->term.type != AZO_TERM_VARIABLE) continue;
					vars = azo_var_list_set(vars, child->value.v.string, child->var_pos, value);
				} else {
					if (child->term.type != AZO_TERM_VARIABLE) continue;
					vars = azo_var_list_remove(vars, child->value.v.string);
				}
			}
			//azo_node_print_info(node, stderr, opt->comp->src, 0);
			break;
		}
		case AZO_TERM_VARIABLE:
			/* If const, replace */
			if (AZO_NODE_IS(node, AZO_TERM_VARIABLE, AZO_TERM_VARIABLE_LOCAL)) {
				AZOVariableList *v = azo_var_list_find(vars, node->value.v.string);
				if (v) {
					//if (!strcmp((const char *) node->value.v.string->str, "all")) break;
					fprintf(stderr, "optimize_const_assign: %s is constant\n", node->value.v.string->str);
					node->term.type = AZO_TERM_CONSTANT;
					node->term.subtype = AZ_IMPL_TYPE(v->var.const_node->value.impl);
					az_packed_value_copy(&node->value, &v->var.const_node->value);
					opt->n_const_subst += 1;
				}
			}
			break;
		default:
			for (AZONode *child = node->children; child; child = child->next) {
				vars = optimize_const_assign(opt, child, vars);
			}
			break;
	}
	return vars;
}

#define noDEBUG_MEMBER_INST

/*
 * REFERENCE_PROPERTY
 *   AZO_TERM_REFERENCE_VARIABLE | AZO_TERM_REFERENCE_PROPERTY | CONSTANT
 *   AZO_TERM_REFERENCE_MEMBER
 */

static int
optimize_attribute (AZOOptimizer *opt, AZONode *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str, unsigned int flags)
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

static int
optimize_property(AZOOptimizer *opt, AZONode *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str, unsigned int flags)
{
	/**
	 * @brief Try to get property from parent
	 * 
	 * REFERENCE -> CONSTANT
	 * 
	 */
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
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
	}
	// fixme: For now we support dot as an attribute but it should be removed
	return optimize_attribute(opt, expr, klass, impl, inst, str, flags);
}

static int
optimize_chain(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	while (node) {
		int result = optimize_node(opt, ctx, node, flags);
		if (result) return result;
		node = node->next;
	}
	return 0;
}

static int
optimize_children(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	return optimize_chain(opt, ctx, node->children, flags);
}

static int
optimize_program(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	return optimize_chain(opt, ctx, node->children->next, flags);
}

static int
optimize_block(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, ctx, node, flags);
}

static int
optimize_group(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, ctx, node, flags);
}

static int
optimize_keyword(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	if (node->term.subtype == AZO_KEYWORD_THIS) {
		if (ctx->this_node && (ctx->this_node->term.type == AZO_TERM_CONSTANT)) {
			node->term.type = AZO_TERM_CONSTANT;
			node->term.subtype = ctx->this_node->term.subtype;
			az_packed_value_copy(&node->value, &ctx->this_node->value);
			fprintf(stderr, "resolve_member: Replaced 'this' with ");
			azo_node_print(node, stderr);
			fprintf(stderr, "\n");
			unsigned int first, last;
			if (azo_source_find_line_range(opt->comp->src, node->term.start, node->term.end, &first, &last)) {
				azo_source_print_lines(opt->comp->src, first, last + 1, stderr);
			}
			opt->n_const_subst += 1;
		}
	}
	return 0;
}

static int
optimize_declaration_list(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	int result = optimize_node(opt, ctx, node->children, flags);
	if (result) return result;
	return optimize_chain(opt, ctx, node->children->next, flags);
}

static int
optimize_declaration(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *name = node->children;
	assert(name->term.subtype == AZO_TERM_REFERENCE_VARIABLE);
	if (name->next) {
		int result = optimize_node(opt, ctx, name->next, flags);
		if (result) return result;
	}
	return 0;
}

static int
optimize_argument_declaration(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *type = node->children;
	AZONode *name = type->next;
	int result = optimize_node(opt, ctx, type, flags);
	if (result) return result;
	assert(AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
	return 0;
}

static int
optimize_function(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	if (node->term.subtype == AZO_TERM_FUNCTION_MEMBER_OLD) {
		AZONode *type = node->children;
		int result = optimize_node(opt, ctx, type, flags);
		if (result) return result;
		AZONode *obj = type->next;
		result = optimize_node(opt, ctx, obj, flags);
		if (result) return result;
		AZONode *args = obj->next;
		result = optimize_node(opt, ctx, args, flags);
		if (result) return result;
		AZONode *body = args->next;
		// fixme: Think out the frame/context management
		azo_compiler_set_frame(opt->comp, opt->comp->frames[node->frame]);
		AZOOptimizerCtx new_ctx = *ctx;
		new_ctx.frame = opt->comp->frames[node->frame];
		result = optimize_node(opt, &new_ctx, body, flags);
		azo_compiler_set_frame(opt->comp, ctx->frame);
		if (result) return result;
	} else {
		AZONode *type = node->children;
		int result = optimize_node(opt, ctx, type, flags);
		if (result) return result;
		AZONode *args = type->next;
		result = optimize_node(opt, ctx, args, flags);
		if (result) return result;
		AZONode *body = args->next;
		// fixme: Think out the frame/context management
		azo_compiler_set_frame(opt->comp, opt->comp->frames[node->frame]);
		AZOOptimizerCtx new_ctx = *ctx;
		new_ctx.frame = opt->comp->frames[node->frame];
		result = optimize_node(opt, &new_ctx, body, flags);
		azo_compiler_set_frame(opt->comp, ctx->frame);
		if (result) return result;
	}
	return 0;
}

static int
optimize_function_call(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	assert (ref != NULL);
	AZONode *args = ref->next;
	assert(args != NULL);
	int result = optimize_node(opt, ctx, ref, flags);
	if (result) return result;
	result = optimize_node(opt, ctx, args, flags);
	if (result) return result;

	/* If reference is already constant we have nothing to do */
	if (ref->term.type == AZO_TERM_CONSTANT) return 0;
	/* If reference is variable we cannot optimize */
	if (ref->term.type == AZO_TERM_VARIABLE) return 0;

	/* Test if arguments list is constant */
	unsigned int n_args = 0;
	unsigned int arg_types[32];
	for (AZONode *child = args->children; child; child = child->next) {
		if (child->term.type != AZO_TERM_CONSTANT) return 0;
		arg_types[n_args] = child->term.subtype;
		n_args += 1;
		if (n_args >= 32) return 0;
	}
	/* All arguments are constants */
	/* Find klass/impl/inst */
	const AZClass *klass;
	const AZImplementation *impl;
	void *inst;
	AZString *str;
	if (ref->term.subtype == AZO_TERM_REFERENCE_PROPERTY) {
		AZONode *parent, *member;
		parent = ref->children;
		member = parent->next;
		if (parent->term.type != AZO_TERM_CONSTANT) return 0;
		assert(AZO_NODE_IS(member, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER));
		klass = az_type_get_class (parent->term.subtype);
		impl = parent->value.impl;
		inst = az_value_get_inst(parent->value.impl, &parent->value.v);
		str = member->value.v.string;
	} else {
		fprintf (stderr, "azo_compiler_resolve_function_call: unknown reference subtype\n");
		return 1;
	}
	/* Lookup function */
	AZFunctionSignature *sig = az_function_signature_new(AZ_CLASS_TYPE(klass), AZ_TYPE_ANY, n_args, arg_types);
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	int idx = az_class_lookup_function (klass, impl, inst, str, sig, &def_class, &def_impl, &def_inst);
	az_function_signature_delete (sig);
	if (idx < 0) return 0;
	AZField *field = &def_class->props_self[idx];
	if (!AZ_FIELD_IS_FINAL(field)) return 0;
	assert(AZ_FIELD_IS_FUNCTION(field));
	/* Found a final function with proper signature */
	const AZImplementation *prop_impl;
	AZValue64 prop_val;
	if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &prop_impl, &prop_val.value, 64, NULL)) {
		fprintf (stderr, "azo_compiler_resolve_function_call: Property %s is not readable\n", str->str);
		return 1;
	}
	if (prop_impl == NULL) {
		fprintf(stderr, "optimize_function_call: Property %s is final but missing implementation\n", str->str);
		return 1;
	}
#if 1
	az_packed_value_set_from_impl_value (&ref->value, prop_impl, &prop_val.value);
	ref->term.type = AZO_TERM_CONSTANT;
	ref->term.subtype = AZ_IMPL_TYPE(prop_impl);
	az_value_clear (prop_impl, &prop_val.value);
#ifdef DEBUG_RESOLVE_FUNCTION_CALL
	fprintf (stderr, "azo_compiler_resolve_function_call: Replaced final function %s with constant\n", str->str);
#endif
	// fixme: This is not nice but we keep parent for now as this implementation for call

	//while (ref->children) {
	//	child = ref->children;
	//	expr->children = child->next;
	//	azo_node_free_tree (child);
	//}
#endif
	return 0;

	return 0;
}

static int
optimize_array_element(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	assert(ref != NULL);
	int result = optimize_node(opt, ctx, ref, flags);
	if (result) return result;
	AZONode *idx = ref->next;
	assert(idx != NULL);
	result = optimize_node(opt, ctx, idx, flags);
	if (result) return result;
	return 0;
}

static int
optimize_list(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, ctx, node, flags);
}

static int
optimize_reference(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	/* REFERENCE_VARIBLE has to be resolved to CONSTANT, VARIABLE, REFERENCE_PROPERTY or REFERENCE_ATTRIBUTE */
	/* REFERENCE_MEMBER is never seen alone */
	assert((node->term.subtype == AZO_TERM_REFERENCE_PROPERTY) || (node->term.subtype == AZO_TERM_REFERENCE_ATTRIBUTE));
	AZONode *parent = node->children;
	AZONode *member = parent->next;
	assert(AZO_NODE_IS(member, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER));
	int result = optimize_node(opt, ctx, parent, flags);
	if (result) return result;
	if (parent->term.type == AZO_TERM_CONSTANT) {
		void *inst;
		const AZImplementation *impl = az_packed_value_get_inst_autobox(&parent->value, &inst);
		if (AZO_NODE_IS(node, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_PROPERTY)) {
			return optimize_property (opt, node, AZ_CLASS_FROM_IMPL(impl), impl, inst, member->value.v.string, flags);
		} else if (AZO_NODE_IS(node, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_ATTRIBUTE)) {
			return optimize_attribute(opt, node, AZ_CLASS_FROM_IMPL(impl), impl, inst, member->value.v.string, flags);
		}
	}
	return 0;
}

static int
optimize_literal_array(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	int result = optimize_children(opt, ctx, node, flags);
	if (result) return result;

	unsigned int size = 0;
	for (AZONode *child = node->children; child; child = child->next) {
		if (child->term.type != AZO_TERM_CONSTANT) return 0;
		size += 1;
	}
	AZValueArray *va = az_value_array_new(size);
	unsigned int idx = 0;
	for (AZONode *child = node->children; child; child = child->next) {
		az_value_array_set_element_from_val (va, idx, child->value.impl, &child->value.v);
		idx += 1;
	}
	azo_node_clear_children(node);
	node->term.type = AZO_TERM_CONSTANT;
	node->term.subtype = AZ_TYPE_VALUE_ARRAY;
	az_packed_value_transfer_reference (&node->value, AZ_TYPE_VALUE_ARRAY, &va->reference);
	opt->n_const_subst += 1;
	return 0;
}

static int
optimize_cast(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	assert(node->children);
	assert(node->children->term.type == AZO_TERM_TYPE);
	AZONode *expr = node->children->next;
	int result = optimize_node(opt, ctx, expr, flags);
	if (result) return result;
	return 0;
}

static int
optimize_suffix(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_prefix(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	if ((node->term.subtype != AZO_TERM_PREFIX_INCREMENT) && (node->term.subtype != AZO_TERM_PREFIX_DECREMENT)) {
		if ((node->term.type == AZO_TERM_CONSTANT) && (flags & AZO_OPTIMIZER_FLAG_CALC_CONST_EXPRESSIONS)) {
			return azo_compiler_calculate_rvalue_prefix(opt, node);
		}
	}
	return 0;
}

static int
optimize_binary(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *lhs = node->children;
	assert(lhs);
	AZONode *rhs = lhs->next;
	assert(rhs);
	int result = optimize_node(opt, ctx, lhs, flags);
	if (result) return result;
	result = optimize_node(opt, ctx, rhs, flags);
	if (result) return result;
	if ((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT) && (flags & AZO_OPTIMIZER_FLAG_CALC_CONST_EXPRESSIONS)) {
		/* Calculate result */
		return azo_compiler_calculate_constant_binary(opt, node);
	}
	return 0;
}

static int
optimize_comparison(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_assign(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	AZONode *val = ref->next;
	return optimize_node(opt, ctx, val, flags);
}

static int
optimize_test(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_select(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	int result = optimize_children(opt, ctx, node, flags);
	if (result) return result;
	AZONode *cond = node->children;
	AZONode *if_true = cond->next;
	AZONode *if_false = if_true->next;
	assert(cond && if_true && if_false);
	assert(!if_false->next);
	if ((cond->term.type == AZO_TERM_CONSTANT) && (flags & AZO_OPTIMIZER_FLAG_CALC_CONST_EXPRESSIONS)) {
		if (cond->value.impl && (AZ_IMPL_TYPE(cond->value.impl) == AZ_TYPE_BOOLEAN)) {
			az_packed_value_clear(&node->value);
			if (cond->value.v.boolean_v) {
				*node = *if_true;
				azo_node_free(if_true);
				azo_node_free_tree(if_false);
			} else {
				*node = *if_false;
				azo_node_free(if_false);
				azo_node_free_tree(if_true);
			}
			azo_node_free_tree(cond);
			opt->n_const_subst += 1;
			return 0;
		} else {
			fprintf(stderr, "optimize_select: condition is not a constant boolean\n");
			return 1;
		}
	}
	return 0;
}

static int
optimize_constant(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_variable(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_type(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_node(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node, unsigned int flags)
{
	int result = 0;
	switch (node->term.type) {
		case AZO_TERM_INVALID:
			fprintf(stderr, "optimize_node: type = INVALID\n");
			return 1;
		case AZO_TERM_EMPTY:
			return 0;
		case AZO_TERM_CONTEXT: {
			AZONode *this_node = node->children;
			unsigned int lresult = optimize_node(opt, ctx, this_node, flags);
			if (lresult) result = 1;
			AZOResolveCtx lctx = *ctx;
			lctx.this_node = this_node;
			lresult = optimize_chain(opt, &lctx, this_node->next, flags);
			if (lresult) result = 1;
			return result;
		}
		case AZO_TERM_PROGRAM:
			return optimize_program(opt, ctx, node, flags);
		case AZO_TERM_BLOCK:
			return optimize_block(opt, ctx, node, flags);
		case AZO_TERM_STATEMENT_GROUP:
			return optimize_group(opt, ctx, node, flags);
		case AZO_TERM_KEYWORD:
			return optimize_keyword(opt, ctx, node, flags);
		case AZO_TERM_DECLARATION_LIST:
			return optimize_declaration_list(opt, ctx, node, flags);
		case AZO_TERM_DECLARATION:
			return optimize_declaration(opt, ctx, node, flags);
		case AZO_TERM_ARGUMENT_DECLARATION:
			return optimize_argument_declaration(opt, ctx, node, flags);
		case AZO_TERM_FUNCTION:
			return optimize_function(opt, ctx, node, flags);
		case AZO_TERM_FUNCTION_CALL:
			return optimize_function_call(opt, ctx, node, flags);
		case AZO_TERM_ARRAY_ELEMENT:
			return optimize_array_element(opt, ctx, node, flags);
		case AZO_TERM_LIST:
			return optimize_list(opt, ctx, node, flags);
		case AZO_TERM_REFERENCE:
			return optimize_reference(opt, ctx, node, flags);
		case AZO_TERM_LITERAL_ARRAY:
			return optimize_literal_array(opt, ctx, node, flags);
		case AZO_TERM_CAST:
			return optimize_cast(opt, ctx, node, flags);
		case AZO_TERM_SUFFIX:
			return optimize_suffix(opt, node, flags);
		case AZO_TERM_PREFIX:
			return optimize_prefix(opt, node, flags);
		case AZO_TERM_BINARY:
			return optimize_binary(opt, ctx, node, flags);
		case AZO_TERM_COMPARISON:
			return optimize_comparison(opt, node, flags);
		case AZO_TERM_ASSIGN:
			return optimize_assign(opt, ctx, node, flags);
		case AZO_TERM_TEST:
			return optimize_test(opt, ctx, node, flags);
		case AZO_TERM_SELECT:
			return optimize_select(opt, ctx, node, flags);
		case  AZO_TERM_CONSTANT:
			return optimize_constant(opt, node, flags);
		case AZO_TERM_VARIABLE:
			return optimize_variable(opt, node, flags);
		case AZO_TERM_TYPE:
			return optimize_type(opt, node, flags);
		default:
			return 1;
	}
	return 0;
}

int
azo_compiler_optimize_program(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *root, unsigned int flags)
{
	assert(AZO_NODE_IS(root, AZO_TERM_PROGRAM, AZO_TERM_GENERIC));
	unsigned int iter = 0;
	do {
		fprintf(stderr, "---- Optimizer iteration %d ------\n", iter++);
		opt->n_const_subst = 0;
		int result = optimize_chain(opt, ctx, root->children, flags);
		if (result) return result;
		// fprintf(stderr, "----------before--------------\n");
		//azo_node_print_info(root, stderr, opt->comp->src, 0);
		AZOVariableList *vars = optimize_const_assign(opt, root, NULL);
		//fprintf(stderr, "----------after---------------\n");
		//azo_node_print_info(root, stderr, opt->comp->src, 0);
		azo_var_list_free(vars);
		fprintf(stderr, "---- Num substitutions %d ------\n", opt->n_const_subst);
	} while (opt->n_const_subst > 0);
	return 0;
}
