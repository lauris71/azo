#define __AZO_RESOLVE_FRAMES_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <az/string.h>
#include <az/function.h>
#include <az/field.h>
#include <az/classes/value-array.h>

#include <azo/compiler/compiler.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/compiler/resolver.h>

static unsigned int
resolve_chain(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = 0;
	while (node) {
		unsigned int lresult = azo_compiler_resolve_node (comp, rctx, node);
		if (lresult) result = 1;
		node = node->next;
	}
	return result;
}

static unsigned int
resolve_children (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	return resolve_chain(comp, rctx, node->children);
}

unsigned int
resolve_sentence (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	rctx->frame->ret_is_last = 0;
	return azo_compiler_resolve_node(comp, rctx, node);
}

unsigned int
resolve_sentences (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = 0;
	while (node) {
		int lresult = resolve_sentence(comp, rctx, node);
		if (lresult) result = 1;
		node = node->next;
	}
	return result;
}

static int
azo_compiler_resolve_frame(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	int result = resolve_sentences(comp, rctx, node->children);
	if (rctx->print_tree) {
		azo_node_print_info(node, stdout, comp->src, 0);
	}
	rctx->print_tree = 0;
	if (result) return result;

	if (rctx->frame->ret_type && !rctx->frame->ret_is_last) {
		fprintf (stderr, "azo_compiler_resolve_frame: Missing return statement\n");
		return 1;
	}
	if (rctx->frame->parent_vars) {
		/* Reverse list */
		rctx->frame->parent_vars = azo_var_list_reverse(rctx->frame->parent_vars);
	}
	return 0;
}

static unsigned int
resolve_for (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = 0;
	AZONode *init = expr->children;
	AZONode *test = init->next;
	AZONode *step = test->next;
	AZONode *content = step->next;
	/* for: create new scope */
	azo_frame_push_scope (rctx->frame);
	unsigned int lresult = azo_compiler_resolve_node (comp, rctx, init);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, test);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, step);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, content);
	if (lresult) result = 1;
	azo_frame_pop_scope (rctx->frame);
	return result;
}

#define noDEBUG_RESOLVE_NEW

// fixme: I ma not sure about this
// What happens is that we call an method on class intance (the actual class)
// It has to resolve either to:
// Class own method (toString() result in "xyz class")
// Static method of null instance
// If the logic reamins, there should be separate FUNCTION_CALL_CLASS subtype
// but the logic is sound otherwise: MyObj.someStaticMethod should work if MyObj is MyObj class
static unsigned int
resolve_new (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *type = node->children;
	AZONode *args = type->next;
	assert (!args->next);

	unsigned int result = azo_compiler_resolve_type_expression(comp, rctx, type);
	if (result) return result;
	//result = azo_compiler_resolve_node (comp, rctx, args);
	//if (result) return result;

	/* Replace with: FUNCTION_CALL->(REF_MEMBER(TYPE,new), ARGS) */
	type->term.type = AZO_TERM_CONSTANT;
	//type->term.subtype = AZ_TYPE_CLASS;
	az_packed_value_set(&type->value, AZ_IMPL_FROM_TYPE(AZ_TYPE_CLASS), AZ_CLASS_FROM_TYPE(type->term.subtype));
	type->term.subtype = AZ_TYPE_CLASS;
	AZONode *str = azo_node_new(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER, node->term.start, node->term.end);
	az_packed_value_set_string(&str->value, azo_keyword_str(AZO_KEYWORD_NEW));
	AZONode *ref = azo_node_new_with_children(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_PROPERTY, node->term.start, type->term.end, 2, type, str);
	ref->next = args;
	node->children = ref;
	node->term.type = AZO_TERM_FUNCTION_CALL;
	/* We create plain call and re-invoke resolver to resolve it into a proper reference variant */
	node->term.subtype = AZO_TERM_FUNCTION_CALL_PLAIN;
	az_packed_value_clear(&node->value);

	return azo_compiler_resolve_node(comp, rctx, node);
}

static unsigned int
resolve_declaration (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *name = node->children;
	assert(AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
	AZONode *value = name->next;
	if (azo_scope_lookup_local_var (rctx->frame->scope, name->value.v.string)) {
		fprintf (stderr, "resolve_declaration: Variable %s already declared in scope\n", name->value.v.string->str);
		return 1;
	}
	/* Value has to be solved first so it cannot refer to name */
	if (value) {
		unsigned int result = azo_compiler_resolve_node (comp, rctx, value);
		if (result) return result;
	}
	// fixme: Use type
	AZOVariable *var = azo_frame_declare_variable (rctx->frame, name->value.v.string, AZ_TYPE_ANY);
	name->var_pos = var->pos;
	return 0;
}

static unsigned int
resolve_declaration_list (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result;
	AZONode *type = expr->children;
	AZONode *child = type->next;
	result = azo_compiler_resolve_type_expression(comp, rctx, type);
	if (result) return result;
	for (child = type->next; child; child = child->next) {
		if (resolve_declaration (comp, rctx, child)) {
			return 1;
		}
	}
	return 0;
}

static unsigned int
resolve_argument_declaration (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result;
	AZONode *type = expr->children;
	AZONode *child = type->next;
	if (type->term.type != AZO_TERM_EMPTY) {
		result = azo_compiler_resolve_type_expression(comp, rctx, type);
		if (result) return result;
		// fixme: Use type
	}
	return azo_compiler_resolve_node (comp, rctx, child);
}

static unsigned int
resolve_function (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *func)
{
	unsigned int result = 0;
	AZONode *ret_type = func->children;
	AZONode *args = ret_type->next;
	AZONode *body = args->next;

	/* Return type */
	if (ret_type->term.type == AZO_TERM_EMPTY) {
		/* Replace void with type none */
		ret_type->term.type = AZO_TERM_TYPE;
		ret_type->term.subtype = AZ_TYPE_NONE;
	} else {
		result = azo_compiler_resolve_type_expression(comp, rctx, ret_type);
		if (result) return result;
	}

	/* Argument list */
	unsigned int n_args = 0;
	for (AZONode *decl = args->children; decl; decl = decl->next) {
		if (decl->term.type != AZO_TERM_ARGUMENT_DECLARATION) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", decl->term.type, decl->term.subtype);
			return 1;
		}
		AZONode *type = decl->children;
		AZONode *name = type->next;
		if (type->term.type == AZO_TERM_EMPTY) {
			type->term.type = AZO_TERM_CONSTANT;
			type->term.subtype = AZ_TYPE_ANY;
		} else {
			result = azo_compiler_resolve_type_expression(comp, rctx, type);
			if (result) return result;
		}
		if (!AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE)) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", name->term.type, name->term.subtype);
			return 1;
		}
		n_args += 1;
	}

	AZOResolveCtx fctx = *rctx;
	fctx.ret_type = ret_type->term.subtype;

	if (func->term.flags & AZO_TERM_FLAG_STATIC) {
		fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 0, n_args, ret_type->term.subtype);
		fctx.this_variant = AZO_COMPILER_NO_THIS;
	} else {
		/* Capture 'this'' it exists in parent frame */
		if (rctx->this_variant == AZO_COMPILER_NO_THIS) {
			fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 0, n_args, ret_type->term.subtype);
		} else {
			fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 1, n_args, ret_type->term.subtype);
			fctx.this_variant = AZO_COMPILER_THIS_IS_CAPTURE;
			fctx.this_capture_pos = 0;
		}
	}

	for (AZONode *child = args->children; child; child = child->next) {
		AZONode *type = child->children;
		AZONode *name = type->next;
		// fixme: Use type
		if (!azo_frame_declare_variable (fctx.frame, name->value.v.string, AZ_TYPE_ANY)) {
			fprintf (stderr, "resolve_function: Repeated variable name %s\n", name->value.v.string->str);
			return 1;
		}
	}

	int lresult = azo_compiler_resolve_frame(comp, &fctx, body);
	if (lresult) result = 1;

	func->frame = fctx.frame;

	return result;
}

static unsigned int
resolve_function_call (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *func)
{
	AZONode *ref = func->children;
	AZONode *args = ref->next;
	assert (!args->next);
	if (args->term.type != AZO_TERM_LIST) {
		fprintf (stderr, "resolve_function_call: arguments node is not list\n");
		return 1;
	}
	unsigned int result = azo_compiler_resolve_reference (comp, rctx, ref);
	if (result) return result;
	result = azo_compiler_resolve_node (comp, rctx, args);
	if (result) return result;
	if (ref->term.type == AZO_TERM_REFERENCE) {
		/* REFERENCE_VARIABLE does not exist after resolve pass */
		switch (ref->term.subtype) {
			case AZO_TERM_REFERENCE_PROPERTY:
				func->term.subtype = AZO_TERM_FUNCTION_CALL_PROPERTY;
				break;
			case AZO_TERM_REFERENCE_ATTRIBUTE:
				func->term.subtype = AZO_TERM_FUNCTION_CALL_ATTRIBUTE;
				break;
			case AZO_TERM_REFERENCE_PROPERTY_OR_ATTRIBUTE:
				func->term.subtype = AZO_TERM_FUNCTION_CALL_PROPERTY_OR_ATTRIBUTE;
				break;
			default:
				fprintf (stderr, "resolve_function_call: Invalid reference subtype %u\n", ref->term.subtype);
				return 1;
		}
		/* FUNCTION(REF(BASE,NAME),ARGS) -> FUNCTION(BASE,NAME,ARGS) */
		AZONode *base = ref->children;
		AZONode *name = base->next;
		assert(!name->next);
		func->term.subtype = AZO_TERM_FUNCTION_CALL_PROPERTY;
		func->children = base;
		name->next = args;
		azo_node_free(ref);
	}

	return 0;
}

static unsigned int
azo_compiler_resolve_literal_array (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	return resolve_children(comp, rctx, node);
}

static unsigned int
resolve_prefix_suffix (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = resolve_children(comp, rctx, expr);
	if (result) return result;
	return 0;
}

static unsigned int
resolve_plain_assign(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	assert(AZO_NODE_IS(node, AZO_TERM_ASSIGN, AZO_TERM_ASSIGN_PLAIN));
	AZONode *this_node = NULL;
	for (AZONode *child = node->children; child; child = child->next) {
		if (child->next) {
			int result = azo_compiler_resolve_node (comp, rctx, child);
			if (result) return result;
			if (AZO_NODE_IS(child, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_PROPERTY) || AZO_NODE_IS(child, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_ATTRIBUTE)) {
				/* Keep track of context */
				this_node = child->children;
			}
		} else {
#if 0
			if (this_node && AZO_NODE_IS(child, AZO_TERM_FUNCTION, AZO_TERM_LAMBDA) && !(child->term.flags & AZO_TERM_FLAG_STATIC)) {
				/* If non-static function is assigned to property/attribute create internal this context for the function */
				AZONode *body = child->children->next->next;
				assert (body->term.type == AZO_TERM_BLOCK);
				/* Add context node to lambda body block */
				this_node = azo_node_duplicate_tee(this_node);
				AZONode *ctx_node = azo_node_new(AZO_TERM_CONTEXT, AZO_TERM_GENERIC, child->term.start, child->term.end);
				ctx_node->children = this_node;
				this_node->next = body->children;
				body->children = ctx_node;
				azo_node_print_info(child, stdout, comp->src, 0);
			}
#endif
			int result = azo_compiler_resolve_node (comp, rctx, child);
			if (result) return result;
		}
	}
	return 0;
}

static unsigned int
resolve_assign (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	if (node->term.subtype == AZO_TERM_ASSIGN_PLAIN) {
		return resolve_plain_assign(comp, rctx, node);
	}
	/* Replace shorhand binary assign with full operation */
	int binary_type = -1;
	switch (node->term.subtype) {
		case AZO_TERM_ASSIGN_PLUS:
			binary_type = AZO_TERM_ARITHMETIC_PLUS;
			break;
		case AZO_TERM_ASSIGN_MINUS:
			binary_type = AZO_TERM_ARITHMETIC_MINUS;
			break;
		case AZO_TERM_ASSIGN_STAR:
			binary_type = AZO_TERM_ARITHMETIC_STAR;
			break;
		case AZO_TERM_ASSIGN_SLASH:
			binary_type = AZO_TERM_ARITHMETIC_SLASH;
			break;
		case AZO_TERM_ASSIGN_PERCENT:
			binary_type = AZO_TERM_ARITHMETIC_PERCENT;
			break;
		case AZO_TERM_ASSIGN_SHIFT_LEFT:
			binary_type = AZO_TERM_ARITHMETIC_SHIFT_LEFT;
			break;
		case AZO_TERM_ASSIGN_SHIFT_RIGHT:
			binary_type = AZO_TERM_ARITHMETIC_SHIFT_RIGHT;
			break;
		case AZO_TERM_ASSIGN_AND:
			binary_type = AZO_TERM_ARITHMETIC_AND;
			break;
		case AZO_TERM_ASSIGN_OR:
			binary_type = AZO_TERM_ARITHMETIC_OR;
			break;
		case AZO_TERM_ASSIGN_XOR:
			binary_type = AZO_TERM_ARITHMETIC_CARET;
			break;
		default:
			break;
	}
	if (binary_type < 0) {
		fprintf(stderr, "resolve_assign: unknown shorthand assignment type %d\n", node->term.subtype);
		return 1;
	}
	AZONode *ref = node->children;
	AZONode *val = ref->next;
	AZONode *lhs = azo_node_new (ref->term.type, ref->term.subtype, ref->term.start, ref->term.end);
	az_packed_value_copy(&lhs->value, &ref->value);
	AZONode *binary = azo_node_new_with_children(AZO_TERM_BINARY, binary_type, node->term.start, node->term.end, 2, lhs, val);
	node->term.subtype = AZO_TERM_ASSIGN_PLAIN;
	ref->next = binary;
	return resolve_plain_assign(comp, rctx, node);
}

static unsigned int
resolve_return (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *term)
{
	unsigned int result = 0;
	if (term->children) {
		AZONode *val = term->children;
		result = azo_compiler_resolve_node (comp, rctx, val);
		if (result) return result;
		if (val->term.type == AZO_TERM_CONSTANT) {
			// fixme: Optimizer stuff
			// fixme: Have to check is assignable
			if (!az_type_is_a (val->term.subtype, rctx->frame->ret_type)) {
				if (az_value_convert_in_place (&val->value.impl, &val->value.v, rctx->frame->ret_type, AZ_CONVERT_CONDITIONAL) == AZ_CONVERSION_FAILED) {
					fprintf (stderr, "azo_compiler_resolve_expression: Return value is wrong type\n");
					result = 1;
				} else {
					val->term.subtype = AZ_PACKED_VALUE_TYPE(&val->value);
				}
			}
		}
	} else {
		if (rctx->frame->ret_type != AZ_TYPE_NONE) {
			fprintf (stderr, "azo_compiler_resolve_expression: Must return a value\n");
			result = 1;
		}
	}
	rctx->frame->ret_is_last = 1;
	return result;
}

unsigned int
azo_compiler_resolve_cast (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *type = expr->children;
	AZONode *val = type->next;
	unsigned int result;
	if (expr->term.subtype != AZO_TERM_CAST_CONVERT) {
		/* fixme: implement checked class/interface conversion (as) */
		fprintf (stderr, "azo_compiler_resolve_cast: only primitive conversion casts are implemented\n");
		return 1;
	}
	result = azo_compiler_resolve_node (comp, rctx, type);
	if (result) return result;
	if (type->term.type != AZO_TERM_CONSTANT) {
		fprintf (stderr, "azo_compiler_resolve_cast: Type expression is not a compile-time constant\n");
		return 1;
	}
	if (type->term.subtype != AZ_TYPE_CLASS) {
		fprintf (stderr, "azo_compiler_resolve_cast: Type is not a class\n");
		return 1;
	}
	type->term.type = AZO_TERM_TYPE;
	type->term.subtype = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);

	result = azo_compiler_resolve_node (comp, rctx, val);
	if (result) return result;
	return 0;
}

static int
resolve_this(AZOCompiler *comp, AZOResolveCtx *ctx, AZONode *node)
{
	switch (ctx->this_variant) {
		case AZO_COMPILER_NO_THIS:
			fprintf(stderr, "resolve_this: 'this' used outside of class context\n");
			return 1;
		case AZO_COMPILER_THIS_IS_ARGUMENT:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_LOCAL;
			node->var_pos = 0;
			break;
		case AZO_COMPILER_THIS_IS_VARIABLE:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_LOCAL;
			node->var_pos = ctx->this_var_pos;
			break;
		case AZO_COMPILER_THIS_IS_SHARED:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_SHARED;
			node->var_pos = ctx->this_static_pos;
			break;
		case AZO_COMPILER_THIS_IS_CAPTURE:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_CAPTURE;
			node->var_pos = ctx->this_capture_pos;
			break;
		default:
			fprintf(stderr, "resolve_this: Unknown this variant\n");
			return 1;
	}
	az_packed_value_set_string(&node->value, azo_keyword_str(AZO_KEYWORD_THIS));
	return 0;
}

static int
resolve_block(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	int result;
	/* Create new scope */
	azo_frame_push_scope(rctx->frame);
	result = resolve_children(comp, rctx, node);
	azo_frame_pop_scope(rctx->frame);
	return result;
}

static int
resolve_context(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *this_node = node->children;
	unsigned int result = azo_compiler_resolve_node(comp, rctx, this_node);
	if (result) result = 1;
	AZOResolveCtx lctx = *rctx;
	if (this_node->term.type == AZO_TERM_EMPTY) {
		lctx.this_variant = AZO_COMPILER_NO_THIS;
	} else if (this_node->term.type == AZO_TERM_VARIABLE) {
		/* If context this is any known variable type we can use it directly */
		switch (this_node->term.subtype) {
			case AZO_TERM_VARIABLE_LOCAL:
				lctx.this_variant = AZO_COMPILER_THIS_IS_VARIABLE;
				lctx.this_var_pos = this_node->var_pos;
				break;
			case AZO_TERM_VARIABLE_SHARED:
				lctx.this_variant = AZO_COMPILER_THIS_IS_SHARED;
				lctx.this_static_pos = this_node->var_pos;
				break;
			case AZO_TERM_VARIABLE_CAPTURE:
				lctx.this_variant = AZO_COMPILER_THIS_IS_CAPTURE;
				lctx.this_capture_pos = this_node->var_pos;
				break;
			default:
				fprintf(stderr, "resolve_context: Unknown variable type\n");
				return 1;
		}
	} else {
		/* Context 'this' was not resolved, reserve a variable spot ('this' is not valid name anyways)*/
		AZOVariable *var = azo_frame_declare_this(rctx->frame, AZ_TYPE_ANY);
		node->var_pos = var->pos;
		lctx.this_variant = AZO_COMPILER_THIS_IS_VARIABLE;
		lctx.this_var_pos = var->pos;
	}
	result = resolve_chain(comp, &lctx, this_node->next);
	if (lctx.print_tree) azo_node_print_info(node, stderr, comp->src, 0);
	return result;
}

unsigned int
azo_compiler_resolve_node(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = 0;
	switch (node->term.type) {
		case AZO_TERM_INVALID:
			fprintf(stderr, "resolve_node: type = INVALID\n");
			return 1;
		case AZO_TERM_EMPTY:
			return 0;
		case AZO_TERM_CONTEXT:
			return resolve_context(comp, rctx, node);
		case AZO_TERM_PROGRAM:
			fprintf(stderr, "resolve_node: type = PROGRAM\n");
			return 1;
		case AZO_TERM_BLOCK:
			return resolve_block(comp, rctx, node);
		case AZO_TERM_STATEMENT_GROUP:
			return resolve_children(comp, rctx, node);
		case AZO_TERM_KEYWORD:
			if (node->term.subtype == AZO_KEYWORD_DEBUG) {
				if (node->term.flags & AZO_TERM_DEBUG_RESOLVER_TREE) {
					rctx->print_tree = 1;
				} else {
					rctx->print_tree = 0;
				}
				return 0;
			} else if (node->term.subtype == AZO_KEYWORD_THIS) {
				return resolve_this(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_FOR) {
				return resolve_for(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_DO) {
				/* fixme: Scope */
				return resolve_children(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_IF) {
				/* fixme: Scope */
				/* fixme: return tracking */
				return resolve_children(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_NEW) {
				return resolve_new(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_RETURN) {
				return resolve_return (comp, rctx, node);
			} else {
				return resolve_children(comp, rctx, node);
			}
			break;
		case AZO_TERM_DECLARATION_LIST:
			return resolve_declaration_list (comp, rctx, node);
		case AZO_TERM_DECLARATION:
			/* Should not be called directly */
			assert(0);
		case AZO_TERM_ARGUMENT_DECLARATION:
			return resolve_argument_declaration(comp, rctx, node);
		case AZO_TERM_FUNCTION:
			return resolve_function (comp, rctx, node);
		case AZO_TERM_FUNCTION_CALL:
			return resolve_function_call (comp, rctx, node);
		case AZO_TERM_ARRAY_ELEMENT:
		case AZO_TERM_LIST:
			return resolve_children(comp, rctx, node);
		case AZO_TERM_REFERENCE:
			return azo_compiler_resolve_reference (comp, rctx, node);
		case AZO_TERM_LITERAL_ARRAY:
			return azo_compiler_resolve_literal_array (comp, rctx, node);
		case AZO_TERM_CAST:
			return azo_compiler_resolve_cast (comp, rctx, node);
		case AZO_TERM_PREFIX:
			if ((node->term.subtype == AZO_TERM_PREFIX_INCREMENT) || (node->term.subtype == AZO_TERM_PREFIX_DECREMENT)) {
				return resolve_prefix_suffix (comp, rctx, node);
			} else {
				return resolve_children (comp, rctx, node);
			}
		case AZO_TERM_SUFFIX:
			return resolve_prefix_suffix (comp, rctx, node);
		case AZO_TERM_BINARY:
		case AZO_TERM_COMPARISON:
			return resolve_children(comp, rctx, node);
		case AZO_TERM_ASSIGN:
			return resolve_assign (comp, rctx, node);
		case AZO_TERM_TEST:
		case AZO_TERM_SELECT:
		case AZO_TERM_CONSTANT:
			return resolve_children(comp, rctx, node);
		case AZO_TERM_TYPE:
		case AZO_TERM_VARIABLE:
			/* Already final */
			return 0;
		default:
			assert(0);
	}
	return 0;
}

int
azo_compiler_resolve_program(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	assert(AZO_NODE_IS(node, AZO_TERM_PROGRAM, AZO_TERM_GENERIC));
	int result = azo_compiler_resolve_frame(comp, rctx, node);
	return result;
}
