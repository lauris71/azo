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
#include <az/classes/active-object.h>

#include <azo/compiler/compiler.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/exception.h>
#include <azo/compiler/resolver.h>
#include <azo/calculate.h>
#include <azo/errors.h>

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

static unsigned int resolve_check_boolean (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node);

/* Resolve children and mark the node as a boolean-producing expression */
static unsigned int
resolve_boolean_expression (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = resolve_children (comp, rctx, node);
	if (result) return result;
	node->type = AZ_TYPE_BOOLEAN;
	return 0;
}

/* Resolve arithmetic binary; logical && and || always produce boolean */
static unsigned int
resolve_binary (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = resolve_children (comp, rctx, node);
	if (result) return result;
	unsigned int subtype = node->term.subtype;
	if ((subtype == AZO_TERM_ARITHMETIC_ANDAND) || (subtype == AZO_TERM_ARITHMETIC_OROR)) {
		node->type = AZ_TYPE_BOOLEAN;
		if (resolve_check_boolean (comp, rctx, node->children)) return 1;
		if (resolve_check_boolean (comp, rctx, node->children->next)) return 1;
	} else {
		/* Infer the promoted result type when both operand types are known */
		unsigned int lhs_type = node->children->type;
		unsigned int rhs_type = node->children->next->type;
		if (lhs_type && (lhs_type != AZ_TYPE_ANY) && rhs_type && (rhs_type != AZ_TYPE_ANY)) {
			node->type = azo_calculate_result_type (lhs_type, rhs_type, subtype);
		}
	}
	return 0;
}

/* Coarse value-type class, used when the exact type is not (yet) known */
enum {
	AZO_KIND_UNKNOWN = 0,
	AZO_KIND_BOOLEAN,
	AZO_KIND_ARITHMETIC,
	AZO_KIND_INTEGRAL,
	AZO_KIND_BLOCK
};

/* Derive the type class of a resolved node, structurally where possible */
static unsigned int
resolve_node_kind (const AZONode *node)
{
	unsigned int type = node->type;
	if (type && (type != AZ_TYPE_ANY)) {
		if (type == AZ_TYPE_BOOLEAN) return AZO_KIND_BOOLEAN;
		if (AZ_TYPE_IS_INTEGRAL (type)) return AZO_KIND_INTEGRAL;
		if (AZ_TYPE_IS_ARITHMETIC (type)) return AZO_KIND_ARITHMETIC;
		if (AZ_TYPE_IS_BLOCK (type)) return AZO_KIND_BLOCK;
		return AZO_KIND_UNKNOWN;
	}
	switch (node->term.type) {
	case AZO_TERM_COMPARISON:
	case AZO_TERM_TEST:
		return AZO_KIND_BOOLEAN;
	case AZO_TERM_BINARY:
		switch (node->term.subtype) {
		case AZO_TERM_ARITHMETIC_ANDAND:
		case AZO_TERM_ARITHMETIC_OROR:
			return AZO_KIND_BOOLEAN;
		case AZO_TERM_ARITHMETIC_SHIFT_LEFT:
		case AZO_TERM_ARITHMETIC_SHIFT_RIGHT:
		case AZO_TERM_ARITHMETIC_AND:
		case AZO_TERM_ARITHMETIC_OR:
		case AZO_TERM_ARITHMETIC_CARET:
			return AZO_KIND_INTEGRAL;
		case AZO_TERM_ARITHMETIC_PLUS:
		case AZO_TERM_ARITHMETIC_MINUS:
		case AZO_TERM_ARITHMETIC_SLASH:
		case AZO_TERM_ARITHMETIC_STAR:
		case AZO_TERM_ARITHMETIC_PERCENT:
			return AZO_KIND_ARITHMETIC;
		default:
			return AZO_KIND_UNKNOWN;
		}
	case AZO_TERM_PREFIX:
		if (node->term.subtype == AZO_TERM_PREFIX_NOT) return AZO_KIND_BOOLEAN;
		if (node->term.subtype == AZO_TERM_PREFIX_TILDE) return AZO_KIND_INTEGRAL;
		return AZO_KIND_ARITHMETIC;
	case AZO_TERM_SUFFIX:
		return AZO_KIND_ARITHMETIC;
	default:
		return AZO_KIND_UNKNOWN;
	}
}

/* Reject expressions that are statically known to be non-boolean */
static unsigned int
resolve_check_boolean (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int kind = resolve_node_kind (node);
	if ((kind == AZO_KIND_UNKNOWN) || (kind == AZO_KIND_BOOLEAN)) return 0;
	azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Expression is not boolean\n");
	return 1;
}

/* Resolve ternary select and require a boolean (or unknown) condition */
static unsigned int
resolve_select (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *cond = node->children;
	AZONode *if_true = cond->next;
	AZONode *if_false = if_true->next;
	unsigned int result = resolve_children (comp, rctx, node);
	if (result) return result;
	if (resolve_check_boolean (comp, rctx, cond)) return 1;
	/* Common branch type */
	if (if_true->type && (if_true->type == if_false->type)) {
		node->type = if_true->type;
	}
	return 0;
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
	/* PROGRAM, BLOCK and MEMBER_BLOCK are all resolvable nodes */
	int result = azo_compiler_resolve_node(comp, rctx, node);
	if (rctx->print_tree) {
		azo_node_print_info(node, stdout, comp->src, 0);
	}
	rctx->print_tree = 0;
	if (result) return result;

	if (rctx->frame->ret_type && !rctx->frame->ret_is_last) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Missing return statement\n");
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
	if (resolve_check_boolean (comp, rctx, test)) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, step);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, content);
	if (lresult) result = 1;
	azo_frame_pop_scope (rctx->frame);
	/* The body may not execute at all */
	rctx->frame->ret_is_last = 0;
	return result;
}

/*
 * WHILE
 *   condition
 *   body sentence
 */

static unsigned int
resolve_while (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = 0;
	AZONode *test = expr->children;
	AZONode *content = test->next;
	/* The loop (condition and body) is a scope */
	azo_frame_push_scope (rctx->frame);
	unsigned int lresult = azo_compiler_resolve_node (comp, rctx, test);
	if (lresult) result = 1;
	if (resolve_check_boolean (comp, rctx, test)) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, content);
	if (lresult) result = 1;
	azo_frame_pop_scope (rctx->frame);
	/* The body may not execute at all */
	rctx->frame->ret_is_last = 0;
	return result;
}

/*
 * DO
 *   body sentence
 *   condition
 */

static unsigned int
resolve_do (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = 0;
	AZONode *content = expr->children;
	AZONode *test = content->next;
	/* The loop (body and condition) is a scope */
	azo_frame_push_scope (rctx->frame);
	unsigned int lresult = azo_compiler_resolve_node (comp, rctx, content);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, test);
	if (lresult) result = 1;
	if (resolve_check_boolean (comp, rctx, test)) result = 1;
	azo_frame_pop_scope (rctx->frame);
	/* Do not analyze loop paths */
	rctx->frame->ret_is_last = 0;
	return result;
}

/*
 * IF
 *   condition
 *   true sentence
 *   [false sentence]
 */

static unsigned int
resolve_if (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = 0;
	AZONode *test = expr->children;
	AZONode *if_true = test->next;
	AZONode *if_false = if_true->next;
	unsigned int lresult = azo_compiler_resolve_node (comp, rctx, test);
	if (lresult) result = 1;
	if (resolve_check_boolean (comp, rctx, test)) result = 1;
	/* Each branch is its own scope */
	azo_frame_push_scope (rctx->frame);
	lresult = azo_compiler_resolve_node (comp, rctx, if_true);
	if (lresult) result = 1;
	unsigned int true_returns = rctx->frame->ret_is_last;
	azo_frame_pop_scope (rctx->frame);
	if (if_false) {
		azo_frame_push_scope (rctx->frame);
		lresult = azo_compiler_resolve_node (comp, rctx, if_false);
		if (lresult) result = 1;
		/* The if statement returns only if both branches return */
		rctx->frame->ret_is_last = true_returns && rctx->frame->ret_is_last;
		azo_frame_pop_scope (rctx->frame);
	} else {
		/* There is a path that does not execute if_true */
		rctx->frame->ret_is_last = 0;
	}
	return result;
}

#define noDEBUG_RESOLVE_NEW

// fixme: I ma not sure about this
// What happens is that we call an method on class instance (the actual class)
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
	az_packed_value_set(&type->value, AZ_IMPL_FROM_TYPE(AZ_TYPE_CLASS), AZ_CLASS_FROM_TYPE(type->type));
	type->type = AZ_TYPE_CLASS;
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
resolve_declaration (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node, unsigned int term_flags, unsigned int type)
{
	AZONode *name = node->children;
	assert(AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE));
	AZONode *value = name->next;
	if (azo_scope_lookup_local_var (rctx->frame->scope, name->value.v.string)) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Variable %s already declared in scope\n", name->value.v.string->str);
		return 1;
	}
	/* Value has to be solved first so it cannot refer to name */
	if (value) {
		unsigned int result = azo_compiler_resolve_node (comp, rctx, value);
		if (result) return result;
	}
	unsigned int var_flags = 0;
	if (term_flags & AZO_TERM_FLAG_STATIC) var_flags |= AZO_VARIABLE_FLAG_STATIC;
	if (term_flags & AZO_TERM_FLAG_SHARED) var_flags |= AZO_VARIABLE_FLAG_SHARED;
	if (term_flags & AZO_TERM_FLAG_FINAL) {
		if (!value) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Final variable %s requires an initializer\n", name->value.v.string->str);
			return 1;
		}
		var_flags |= AZO_VARIABLE_FLAG_FINAL;
	}
	if (term_flags & AZO_TERM_FLAG_WEAK) {
		if (!az_type_is_a (type, AZ_TYPE_ACTIVE_OBJECT)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Weak variable %s requires ActiveObject type\n", name->value.v.string->str);
			return 1;
		}
		var_flags |= AZO_VARIABLE_FLAG_WEAK;
	}
	AZOVariable *var = azo_frame_declare_variable (rctx->frame, name->value.v.string, type, var_flags);
	/* The declaration name node becomes a resolved variable node */
	name->term.type = AZO_TERM_VARIABLE;
	name->term.flags = var->flags;
	name->type = type;
	name->var_idx = var->pos;
	if (var->flags & AZO_VARIABLE_FLAG_STATIC) {
		name->term.subtype = AZO_TERM_VARIABLE_STATIC;
	} else if (var->flags & AZO_VARIABLE_FLAG_SHARED) {
		name->term.subtype = AZO_TERM_VARIABLE_SHARED;
	} else {
		name->term.subtype = AZO_TERM_VARIABLE_LOCAL;
	}
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
	unsigned int resolved_type = type->type;
	for (child = type->next; child; child = child->next) {
		if (resolve_declaration (comp, rctx, child, expr->term.flags, resolved_type)) {
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
		ret_type->type = AZ_TYPE_NONE;
	} else {
		result = azo_compiler_resolve_type_expression(comp, rctx, ret_type);
		if (result) return result;
	}

	/* Argument list */
	unsigned int n_args = 0;
	for (AZONode *decl = args->children; decl; decl = decl->next) {
		if (decl->term.type != AZO_TERM_ARGUMENT_DECLARATION) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, decl->term.start, decl->term.end, "Invalid expression type %u/%u in signature\n", decl->term.type, decl->term.subtype);
			return 1;
		}
		AZONode *type = decl->children;
		AZONode *name = type->next;
		if (type->term.type == AZO_TERM_EMPTY) {
			type->term.type = AZO_TERM_CONSTANT;
			type->type = AZ_TYPE_ANY;
		} else {
			result = azo_compiler_resolve_type_expression(comp, rctx, type);
			if (result) return result;
		}
		if (!AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Invalid expression type %u/%u in signature\n", name->term.type, name->term.subtype);
			return 1;
		}
		n_args += 1;
	}

	AZOResolveCtx fctx = *rctx;
	fctx.ret_type = ret_type->type;
	fctx.print_tree = 0;

	unsigned int frame_idx;
	if (func->term.flags & AZO_TERM_FLAG_STATIC) {
		frame_idx = comp->n_frames;
		fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 0, n_args, ret_type->type);
		fctx.this_variant = AZO_COMPILER_NO_THIS;
	} else {
		/* Capture 'this'' it exists in parent frame */
		if (rctx->this_variant == AZO_COMPILER_NO_THIS) {
			frame_idx = comp->n_frames;
			fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 0, n_args, ret_type->type);
		} else {
			frame_idx = comp->n_frames;
			fctx.frame = azo_compiler_new_frame (comp, rctx->frame, 1, n_args, ret_type->type);
			fctx.this_variant = AZO_COMPILER_THIS_IS_CAPTURE;
			fctx.this_capture_pos = 0;
		}
	}

	for (AZONode *child = args->children; child; child = child->next) {
		AZONode *type = child->children;
		AZONode *name = type->next;
		if (!azo_frame_declare_variable (fctx.frame, name->value.v.string, type->type, 0)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Repeated variable name %s\n", name->value.v.string->str);
			return 1;
		}
	}

	int lresult = azo_compiler_resolve_frame(comp, &fctx, body);
	if (lresult) result = 1;

	func->frame_idx = frame_idx;

	return result;
}

static unsigned int
resolve_function_call (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *func)
{
	AZONode *ref = func->children;
	AZONode *args = ref->next;
	assert (!args->next);
	if (args->term.type != AZO_TERM_LIST) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, args->term.start, args->term.end, "Arguments node is not a list\n");
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
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, ref->term.start, ref->term.end, "Invalid reference subtype %u\n", ref->term.subtype);
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
resolve_check_final_assignment (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *target)
{
	if (AZO_NODE_IS(target, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE)) {
		AZOVariable *var = azo_frame_lookup_local_var (rctx->frame, target->value.v.string);
		if (var && (var->flags & AZO_VARIABLE_FLAG_FINAL)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, target->term.start, target->term.end, "Cannot assign to final variable %s\n", target->value.v.string->str);
			return 1;
		}
	}
	return 0;
}

static unsigned int
resolve_check_assignment_type (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *lvalue, AZONode *rvalue)
{
	/* Only simple variable assignment is type-checked for now */
	if (lvalue->term.type != AZO_TERM_VARIABLE) return 0;
	unsigned int Ta = lvalue->type;
	if (azo_type_assignment_classify_source (rvalue, Ta) == AZO_ASSIGN_REJECT) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, rvalue->term.start, rvalue->term.end, "Cannot assign type %u to type %u\n", rvalue->type, Ta);
		return 1;
	}
	/* Warn when an implicit arithmetic cast may change magnitude (rounding is allowed) */
	if (Ta && AZ_TYPE_IS_ARITHMETIC (Ta) && AZ_TYPE_IS_ARITHMETIC (rvalue->type) && (rvalue->type != Ta) && !azo_type_is_magnitude_safe (Ta, rvalue->type)) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_WARNING, AZO_SUBSYSTEM_RESOLVER, rvalue->term.start, rvalue->term.end, "Assignment may change magnitude; use an explicit cast to clamp or wrap\n");
	}
	return 0;
}

static unsigned int
resolve_prefix_suffix (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	if (resolve_check_final_assignment (comp, rctx, expr->children)) return 1;
	unsigned int result = resolve_children(comp, rctx, expr);
	if (result) return result;
	/* ++ and -- yield the operand type */
	expr->type = expr->children->type;
	return 0;
}

static unsigned int
resolve_plain_assign(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	assert(AZO_NODE_IS(node, AZO_TERM_ASSIGN, AZO_TERM_ASSIGN_PLAIN));
	if (resolve_check_final_assignment (comp, rctx, node->children)) return 1;
	for (AZONode *child = node->children; child; child = child->next) {
		/* The parser has already wrapped member lambdas into MEMBER_BLOCK */
		int result = azo_compiler_resolve_node (comp, rctx, child);
		if (result) return result;
	}
	/* Reject statically-invalid variable assignments */
	if (resolve_check_assignment_type (comp, rctx, node->children, node->children->next)) return 1;
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
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Unknown shorthand assignment type %d\n", node->term.subtype);
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
			if (!az_type_is_a (val->type, rctx->frame->ret_type)) {
				if (az_value_convert_in_place (&val->value.impl, &val->value.v, rctx->frame->ret_type, AZ_CONVERT_CONDITIONAL) == AZ_CONVERSION_FAILED) {
					azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, val->term.start, val->term.end, "Return value is wrong type\n");
					result = 1;
				} else {
					val->type = AZ_PACKED_VALUE_TYPE(&val->value);
				}
			}
		}
	} else {
		if (rctx->frame->ret_type != AZ_TYPE_NONE) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, term->term.start, term->term.end, "Must return a value\n");
			result = 1;
		}
	}
	rctx->frame->ret_is_last = 1;
	return result;
}

/*
 * Exception_declaration (compile-time directive):
 *   exception NAME                        - category declaration
 *   exception NAME : NAME [, NAME ...]    - type declaration in categories
 *
 * Registers the name(s) in the context's exception registry. Re-declaration with
 * identical shape is idempotent; conflicting shape is an error. The node emits no
 * code and is rewritten to EMPTY.
 */

static unsigned int
resolve_exception_declaration (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	/* Program scope only - registrations are context-global, scoping would imply a locality that does not exist */
	if (rctx->frame->parent || rctx->frame->scope->parent) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Exception declarations are only valid at program scope\n");
		return 1;
	}
	AZONode *name = node->children;
	AZString *nm = name->value.v.string;
	if (!name->next) {
		/* Category declaration */
		const AZOExceptionDef *def = azo_context_lookup_exception (comp->globals, nm);
		if (def) {
			if (def->type != AZ_EXCEPTION_NONE) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Cannot register category %s: the name is already an exception type\n", nm->str);
				return 1;
			}
			/* Idempotent re-declaration */
		} else if (!azo_context_register_exception_category (comp->globals, nm)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Cannot register exception category %s\n", nm->str);
			return 1;
		}
	} else {
		/* Type declaration: look up or register every category, then the type */
		uint64_t mask = 0;
		for (AZONode *cat = name->next; cat; cat = cat->next) {
			const AZOExceptionDef *cdef = azo_context_lookup_exception (comp->globals, cat->value.v.string);
			if (cdef && cdef->type != AZ_EXCEPTION_NONE) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, cat->term.start, cat->term.end, "Invalid category %s: the name is already an exception type\n", cat->value.v.string->str);
				return 1;
			}
			uint64_t bit = (cdef) ? cdef->category : azo_context_register_exception_category (comp->globals, cat->value.v.string);
			if (!bit) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, cat->term.start, cat->term.end, "Cannot register exception category %s\n", cat->value.v.string->str);
				return 1;
			}
			mask |= bit;
		}
		const AZOExceptionDef *def = azo_context_lookup_exception (comp->globals, nm);
		if (def) {
			if (def->type == AZ_EXCEPTION_NONE) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Cannot register exception %s: the name is already a category\n", nm->str);
				return 1;
			}
			if (def->category != mask) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Exception %s is already registered with different categories\n", nm->str);
				return 1;
			}
			/* Idempotent re-declaration */
		} else if (azo_context_register_exception (comp->globals, nm, mask) == AZ_EXCEPTION_NONE) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "Cannot register exception %s\n", nm->str);
			return 1;
		}
	}
	/* Compile-time directive - emits no code */
	azo_node_clear_children (node);
	node->term.type = AZO_TERM_EMPTY;
	node->term.subtype = AZO_TERM_GENERIC;
	return 0;
}

/*
 * Try_statement:
 *   KEYWORD(TRY): BLOCK, CATCH, [CATCH ...]
 *
 * Sibling scopes: the try scope reserves sentinel + one catch-handle position per
 * filter alternative (catch-all counts as one); each catch scope reserves 1 position
 * (the binding), which lands exactly on the sentinel position. The sentinel position
 * is stored in the try node's var_idx. Filter references are baked into constants:
 * category -> UINT64 mask bit, exception type -> UINT32 code.
 */

static unsigned int
resolve_try (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = 0;
	AZONode *body = node->children;
	/* The try scope holds the flags word and catch-handle positions */
	azo_frame_push_scope (rctx->frame);
	unsigned int flags_pos = rctx->frame->scope->next_var_pos;
	unsigned int n_positions = 1;
	for (AZONode *clause = body->next; clause; clause = clause->next) {
		AZONode *filters = clause->children->next;
		unsigned int n = 0;
		for (AZONode *f = filters->children; f; f = f->next) n++;
		n_positions += (n) ? n : 1;
	}
	for (unsigned int i = 0; i < n_positions; i++) azo_frame_reserve_pos (rctx->frame);
	node->var_idx = flags_pos;
	/* Protected body (own block scope; locals follow the sentinel + handles on
	 * the stack; catch bindings are not visible here) */
	if (azo_compiler_resolve_node (comp, rctx, body)) result = 1;
	/* The reservations' numeric values are all we need - pop the try scope so
	 * catch scopes become siblings starting from the enclosing position */
	azo_frame_pop_scope (rctx->frame);
	/* Catch clauses */
	for (AZONode *clause = body->next; clause; clause = clause->next) {
		AZONode *binding = clause->children;
		AZONode *filters = binding->next;
		AZONode *cbody = filters->next;
		if (!filters->children && clause->next) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, clause->term.start, clause->term.end, "Catch-all clause must be the last catch clause\n");
			result = 1;
		}
		/* Bake filters into constants */
		for (AZONode *f = filters->children; f; f = f->next) {
			const AZOExceptionDef *def = azo_context_lookup_exception (comp->globals, f->value.v.string);
			if (!def) {
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, f->term.start, f->term.end, "Unknown exception or category %s\n", f->value.v.string->str);
				result = 1;
				continue;
			}
			AZValue v;
			if (def->type == AZ_EXCEPTION_NONE) {
				v.uint64_v = def->category;
				az_packed_value_clear (&f->value);
				az_packed_value_set_from_type_value (&f->value, AZ_TYPE_UINT64, &v);
				f->type = AZ_TYPE_UINT64;
			} else {
				v.uint64_v = def->type;
				az_packed_value_clear (&f->value);
				az_packed_value_set_from_type_value (&f->value, AZ_TYPE_UINT32, &v);
				f->type = AZ_TYPE_UINT32;
			}
			f->term.type = AZO_TERM_CONSTANT;
			f->term.subtype = AZO_TERM_GENERIC;
		}
		/* Each catch is a sibling scope reserving 1 position: the binding lands
		 * on the sentinel position (the try scope's reservations were forgotten
		 * when it was popped) and locals follow right after it - during unwind
		 * the handles are drained, so the catch body starts with only the
		 * exception value on the stack. */
		azo_frame_push_scope (rctx->frame);
		/* The binding is implicitly final: it is the exception being handled, not
		 * a variable to reuse (the runtime places the object internally) */
		AZOVariable *var = azo_frame_declare_variable (rctx->frame, binding->value.v.string, AZO_TYPE_EXCEPTION, AZO_VARIABLE_FLAG_FINAL);
		if (!var) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, binding->term.start, binding->term.end, "Variable %s already declared in scope\n", binding->value.v.string->str);
			result = 1;
		} else {
			assert (var->pos == flags_pos);
			binding->term.type = AZO_TERM_VARIABLE;
			binding->term.subtype = AZO_TERM_VARIABLE_LOCAL;
			binding->term.flags = var->flags;
			binding->type = AZO_TYPE_EXCEPTION;
			binding->var_idx = var->pos;
		}
		if (azo_compiler_resolve_node (comp, rctx, cbody)) result = 1;
		azo_frame_pop_scope (rctx->frame);
	}
	return result;
}

/* Create a baked unsigned constant node (for exception codes and category masks) */

static AZONode *
new_uint_constant (unsigned int type, uint64_t val, unsigned int start, unsigned int end)
{
	AZONode *c = azo_node_new (AZO_TERM_CONSTANT, AZO_TERM_GENERIC, start, end);
	AZValue v;
	v.uint64_v = val;
	az_packed_value_set_from_type_value (&c->value, type, &v);
	c->type = type;
	return c;
}

/*
 * Throw_statement:
 *   KEYWORD(THROW): EXPRESSION
 *
 * Constructor form (throw MyException([message [, payload]])) is validated and
 * rewritten into the fixed structure THROW: CODE(U32), MASK(U64), MESSAGE|EMPTY,
 * PAYLOAD|EMPTY. Any other expression is a general throw (rethrow); the value must
 * be exception-typed if its type is statically known.
 */

static unsigned int
resolve_throw (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *child = node->children;
	if ((child->term.type == AZO_TERM_FUNCTION_CALL) &&
	    (child->children->term.type == AZO_TERM_REFERENCE) &&
	    (child->children->term.subtype == AZO_TERM_REFERENCE_VARIABLE)) {
		/* Constructor form */
		AZONode *name = child->children;
		const AZOExceptionDef *def = azo_context_lookup_exception (comp->globals, name->value.v.string);
		if (!def || (def->type == AZ_EXCEPTION_NONE)) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, name->term.start, name->term.end, "%s is not an exception type\n", name->value.v.string->str);
			return 1;
		}
		AZONode *args = child->children->next;
		unsigned int n_args = 0;
		for (AZONode *a = args->children; a; a = a->next) n_args++;
		if (n_args > 2) {
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, args->term.start, args->term.end, "Exception constructor takes at most message and payload arguments\n");
			return 1;
		}
		/* Message and payload expressions resolve normally */
		if (azo_compiler_resolve_node (comp, rctx, args)) return 1;
		AZONode *msg = args->children;
		AZONode *payload = (msg) ? msg->next : NULL;
		/* Detach message/payload before discarding the call node */
		args->children = NULL;
		if (msg) msg->next = NULL;
		if (!msg) msg = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, child->term.start, child->term.start);
		if (!payload) payload = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, child->term.start, child->term.start);
		AZONode *code = new_uint_constant (AZ_TYPE_UINT32, def->type, name->term.start, name->term.end);
		azo_node_free_tree (child);
		/* Relink into the fixed structure: [typecode, message, payload]
		 * (the category mask is looked up from the registry at throw time) */
		node->children = code;
		code->parent = node;
		code->next = msg;
		msg->parent = node;
		msg->next = payload;
		payload->parent = node;
		payload->next = NULL;
		return 0;
	}
	/* General throw (rethrow): warn if the value is provably not an exception
	 * (it compiles and throws InvalidType at runtime - see doc/errors.md) */
	unsigned int result = azo_compiler_resolve_node (comp, rctx, child);
	if (result) return result;
	if ((child->type != AZ_TYPE_NONE) && (child->type != AZ_TYPE_ANY) && !az_type_is_a (child->type, AZO_TYPE_EXCEPTION)) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_WARNING, AZO_SUBSYSTEM_RESOLVER, child->term.start, child->term.end, "Throwing a non-exception value will throw InvalidType at runtime\n");
	}
	return 0;
}

unsigned int
azo_compiler_resolve_cast (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *type = expr->children;
	AZONode *val = type->next;
	unsigned int result;
	if (expr->term.subtype != AZO_TERM_CAST_CONVERT) {
		/* fixme: implement checked class/interface conversion (as) */
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, expr->term.start, expr->term.end, "Only primitive conversion casts are implemented\n");
		return 1;
	}
	result = azo_compiler_resolve_node (comp, rctx, type);
	if (result) return result;
	if (type->term.type != AZO_TERM_CONSTANT) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, type->term.start, type->term.end, "Type expression is not a compile-time constant\n");
		return 1;
	}
	if (type->type != AZ_TYPE_CLASS) {
		azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, type->term.start, type->term.end, "Type is not a class\n");
		return 1;
	}
	type->term.type = AZO_TERM_TYPE;
	type->type = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);

	result = azo_compiler_resolve_node (comp, rctx, val);
	if (result) return result;
	/* The cast yields the target type */
	expr->type = type->type;
	return 0;
}

static int
resolve_this(AZOCompiler *comp, AZOResolveCtx *ctx, AZONode *node)
{
	switch (ctx->this_variant) {
		case AZO_COMPILER_NO_THIS:
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "'this' used outside of class context\n");
			return 1;
		case AZO_COMPILER_THIS_IS_STACK:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_LOCAL;
			node->var_idx = ctx->this_var_pos;
			break;
		case AZO_COMPILER_THIS_IS_SHARED:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_SHARED;
			node->var_idx = ctx->this_static_pos;
			break;
		case AZO_COMPILER_THIS_IS_CAPTURE:
			node->term.type = AZO_TERM_VARIABLE;
			node->term.subtype = AZO_TERM_VARIABLE_CAPTURE;
			node->var_idx = ctx->this_capture_pos;
			break;
		default:
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Unknown this variant\n");
			return 1;
	}
	az_packed_value_set_string(&node->value, azo_keyword_str(AZO_KEYWORD_THIS));
	return 0;
}

static int
resolve_block(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	int result;
	AZOResolveCtx lctx = *rctx;
	/* Static block evaluates in static context (this == NONE) */
	if (node->term.flags & AZO_TERM_FLAG_STATIC) lctx.this_variant = AZO_COMPILER_NO_THIS;
	/* Mode-qualified block starts a new computation mode */
	if (node->term.flags & AZO_TERM_FLAG_MODE_MASK) {
		lctx.exception_mask = azo_compiler_mode_mask (lctx.exception_mask, node->term.flags);
		lctx.calc_mode = azo_compiler_mode_calc (node->term.flags);
	}
	/* Create new scope */
	azo_frame_push_scope(lctx.frame);
	/* Both mode and defer block need to reserve a stack position (either to saved state or defer handle) */
	if ((node->term.flags & AZO_TERM_FLAG_MODE_MASK) || node->term.subtype == AZO_TERM_BLOCK_DEFER) {
		azo_frame_reserve_pos (lctx.frame);
	}
	/* Block children are sentences (per-sentence return tracking) */
	result = resolve_sentences(comp, &lctx, node->children);
	azo_frame_pop_scope(lctx.frame);
	return result;
}

static int
resolve_member_block(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZONode *this_node = node->children;
	AZONode *block = this_node->next;
	unsigned int result = azo_compiler_resolve_node(comp, rctx, this_node);
	if (result) result = 1;
	AZOResolveCtx lctx = *rctx;
	azo_frame_push_scope(lctx.frame);
	if (this_node->term.type == AZO_TERM_VARIABLE) {
		/* If context this is any known variable type we can use it directly */
		switch (this_node->term.subtype) {
			case AZO_TERM_VARIABLE_LOCAL:
				lctx.this_variant = AZO_COMPILER_THIS_IS_STACK;
				lctx.this_var_pos = this_node->var_idx;
				break;
			case AZO_TERM_VARIABLE_SHARED:
				lctx.this_variant = AZO_COMPILER_THIS_IS_SHARED;
				lctx.this_static_pos = this_node->var_idx;
				break;
			case AZO_TERM_VARIABLE_CAPTURE:
				lctx.this_variant = AZO_COMPILER_THIS_IS_CAPTURE;
				lctx.this_capture_pos = this_node->var_idx;
				break;
			default:
				azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, this_node->term.start, this_node->term.end, "Unknown variable type\n");
				return 1;
		}
	} else {
		/* Context 'this' was not resolved, reserve a variable spot ('this' is not valid name anyways)*/
		AZOVariable *var = azo_frame_declare_this(rctx->frame, AZ_TYPE_ANY);
		node->var_idx = var->pos;
		lctx.this_variant = AZO_COMPILER_THIS_IS_STACK;
		lctx.this_var_pos = var->pos;
	}
	/* The block child creates the scope */
	result = azo_compiler_resolve_node(comp, &lctx, block);
	if (lctx.print_tree) azo_node_print_info(node, stderr, comp->src, 0);
	azo_frame_pop_scope(lctx.frame);
	return result;
}

unsigned int
azo_compiler_resolve_node(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	unsigned int result = 0;
	switch (node->term.type) {
		case AZO_TERM_INVALID:
			azo_compiler_printf_message (comp, stderr, AZO_MESSAGE_ERROR, AZO_SUBSYSTEM_RESOLVER, node->term.start, node->term.end, "Invalid node type\n");
			return 1;
		case AZO_TERM_EMPTY:
			return 0;
		case AZO_TERM_MEMBER_BLOCK:
			return resolve_member_block(comp, rctx, node);
		case AZO_TERM_PROGRAM:
			return resolve_sentences(comp, rctx, node->children);
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
			} else if (node->term.subtype == AZO_KEYWORD_WHILE) {
				return resolve_while(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_DO) {
				return resolve_do(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_IF) {
				return resolve_if(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_NEW) {
				return resolve_new(comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_RETURN) {
				return resolve_return (comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_TRY) {
				return resolve_try (comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_THROW) {
				return resolve_throw (comp, rctx, node);
			} else if (node->term.subtype == AZO_KEYWORD_EXCEPTION) {
				return resolve_exception_declaration (comp, rctx, node);
			} else {
				return resolve_children(comp, rctx, node);
			}
			break;
		case AZO_TERM_DECLARATION_LIST:
			return resolve_declaration_list (comp, rctx, node);
		case AZO_TERM_DECLARATION:
			/* Should not be called directly */
			assert(0);
		case AZO_TERM_CATCH:
			/* Resolved as part of the try statement */
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
				unsigned int result = resolve_children (comp, rctx, node);
				if (result) return result;
				if (node->term.subtype == AZO_TERM_PREFIX_NOT) {
					node->type = AZ_TYPE_BOOLEAN;
					if (resolve_check_boolean (comp, rctx, node->children)) return 1;
				} else {
					/* Unary + - ~ yield the operand type */
					node->type = node->children->type;
				}
				return 0;
			}
		case AZO_TERM_SUFFIX:
			return resolve_prefix_suffix (comp, rctx, node);
		case AZO_TERM_BINARY:
			return resolve_binary (comp, rctx, node);
		case AZO_TERM_COMPARISON:
			return resolve_boolean_expression (comp, rctx, node);
		case AZO_TERM_ASSIGN:
			return resolve_assign (comp, rctx, node);
		case AZO_TERM_TEST:
			return resolve_boolean_expression (comp, rctx, node);
		case AZO_TERM_SELECT:
			return resolve_select (comp, rctx, node);
		case AZO_TERM_MODE_EXPRESSION: {
			AZOResolveCtx lctx = *rctx;
			lctx.exception_mask = azo_compiler_mode_mask (lctx.exception_mask, node->term.flags);
			lctx.calc_mode = azo_compiler_mode_calc (node->term.flags);
			result = resolve_children (comp, &lctx, node);
			if (result) return result;
			/* The mode expression yields the operand value/type */
			if (node->children) node->type = node->children->type;
			return 0;
		}
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
