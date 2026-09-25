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
resolve_children (AZOCompiler *comp, AZONode *node, unsigned int flags)
{
	unsigned int result = 0;
	for (AZONode *child = node->children; child; child = child->next) {
		unsigned int lresult = azo_compiler_resolve_node (comp, child, flags);
		if (lresult) result = 1;
	}
	return result;
}

static unsigned int
resolve_for (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	unsigned int result = 0;
	AZONode *init = expr->children;
	AZONode *test = init->next;
	AZONode *step = test->next;
	AZONode *content = step->next;
	/* for: create new scope */
	azo_frame_push_scope (comp->current);
	unsigned int lresult = azo_compiler_resolve_node (comp, init, flags);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, test, 0);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, step, 0);
	if (lresult) result = 1;
	lresult = azo_compiler_resolve_node (comp, content, 0);
	if (lresult) result = 1;
	expr->scope_size = azo_scope_get_size (comp->current->scope);
	azo_frame_pop_scope (comp->current);
	return result;
}

#define noDEBUG_RESOLVE_NEW

static unsigned int
resolve_new (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	unsigned int result = 0;
	static AZString *new_str = NULL;
	if (!new_str) new_str = az_string_new((const uint8_t *) "new");
	AZONode *ref, *args, *child;
	ref = expr->children;
	args = ref->next;
	assert (!args->next);
	result = azo_compiler_resolve_node_to_class(comp, ref, flags);
	if (result) return result;
	result = azo_compiler_resolve_node (comp, args, flags);
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
	klass = (AZClass *) ref->value.v.block;
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
			az_packed_value_set_from_impl_value (&ref->value, prop_impl, &prop_val.value);
			expr->term.type = AZO_TERM_FUNCTION_CALL;
			expr->term.subtype = AZO_TERM_GENERIC;
			ref->term.type = AZO_TERM_CONSTANT;
			if (prop_impl) {
				ref->term.subtype = AZ_IMPL_TYPE(prop_impl);
				az_value_clear (prop_impl, &prop_val.value);
			} else {
				// Missing new, this is not normal
				ref->term.subtype = 0;
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
resolve_declaration (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	AZOVariable *var;
	unsigned int result;
	AZONode *id = expr->children;
	AZONode *value = id->next;
	if (azo_scope_lookup_local_var (comp->current->scope, id->value.v.string)) {
		fprintf (stderr, "resolve_declaration: Variable %s already declared in scope\n", id->value.v.string->str);
		return 1;
	}
	// fixme: Use type
	var = azo_frame_declare_variable (comp->current, id->value.v.string, AZ_TYPE_ANY, &result);
	if (result) return result;
	if (value) {
		result = azo_compiler_resolve_node (comp, value, flags);
		if (result) return result;
	}
	return 0;
}

static unsigned int
resolve_declaration_list (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	unsigned int result;
	AZONode *type = expr->children;
	AZONode *child = type->next;
	result = azo_compiler_resolve_node_to_class(comp, type, flags);
	if (result) return result;
	type->term.type = AZO_TERM_TYPE;
	type->term.subtype = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);
	az_packed_value_clear (&type->value);
	for (child = type->next; child; child = child->next) {
		if (resolve_declaration (comp, child, flags)) {
			return 1;
		}
	}
	return 0;
}

static unsigned int
resolve_function (AZOCompiler *comp, AZONode *expr, unsigned int flags)
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
	result = azo_compiler_resolve_node (comp, type, flags);
	if (result) return result;
	if (type->term.type == AZO_TERM_EMPTY) {
		/* Replace void with type none */
		type->term.type = AZO_TERM_TYPE;
		type->term.subtype = AZ_TYPE_NONE;
		az_packed_value_clear (&type->value);
	} else {
		if (type->term.type != AZO_TERM_CONSTANT) {
			fprintf (stderr, "resolve_function: Return type is not a compile-time constant (%u/%u)\n", type->term.type, type->term.subtype);
			return 1;
		}
		if (type->term.subtype != AZ_TYPE_CLASS) {
			fprintf (stderr, "resolve_function: Return type is not a class\n");
			return 1;
		}
		type->term.type = AZO_TERM_TYPE;
		type->term.subtype = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);
	}
	unsigned int ret_type = type->term.subtype;

	/* This type */
	const AZImplementation *this_impl = (expr->term.subtype == AZO_TERM_FUNCTION_STATIC_OLD)
		//|| (expr->term.subtype == AZO_TERM_LAMBDA)
		? (const AZImplementation *) az_type_get_class (AZ_TYPE_ANY) : NULL;
	if (obj) {
		result = azo_compiler_resolve_node (comp, obj, flags);
		if (result) return result;
		if (obj->term.type == AZO_TERM_CONSTANT) {
			if (obj->term.subtype != AZ_TYPE_CLASS) {
				fprintf (stderr, "resolve_function: parent is constant non-class (%u)\n", obj->term.subtype);
				return 1;
			}
			this_impl = (const AZImplementation *) obj->value.v.block;
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
		result = azo_compiler_resolve_node (comp, type, flags);
		if (result) return result;
		if (type->term.type != AZO_TERM_EMPTY) {
			if (type->term.type != AZO_TERM_CONSTANT) {
				fprintf (stderr, "resolve_function: Argument type is not a compile-time constant (%u/%u)\n", type->term.type, type->term.subtype);
				return 1;
			}
			if (type->term.subtype != AZ_TYPE_CLASS) {
				fprintf (stderr, "resolve_function: Argument type is not a class\n");
				return 1;
			}
		}
		if ((name->term.type != AZO_TERM_REFERENCE) || (name->term.subtype != AZO_TERM_REFERENCE_VARIABLE)) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", name->term.type, name->term.subtype);
			return 1;
		}
		n_args += 1;
	}

	azo_compiler_push_frame (comp, this_impl, NULL, n_args, ret_type);

	for (child = args->children; child; child = child->next) {
		type = child->children;
		AZONode *name = type->next;
		// fixme: Use type
		if (!azo_frame_declare_variable (comp->current, name->value.v.string, AZ_TYPE_ANY, &result)) {
			fprintf (stderr, "resolve_function: Repeated variable name %s\n", name->value.v.string->str);
			return result;
		}
	}

	int lresult = azo_compiler_resolve_frame(comp, body);
	if (lresult) result = 1;

	expr->frame = azo_compiler_pop_frame (comp);

	return result;
}

#define noDEBUG_RESOLVE_FUNCTION_CALL

static unsigned int
azo_compiler_resolve_function_call (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	AZONode *ref = expr->children;
	AZONode *args = ref->next;
	assert (!args->next);
	unsigned int result = 0;
	if (args->term.type != AZO_TERM_LIST) {
		fprintf (stderr, "azo_compiler_resolve_function_call: arguments is not list\n");
		return 1;
	}

	result = azo_compiler_resolve_reference (comp, ref, flags);
	if (result) return result;
	result = azo_compiler_resolve_node (comp, args, flags);
	if (result) return result;

	return 0;
}

static unsigned int
azo_compiler_resolve_literal_array (AZOCompiler *comp, AZONode *node)
{
	return resolve_children(comp, node, 0);
}

static unsigned int
resolve_prefix_suffix (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	unsigned int result = resolve_children(comp, expr, AZO_COMPILER_VAR_IS_LVALUE);
	if (result) return result;
	return 0;
}

static unsigned int
resolve_plain_assign(AZOCompiler *comp, AZONode *node, unsigned int flags)
{
	assert(AZO_NODE_IS(node, AZO_TERM_ASSIGN, AZO_TERM_ASSIGN_PLAIN));
	AZONode *left = node->children;
	AZONode *right = left->next;
	unsigned int result = azo_compiler_resolve_node (comp, left, flags | AZO_COMPILER_VAR_IS_LVALUE);
	if (result) return result;
	result = azo_compiler_resolve_node (comp, right, flags);
	if (result) return result;
	return 0;
}

static unsigned int
resolve_assign (AZOCompiler *comp, AZONode *node, unsigned int flags)
{
	if (node->term.subtype == AZO_TERM_ASSIGN_PLAIN) {
		return resolve_plain_assign(comp, node, flags);
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
	return resolve_plain_assign(comp, node, flags);
}

static unsigned int
resolve_return (AZOCompiler *comp, AZONode *term, unsigned int flags)
{
	unsigned int result = 0;
	if (term->children) {
		AZONode *val = term->children;
		result = azo_compiler_resolve_node (comp, val, flags);
		if (result) return result;
		if (val->term.type == AZO_TERM_EMPTY) {
			if (comp->current->ret_type != AZ_TYPE_NONE) {
				fprintf (stderr, "azo_compiler_resolve_expression: Must return a value\n");
				result = 1;
			}
		} else if (val->term.type == AZO_TERM_CONSTANT) {
			if (!az_type_is_a (val->term.subtype, comp->current->ret_type)) {
				if (az_value_convert_in_place (&val->value.impl, &val->value.v, comp->current->ret_type, AZ_CONVERT_CONDITIONAL) == AZ_CONVERSION_FAILED) {
					fprintf (stderr, "azo_compiler_resolve_expression: Return value is wrong type\n");
					result = 1;
				} else {
					val->term.subtype = AZ_PACKED_VALUE_TYPE(&val->value);
				}
			}
		} else {
		}
	} else {
		if (comp->current->ret_type != AZ_TYPE_NONE) {
			fprintf (stderr, "azo_compiler_resolve_expression: Must return a value\n");
			result = 1;
		}
	}
	return result;
}

unsigned int
azo_compiler_resolve_cast (AZOCompiler *comp, AZONode *expr, unsigned int flags)
{
	AZONode *type = expr->children;
	AZONode *val = type->next;
	unsigned int result;
	if (expr->term.subtype != AZO_TERM_CAST_CONVERT) {
		/* fixme: implement checked class/interface conversion (as) */
		fprintf (stderr, "azo_compiler_resolve_cast: only primitive conversion casts are implemented\n");
		return 1;
	}
	result = azo_compiler_resolve_node (comp, type, flags);
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

	result = azo_compiler_resolve_node (comp, val, flags);
	if (result) return result;
	return 0;
}

unsigned int
azo_compiler_resolve_node (AZOCompiler *comp, AZONode *node, unsigned int flags)
{
	unsigned int result = 0;
	switch (node->term.type) {
		case AZO_TERM_INVALID:
			fprintf(stderr, "resolve_node: type = INVALID\n");
			return 1;
		case AZO_TERM_EMPTY:
			return 0;
		case AZO_TERM_PROGRAM:
			return resolve_children(comp, node, flags);
		case AZO_TERM_BLOCK:
			/* Create new scope */
			azo_frame_push_scope(comp->current);
			result = resolve_children(comp, node, flags);
			node->scope_size = azo_scope_get_size(comp->current->scope);
			azo_frame_pop_scope(comp->current);
			return result;
		case AZO_TERM_STATEMENT_GROUP:
			return resolve_children(comp, node, flags);
		case AZO_TERM_KEYWORD:
			if (node->term.subtype == AZO_KEYWORD_FOR) {
				return resolve_for(comp, node, flags);
			} else if (node->term.subtype == AZO_KEYWORD_DO) {
				/* fixme: Scope */
				return resolve_children(comp, node, flags);
			} else if (node->term.subtype == AZO_KEYWORD_IF) {
				/* fixme: Scope */
				return resolve_children(comp, node, flags);
			} else if (node->term.subtype == AZO_KEYWORD_NEW) {
				return resolve_new(comp, node, flags);
			} else if (node->term.subtype == AZO_KEYWORD_RETURN) {
				return resolve_return (comp, node, flags);
			} else {
				return resolve_children(comp, node, flags);
			}
			break;
		case AZO_TERM_DECLARATION_LIST:
			return resolve_declaration_list (comp, node, flags);
		case AZO_TERM_DECLARATION:
		case AZO_TERM_ARGUMENT_DECLARATION:
			return resolve_children(comp, node, flags);
		case AZO_TERM_FUNCTION:
			return resolve_function (comp, node, flags);
		case AZO_TERM_FUNCTION_CALL:
			return azo_compiler_resolve_function_call (comp, node, flags);
		case AZO_TERM_ARRAY_ELEMENT:
		case AZO_TERM_LIST:
			return resolve_children(comp, node, flags);
		case AZO_TERM_REFERENCE:
			return azo_compiler_resolve_reference (comp, node, flags);
		case AZO_TERM_LITERAL_ARRAY:
			return azo_compiler_resolve_literal_array (comp, node);
		case AZO_TERM_CAST:
			return azo_compiler_resolve_cast (comp, node, flags);
		case AZO_TERM_PREFIX:
			if ((node->term.subtype == AZO_TERM_PREFIX_INCREMENT) || (node->term.subtype == AZO_TERM_PREFIX_DECREMENT)) {
				return resolve_prefix_suffix (comp, node, flags);
			} else {
				return resolve_children (comp, node, flags);
			}
		case AZO_TERM_SUFFIX:
			return resolve_prefix_suffix (comp, node, flags);
		case AZO_TERM_BINARY:
		case AZO_TERM_COMPARISON:
			return resolve_children(comp, node, flags);
		case AZO_TERM_ASSIGN:
			return resolve_assign (comp, node, flags);
		case AZO_TERM_TEST:
		case AZO_TERM_SELECT:
		case AZO_TERM_CONSTANT:
			return resolve_children(comp, node, flags);
		/* The following two should not be in parse tree */
		case AZO_TERM_VARIABLE:
		case AZO_TERM_TYPE:
		default:
			assert(0);
	}
	return 0;
}

int
azo_compiler_resolve_frame(AZOCompiler *comp, AZONode *node)
{
	unsigned int result = 0;
	unsigned int ret_is_last = 0;
	for (AZONode *child = node->children; child; child = child->next) {
		if (AZO_NODE_IS (child, AZO_TERM_KEYWORD, AZO_KEYWORD_RETURN)) {
			result = resolve_return (comp, child, 0);
			ret_is_last = 1;
		} else {
			unsigned int flags = 0;
			flags |= AZO_COMPILER_VAR_IS_LVALUE;
			result = azo_compiler_resolve_node (comp, child, flags);
			ret_is_last = 0;
		}
		if (result) break;
	}
	if (comp->current->ret_type && !ret_is_last) {
		fprintf (stderr, "azo_compiler_resolve_frame: Missing return statement\n");
		return 1;
	}
	if (comp->current->parent_vars) {
		/* Reverse list */
		comp->current->parent_vars = azo_var_list_reverse(comp->current->parent_vars);
	}
	return result;
}
