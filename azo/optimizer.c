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

#include <azo/keyword.h>
#include <azo/optimizer.h>
#include <azo/compiler/compiler.h>

static int optimize_node(AZOOptimizer *opt, AZONode *node, unsigned int flags);

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
	return optimize_children(opt, node->children, flags);
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
	/* fixme: */
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
	if ((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT)) {
		return azo_compiler_optimize_constant_binary(opt, node);
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
	/* fixme: */
	return 0;
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
azo_compiler_optimize (AZOOptimizer *opt, AZONode *node, unsigned int flags)
{
	return optimize_node(opt, node, flags);
}