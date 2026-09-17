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
#include <az/string.h>
#include <az/primitives.h>
#include <az/classes/value-array.h>

#include <azo/keyword.h>
#include <azo/compiler/optimizer.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/variable.h>

static int optimize_node(AZOOptimizer *opt, AZONode *node, unsigned int flags);

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
			if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER)) {
				AZONode *ret = node->children;
				AZONode *args = ret->next;
				AZONode *body = args->next;
				vars = tag_assigns(opt, args, vars);
				break;
			} else {
				AZONode *ret = node->children;
				AZONode *this = ret->next;
				AZONode *args = this->next;
				AZONode *body = args->next;
				vars = tag_assigns(opt, this, vars);
				vars = tag_assigns(opt, args, vars);
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
			} else if (node->term.subtype == AZO_KEYWORD_DO) {
#if 1
				AZONode *body = node->children;
				AZONode *cond = body->next;
				
				AZOVariableList *trashed = tag_assigns(opt, body, NULL);
				trashed = tag_assigns(opt, cond, trashed);

				/* Remove trashed from vars */
				vars = azo_var_list_remove_all(vars, trashed);

				/* Process body for local const-ness */
				vars = optimize_const_assign(opt, body, vars);
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
			if (AZO_NODE_IS(node, AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER)) {
				ret = node->children;
				this = NULL;
				args = ret->next;
				body = args->next;
				break;
			} else {
				ret = node->children;
				this = ret->next;
				args = this->next;
				body = args->next;
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

static int
optimize_children(AZOOptimizer *opt, AZONode *children, unsigned int flags)
{
	for (AZONode *child = children; child; child = child->next) {
		int result = optimize_node(opt, child, flags);
		if (result) return result;
	}
	return 0;
}

static int
optimize_program(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, node->children, flags);
}

static int
optimize_block(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, node->children, flags);
}

static int
optimize_group(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, node->children, flags);
}

static int
optimize_keyword(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return 0;
}

static int
optimize_declaration_list(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	int result = optimize_node(opt, node->children, flags);
	if (result) return result;
	return optimize_children(opt, node->children->next, flags);
}

static int
optimize_declaration(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *name = node->children;
	assert(name->term.subtype == AZO_TERM_REFERENCE_VARIABLE);
	if (name->next) {
		int result = optimize_node(opt, name->next, flags);
		if (result) return result;
	}
	return 0;
}

static int
optimize_argument_declaration(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *type = node->children;
	AZONode *name = type->next;
	int result = optimize_node(opt, type, flags);
	if (result) return result;
	assert(AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
	return 0;
}

static int
optimize_function(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	if (node->term.subtype == AZO_TERM_FUNCTION_MEMBER) {
		AZONode *type = node->children;
		int result = optimize_node(opt, type, flags);
		if (result) return result;
		AZONode *obj = type->next;
		result = optimize_node(opt, obj, flags);
		if (result) return result;
		AZONode *args = obj->next;
		result = optimize_node(opt, args, flags);
		if (result) return result;
		AZONode *body = args->next;
		result = optimize_node(opt, body, flags);
		if (result) return result;
	} else {
		AZONode *type = node->children;
		int result = optimize_node(opt, type, flags);
		if (result) return result;
		AZONode *args = type->next;
		result = optimize_node(opt, args, flags);
		if (result) return result;
		AZONode *body = args->next;
		result = optimize_node(opt, body, flags);
		if (result) return result;
	}
	return 0;
}

static int
optimize_function_call(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	assert (ref != NULL);
	int result = optimize_node(opt, ref, flags);
	if (result) return result;
	AZONode *args = ref->next;
	assert(args != NULL);
	result = optimize_node(opt, args, flags);
	if (result) return result;
	return 0;
}

static int
optimize_array_element(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	assert(ref != NULL);
	int result = optimize_node(opt, ref, flags);
	if (result) return result;
	AZONode *idx = ref->next;
	assert(idx != NULL);
	result = optimize_node(opt, idx, flags);
	if (result) return result;
	return 0;
}

static int
optimize_list(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return optimize_children(opt, node->children, flags);
}

static int
optimize_reference(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* Variable references have to be resolved to either CONSTANT or MEMBER */
	assert(node->term.subtype == AZO_TERM_REFERENCE_MEMBER);
	AZONode *expr = node->children;
	int result = optimize_node(opt, expr, flags);
	if (result) return result;
	return 0;
}

static int
optimize_literal_array(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	int result = optimize_children(opt, node->children, flags);
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
optimize_cast(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	assert(node->children);
	assert(node->children->term.type == AZO_TERM_TYPE);
	AZONode *expr = node->children->next;
	int result = optimize_node(opt, expr, flags);
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
		return azo_compiler_calculate_rvalue_prefix(opt, node);
	}
	return 0;
}

static int
optimize_binary(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *lhs = node->children;
	assert(lhs);
	AZONode *rhs = lhs->next;
	assert(rhs);
	int result = optimize_node(opt, lhs, flags);
	if (result) return result;
	result = optimize_node(opt, rhs, flags);
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
optimize_assign(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	AZONode *ref = node->children;
	AZONode *val = ref->next;
	return optimize_node(opt, val, flags);
}

static int
optimize_test(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
	return 0;
}

static int
optimize_select(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	/* fixme: */
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
optimize_node(AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	switch (node->term.type) {
		case AZO_TERM_INVALID:
			fprintf(stderr, "optimize_node: type = INVALID\n");
			return 1;
		case AZO_TERM_EMPTY:
			return 0;
		case AZO_TERM_PROGRAM:
			return optimize_program(opt, node, flags);
		case AZO_TERM_BLOCK:
			return optimize_block(opt, node, flags);
		case AZO_TERM_STATEMENT_GROUP:
			return optimize_group(opt, node, flags);
		case AZO_TERM_KEYWORD:
			return optimize_keyword(opt, node, flags);
		case AZO_TERM_DECLARATION_LIST:
			return optimize_declaration_list(opt, node, flags);
		case AZO_TERM_DECLARATION:
			return optimize_declaration(opt, node, flags);
		case AZO_TERM_ARGUMENT_DECLARATION:
			return optimize_argument_declaration(opt, node, flags);
		case AZO_TERM_FUNCTION:
			return optimize_function(opt, node, flags);
		case AZO_TERM_FUNCTION_CALL:
			return optimize_function_call(opt, node, flags);
		case AZO_TERM_ARRAY_ELEMENT:
			return optimize_array_element(opt, node, flags);
		case AZO_TERM_LIST:
			return optimize_list(opt, node, flags);
		case AZO_TERM_REFERENCE:
			return optimize_reference(opt, node, flags);
		case AZO_TERM_LITERAL_ARRAY:
			return optimize_literal_array(opt, node, flags);
		case AZO_TERM_CAST:
			return optimize_cast(opt, node, flags);
		case AZO_TERM_SUFFIX:
			return optimize_suffix(opt, node, flags);
		case AZO_TERM_PREFIX:
			return optimize_prefix(opt, node, flags);
		case AZO_TERM_BINARY:
			return optimize_binary(opt, node, flags);
		case AZO_TERM_COMPARISON:
			return optimize_comparison(opt, node, flags);
		case AZO_TERM_ASSIGN:
			return optimize_assign(opt, node, flags);
		case AZO_TERM_TEST:
			return optimize_test(opt, node, flags);
		case AZO_TERM_SELECT:
			return optimize_select(opt, node, flags);
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
azo_compiler_optimize (AZOOptimizer *opt, AZONode *root, unsigned int flags)
{
	unsigned int iter = 0;
	do {
		fprintf(stderr, "---- Optimizer iteration %d ------\n", iter++);
		opt->n_const_subst = 0;
		int result = optimize_node(opt, root, flags);
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
