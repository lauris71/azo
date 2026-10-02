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

static void
analyze_variables (AZOCompiler *comp, AZONode *expr)
{
	AZOVariableList *var;
	fprintf (stderr, "Popping scope:\n");
	for (var = comp->current->scope->variables; var; var = var->next) {
		fprintf (stderr, "Var %s\n", var->var.name->str);
	}
}

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
	comp->current->ret_is_last = 0;
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
	if (result) return result;

	if (rctx->frame->ret_type && !comp->current->ret_is_last) {
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
	azo_frame_push_scope (comp->current);
	unsigned int lresult = azo_compiler_resolve_node (comp, rctx, init);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, test);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, step);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, rctx, content);
	if (lresult) result = 1;
	azo_frame_pop_scope (comp->current);
	return result;
}

#define noDEBUG_RESOLVE_NEW

static unsigned int
resolve_new (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	unsigned int result = 0;
	static AZString *new_str = NULL;
	if (!new_str) new_str = az_string_new((const uint8_t *) "new");
	AZONode *type, *args, *child;
	type = expr->children;
	args = type->next;
	assert (!args->next);

	result = azo_compiler_resolve_type_expression(comp, rctx, type);
	if (result) return result;

	result = azo_compiler_resolve_node (comp, rctx, args);
	if (result) return result;

	/* fixme: Optimizer thing */
	/* Test if arguments list is constant */
	unsigned int n_args = 0;
	unsigned int arg_types[64];
	for (child = args->children; child; child = child->next) {
		if (child->term.type != AZO_TERM_CONSTANT) return result;
		arg_types[n_args] = child->term.subtype;
		n_args += 1;
		if (n_args >= 64) return result;
	}
	/* All arguments are constants */
	const AZClass *klass;
	const AZImplementation *impl;

	klass = AZ_CLASS_FROM_TYPE(type->term.subtype);

	impl = (AZImplementation *) klass;
	AZFunctionSignature *sig = az_function_signature_new (AZ_TYPE_NONE, AZ_CLASS_TYPE(klass), n_args, arg_types);
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	int idx = az_class_lookup_function (klass, impl, NULL, new_str, sig, &def_class, &def_impl, &def_inst);
	az_function_signature_delete (sig);
	if (idx >= 0) {
		AZField *field = &def_class->props_self[idx];
		if (AZ_FIELD_IS_FINAL(field) && AZ_FIELD_IS_FUNCTION(field)) {
			const AZImplementation *prop_impl;
			AZValue64 prop_val;
			if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &prop_impl, &prop_val.value, 64, NULL)) {
				fprintf (stderr, "azo_compiler_resolve_new: Property new is not readable\n");
				return 1;
			}
			az_packed_value_set_from_impl_value (&type->value, prop_impl, &prop_val.value);
			expr->term.type = AZO_TERM_FUNCTION_CALL;
			expr->term.subtype = AZO_TERM_GENERIC;
			type->term.type = AZO_TERM_CONSTANT;
			if (prop_impl) {
				type->term.subtype = AZ_IMPL_TYPE(prop_impl);
				az_value_clear (prop_impl, &prop_val.value);
			} else {
				// Missing new, this is not normal
				type->term.subtype = 0;
			}
#ifdef DEBUG_RESOLVE_NEW
			fprintf (stderr, "azo_compiler_resolve_new: Replaced new %s with constant\n", klass->name);
#endif
			return 0;
		}
	}
	return 0;
}

static unsigned int
resolve_declaration (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	AZOVariable *var;
	unsigned int result;
	AZONode *name = node->children;
	AZONode *value = name->next;
	if (azo_scope_lookup_local_var (comp->current->scope, name->value.v.string)) {
		fprintf (stderr, "resolve_declaration: Variable %s already declared in scope\n", name->value.v.string->str);
		return 1;
	}
	/* Value has to be solved first so it cannot refer to name */
	if (value) {
		result = azo_compiler_resolve_node (comp, rctx, value);
		if (result) return result;
	}
	// fixme: Use type
	azo_frame_declare_variable (comp->current, name->value.v.string, AZ_TYPE_ANY, &result);
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
resolve_function (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *obj, *type, *args, *body, *child;
	unsigned int result = 0;
	if (expr->term.subtype == AZO_TERM_FUNCTION_MEMBER_OLD) {
		type = expr->children;
		obj = type->next;
		args = obj->next;
		body = args->next;
	} else if (expr->term.subtype == AZO_TERM_FUNCTION_STATIC_OLD) {
		type = expr->children;
		obj = NULL;
		args = type->next;
		body = args->next;
	} else if (expr->term.subtype == AZO_TERM_LAMBDA) {
		obj = NULL;
		type = expr->children;
		args = type->next;
		body = args->next;
	} else {
		fprintf (stderr, "resolve_function: Invalid function expression subtype %u\n", expr->term.subtype);
		return 1;
	}

	/* Return type */
	if (type->term.type == AZO_TERM_EMPTY) {
		/* Replace void with type none */
		type->term.type = AZO_TERM_TYPE;
		type->term.subtype = AZ_TYPE_NONE;
		az_packed_value_clear (&type->value);
	} else {
		result = azo_compiler_resolve_type_expression(comp, rctx, type);
		if (result) return result;
	}
	unsigned int ret_type = type->term.subtype;

	/* This type */
	AZONode *this_node = NULL;
	if (obj) {
		this_node = azo_node_duplicate_tee(obj);
	} else if (expr->term.subtype == AZO_TERM_FUNCTION_STATIC_OLD) {
		this_node = azo_node_new(AZO_TERM_TYPE, AZ_TYPE_ANY, body->term.start, body->term.end);
	}
	if (this_node) {
		AZONode *ctx_node = azo_node_new(AZO_TERM_CONTEXT, AZO_TERM_GENERIC, body->term.start, body->term.end);
		ctx_node->children = this_node;
		this_node->next = body->children;
		body->children = ctx_node;
	}
	const AZImplementation *this_impl = (expr->term.subtype == AZO_TERM_FUNCTION_STATIC_OLD)
		//|| (expr->term.subtype == AZO_TERM_LAMBDA)
		? (const AZImplementation *) az_type_get_class (AZ_TYPE_ANY) : NULL;
	if (obj) {
		result = azo_compiler_resolve_node(comp, rctx, obj);
		if (result) return result;
		if (obj->term.type == AZO_TERM_CONSTANT) {
			if (obj->term.subtype != AZ_TYPE_CLASS) {
				fprintf (stderr, "resolve_function: parent is constant non-class (%u)\n", obj->term.subtype);
				return 1;
			}
			this_impl = (const AZImplementation *) obj->value.v.block;
			obj->term.type = AZO_TERM_TYPE;
			obj->term.subtype = AZ_IMPL_TYPE((const AZImplementation *) obj->value.v.block);
			az_packed_value_clear(&obj->value);
		} else {
			this_impl = (const AZImplementation *) az_type_get_class (AZ_TYPE_ANY);
		}
	}

	unsigned int n_args = 0;
	for (child = args->children; child; child = child->next) {
		if (child->term.type != AZO_TERM_ARGUMENT_DECLARATION) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", child->term.type, child->term.subtype);
			return 1;
		}
		type = child->children;
		AZONode *name = type->next;
		if (type->term.type != AZO_TERM_EMPTY) {
			result = azo_compiler_resolve_type_expression(comp, rctx, type);
			if (result) return result;
		}
		if (!AZO_NODE_IS(name, AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE)) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", name->term.type, name->term.subtype);
			return 1;
		}
		n_args += 1;
	}

	AZOFrame *current = rctx->frame;
	unsigned int func_frame_idx = comp->n_frames;
	AZOFrame *func_frame = azo_compiler_push_frame (comp, this_impl, NULL, n_args, ret_type);
	assert(func_frame);

	for (child = args->children; child; child = child->next) {
		type = child->children;
		AZONode *name = type->next;
		// fixme: Use type
		if (!azo_frame_declare_variable (comp->current, name->value.v.string, AZ_TYPE_ANY, &result)) {
			fprintf (stderr, "resolve_function: Repeated variable name %s\n", name->value.v.string->str);
			return result;
		}
	}

	AZOResolveCtx fctx = *rctx;
	if (obj) fctx.this_node = obj;
	fctx.ret_type = ret_type;
	fctx.frame = func_frame;
	int lresult = azo_compiler_resolve_frame(comp, &fctx, body);
	if (lresult) result = 1;

	expr->frame = func_frame_idx;
	azo_compiler_set_frame(comp, current);

	return result;
}

#define noDEBUG_RESOLVE_FUNCTION_CALL

static unsigned int
azo_compiler_resolve_function_call (AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr)
{
	AZONode *ref = expr->children;
	AZONode *args = ref->next;
	assert (!args->next);
	unsigned int result = 0;
	if (args->term.type != AZO_TERM_LIST) {
		fprintf (stderr, "azo_compiler_resolve_function_call: arguments is not list\n");
		return 1;
	}

	result = azo_compiler_resolve_reference (comp, rctx, ref);
	if (result) return result;
	result = azo_compiler_resolve_node (comp, rctx, args);
	if (result) return result;

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
			if (this_node && AZO_NODE_IS(child, AZO_TERM_FUNCTION, AZO_TERM_LAMBDA)) {
				AZONode *type = child->children;
				AZONode *args = type->next;
				AZONode *body = args->next;
				/* Add context node to lambda body */
				this_node = azo_node_duplicate_tee(this_node);
				AZONode *ctx_node = azo_node_new(AZO_TERM_CONTEXT, AZO_TERM_GENERIC, child->term.start, child->term.end);
				ctx_node->children = this_node;
				this_node->next = body;
				args->next = ctx_node;
			}
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
			if (!az_type_is_a (val->term.subtype, comp->current->ret_type)) {
				if (az_value_convert_in_place (&val->value.impl, &val->value.v, comp->current->ret_type, AZ_CONVERT_CONDITIONAL) == AZ_CONVERSION_FAILED) {
					fprintf (stderr, "azo_compiler_resolve_expression: Return value is wrong type\n");
					result = 1;
				} else {
					val->term.subtype = AZ_PACKED_VALUE_TYPE(&val->value);
				}
			}
		}
	} else {
		if (comp->current->ret_type != AZ_TYPE_NONE) {
			fprintf (stderr, "azo_compiler_resolve_expression: Must return a value\n");
			result = 1;
		}
	}
	comp->current->ret_is_last = 1;
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
	if (!ctx->this_node || (ctx->this_node->term.type == AZO_TERM_EMPTY)) {
		fprintf(stderr, "resolve_this: 'this' used outside of class context\n");
		return 1;
	}
	return 0;
}

static int
resolve_block(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node)
{
	int result;
	/* Create new scope */
	azo_frame_push_scope(comp->current);
	result = resolve_children(comp, rctx, node);
	azo_frame_pop_scope(comp->current);
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
		case AZO_TERM_CONTEXT: {
			AZONode *this_node = node->children;
			unsigned int lresult = azo_compiler_resolve_node(comp, rctx, this_node);
			if (lresult) result = 1;
			AZOResolveCtx lctx = *rctx;
			lctx.this_node = this_node;
			lresult = resolve_chain(comp, &lctx, this_node->next);
			if (lresult) result = 1;
			return result;
		}
		case AZO_TERM_PROGRAM:
			fprintf(stderr, "resolve_node: type = PROGRAM\n");
			return 1;
		case AZO_TERM_BLOCK:
			return resolve_block(comp, rctx, node);
		case AZO_TERM_STATEMENT_GROUP:
			return resolve_children(comp, rctx, node);
		case AZO_TERM_KEYWORD:
			if (node->term.subtype == AZO_KEYWORD_THIS) {
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
			return azo_compiler_resolve_function_call (comp, rctx, node);
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
azo_compiler_resolve_program(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node, const AZImplementation *this_impl, void *this_inst)
{
	assert(AZO_NODE_IS(node, AZO_TERM_PROGRAM, AZO_TERM_GENERIC));
	if (this_impl) {
		/* We have to wrap the whole program in context node */
		AZONode *ctx_node = azo_node_new(AZO_TERM_CONTEXT, AZO_TERM_GENERIC, node->term.start, node->term.end);
		AZONode *this_node;
		if (this_inst) {
			this_node = azo_node_new(AZO_TERM_CONSTANT, AZ_IMPL_TYPE(this_impl), node->term.start, node->term.start);
			az_packed_value_set_autobox(&this_node->value, this_impl, this_inst);
		} else {
			this_node = azo_node_new(AZO_TERM_TYPE, AZ_IMPL_TYPE(this_impl), node->term.start, node->term.start);
		}
		ctx_node->children = this_node;
		this_node->next = node->children;
		node->children = ctx_node;
	}
	return azo_compiler_resolve_frame(comp, rctx, node);
}
