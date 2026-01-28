#define __AZO_RESOLVE_FRAMES_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <stdlib.h>
#include <stdio.h>

#include <az/string.h>

#include <azo/compiler.h>
#include <azo/expression.h>
#include <azo/keyword.h>
#include <azo/optimizer.h>

static void
analyze_variables (AZOCompiler *comp, AZOExpression *expr)
{
	AZOVariable *var;
	fprintf (stderr, "Popping scope:\n");
	for (var = comp->current->scope->variables; var; var = var->next) {
		fprintf (stderr, "Var %s: const %u\n", var->name->str, var->const_expr != NULL);
	}
}

static unsigned int
resolve_function (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *obj, *type, *args, *body, *child;
	unsigned int result = 0;
	if (expr->term.subtype == FUNCTION_MEMBER) {
		type = expr->children;
		obj = type->next;
		args = obj->next;
		body = args->next;
	} else {
		type = expr->children;
		obj = NULL;
		args = type->next;
		body = args->next;
	}

	/* Return type */
	unsigned int ret_type;
	type = azo_compiler_resolve_expression (comp, type, flags, &result);
	if (result) return result;
	if ((type->term.type == AZO_TERM_EMPTY) || ((type->term.type == EXPRESSION_KEYWORD) && (type->term.subtype == AZO_KEYWORD_VOID))) {
		/* Replace void with type none */
		type->term.type = EXPRESSION_TYPE;
		type->term.subtype = AZ_TYPE_NONE;
		az_packed_value_clear (&type->value);
		ret_type = AZ_TYPE_NONE;
	} else {
		if (type->term.type != EXPRESSION_CONSTANT) {
			fprintf (stderr, "resolve_function: Return type is not a compile-time constant (%u/%u)\n", type->term.type, type->term.subtype);
			return 1;
		}
		if (type->term.subtype != AZ_TYPE_CLASS) {
			fprintf (stderr, "resolve_function: Return type is not a class\n");
			return 1;
		}
		type->term.type = EXPRESSION_TYPE;
		type->term.subtype = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);
		ret_type = AZ_IMPL_TYPE((AZImplementation *) type->value.v.block);
	}

	/* This type */
	const AZImplementation *this_impl = (const AZImplementation *) az_type_get_class (AZ_TYPE_ANY);
	if (obj) {
		obj = azo_compiler_resolve_expression (comp, obj, flags, &result);
		if (result) return result;
		if (obj->term.type == EXPRESSION_CONSTANT) {
			if (obj->term.subtype != AZ_TYPE_CLASS) {
				fprintf (stderr, "resolve_function: parent is constant non-class (%u)\n", obj->term.subtype);
				return 1;
			}
			this_impl = ( const AZImplementation *) obj->value.v.block;
		}
	}
	azo_compiler_push_frame (comp, this_impl, NULL, ret_type);

	unsigned int n_args = 0;
	for (child = args->children; child; child = child->next) {
		if (child->term.type != EXPRESSION_ARGUMENT_DECLARATION) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", child->term.type, child->term.subtype);
			return 1;
		}
		type = child->children;
		AZOExpression *name = type->next;
		type = azo_compiler_resolve_expression (comp, type, flags, &result);
		if (result) return result;
		if (type->term.type != AZO_TERM_EMPTY) {
			if (type->term.type != EXPRESSION_CONSTANT) {
				fprintf (stderr, "resolve_function: Argument type is not a compile-time constant (%u/%u)\n", type->term.type, type->term.subtype);
				return 1;
			}
			if (type->term.subtype != AZ_TYPE_CLASS) {
				fprintf (stderr, "resolve_function: Argument type is not a class\n");
				return 1;
			}
		}
		if ((name->term.type != EXPRESSION_REFERENCE) || (name->term.subtype != REFERENCE_VARIABLE)) {
			fprintf (stderr, "resolve_function: Invalid expression type %u/%u in signature\n", name->term.type, name->term.subtype);
			return 1;
		}
		// fixme: Use type
		if (!azo_frame_declare_variable (comp->current, name->value.v.string, AZ_TYPE_ANY, &result)) {
			fprintf (stderr, "resolve_function: Repeated variable name %s\n", name->value.v.string->str);
			return result;
		}
		n_args += 1;
	}

	azo_compiler_resolve_frame (comp, body);

	if (comp->current->n_parent_vars) {
		/* Reverse list */
		AZOVariable *prev = NULL;
		AZOVariable *var = comp->current->parent_vars;
		while (var) {
			AZOVariable *next = var->next;
			var->next = prev;
			prev = var;
			var = next;
		}
		comp->current->parent_vars = prev;
	}

	expr->frame = azo_compiler_pop_frame (comp);
	return 0;
}

static unsigned int
resolve_declaration (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *id, *value;
	AZOVariable *var;
	unsigned int result;
	id = expr->children;
	value = id->next;
	if (azo_scope_lookup (comp->current->scope, id->value.v.string)) {
		fprintf (stderr, "resolve_declaration: Variable %s already declared in scope\n", id->value.v.string->str);
		return 1;
	}
	// fixme: Use type
	var = azo_frame_declare_variable (comp->current, id->value.v.string, AZ_TYPE_ANY, &result);
	if (result) return result;
	if (value) {
		value = azo_compiler_resolve_expression (comp, value, flags, &result);
		if (result) return result;
		if (!(flags & AZO_COMPILER_NO_CONST_ASSIGN) && value->term.type == EXPRESSION_CONSTANT) {
			var->const_expr = value;
		}
	}
	return 0;
}

static unsigned int
resolve_declaration_list (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *type, *child;
	unsigned int result;
	type = expr->children;
	child = type->next;
	type = azo_compiler_resolve_expression (comp, type, flags, &result);
	if (result) {
		return result;
	}
	if (type->term.type != EXPRESSION_CONSTANT) {
		fprintf (stderr, "resolve_declaration_list: Type expression is not compile-time constant (%u/%u)\n", type->term.type, type->term.subtype);
		return 1;
	}
	if (type->term.subtype != AZ_TYPE_CLASS) {
		fprintf (stderr, "resolve_declaration_list: Type expression is not a class\n");
		return 1;
	}
	type->term.type = EXPRESSION_TYPE;
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
resolve_children (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *child;
	unsigned int result;
	for (child = expr->children; child; child = child->next) {
		azo_compiler_resolve_expression (comp, child, flags, &result);
		if (result) return result;
	}
	return 0;
}

static unsigned int
resolve_for (AZOCompiler *comp, AZOExpression *expr, unsigned int *result)
{
	AZOExpression *init, *test, *step, *content;
	init = expr->children;
	test = init->next;
	step = test->next;
	content = step->next;
	/* for: create new scope */
	azo_frame_push_scope (comp->current);
	azo_compiler_resolve_expression (comp, init, AZO_COMPILER_NO_CONST_ASSIGN, result);
	azo_compiler_resolve_expression (comp, test, 0, result);
	azo_compiler_resolve_expression (comp, step, 0, result);
	azo_compiler_resolve_expression (comp, content, 0, result);
	//analyze_variables (comp, expr);
	expr->scope_size = azo_scope_get_size (comp->current->scope);
	azo_frame_pop_scope (comp->current);
	return *result;
}

static unsigned int
resolve_assign (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *left = expr->children;
	AZOExpression *right = left->next;
	unsigned int result;
	left = azo_compiler_resolve_expression (comp, left, flags | AZO_COMPILER_VAR_IS_LVALUE, &result);
	if (result) return result;
	right = azo_compiler_resolve_expression (comp, right, flags, &result);
	if (result) return result;
	if ((left->term.type == EXPRESSION_VARIABLE) && (left->term.subtype == VARIABLE_LOCAL)) {
		AZOVariable *var = azo_frame_lookup_var (comp->current, left->value.v.string);
		if (!var) {
			fprintf (stderr, "resolve_assign: CRITICAL variable %u not found\n", left->var_pos);
			return 1;
		}
		if (!(flags & AZO_COMPILER_NO_CONST_ASSIGN) && right->term.type == EXPRESSION_CONSTANT) {
			/* fixme: In base block (i.e. no if/for/while we could ignore and treat all variables as local */
			AZOVariable *loc = azo_scope_ensure_local_var (comp->current->scope, var);
			loc->const_expr = right;
			if (loc != var) var->const_expr = NULL;
		} else {
			var->const_expr = NULL;
		}
	}
	return 0;
}

static unsigned int
resolve_prefix_suffix (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *left = expr->children;
	unsigned int result;
	left = azo_compiler_resolve_expression (comp, left, flags | AZO_COMPILER_VAR_IS_LVALUE, &result);
	if (result) return result;
	if ((left->term.type == EXPRESSION_VARIABLE) && (left->term.subtype == VARIABLE_LOCAL)) {
		AZOVariable *var = azo_frame_lookup_var (comp->current, left->value.v.string);
		if (!var) {
			fprintf (stderr, "Critical failure: variable %u not found\n", left->var_pos);
			return 1;
		}
		/* fixme: In theory we could calculate the new value anyways */
		var->const_expr = NULL;
	}
	return 0;
}

static unsigned int
resolve_return (AZOCompiler *comp, AZOExpression *term, unsigned int flags)
{
	unsigned int result = 0;
	if (term->children) {
		AZOExpression *val = term->children;
		val = azo_compiler_resolve_expression (comp, val, flags, &result);
		if (result) return result;
		if (val->term.type == AZO_TERM_EMPTY) {
			if (comp->current->ret_type != AZ_TYPE_NONE) {
				fprintf (stderr, "azo_compiler_resolve_expression: Must return a value\n");
				result = 1;
			}
		} else if (val->term.type == EXPRESSION_CONSTANT) {
			if (!az_type_is_a (val->term.subtype, comp->current->ret_type)) {
				if (!az_value_convert_in_place (&val->value.impl, &val->value.v, comp->current->ret_type)) {
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

AZOExpression *
azo_compiler_resolve_expression (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result)
{
	*result = 0;
	if ((expr->term.type == EXPRESSION_KEYWORD) && (expr->term.subtype == AZO_KEYWORD_FOR)) {
		resolve_for (comp, expr, result);
	} else if (expr->term.type == AZO_EXPRESSION_BLOCK) {
		/* block: create new scope */
		azo_frame_push_scope (comp->current);
		resolve_children (comp, expr, flags);
		//analyze_variables (comp, expr);
		expr->scope_size = azo_scope_get_size (comp->current->scope);
		azo_frame_pop_scope (comp->current);
	} else if (expr->term.type == EXPRESSION_FUNCTION) {
		/* function: create new frame */
		*result = resolve_function (comp, expr, flags);
	} else if (expr->term.type == EXPRESSION_DECLARATION_LIST) {
		*result = resolve_declaration_list (comp, expr, flags);
	} else if (expr->term.type == EXPRESSION_REFERENCE) {
		expr = azo_compiler_resolve_reference (comp, expr, flags, result);
	} else if (expr->term.type == EXPRESSION_FUNCTION_CALL) {
		expr = azo_compiler_resolve_function_call (comp, expr, flags, result);
	} else if ((expr->term.type == EXPRESSION_KEYWORD) && (expr->term.subtype == AZO_KEYWORD_NEW)) {
		expr = azo_compiler_resolve_new (comp, expr, flags, result);
	} else if (AZO_EXPRESSION_IS (expr, EXPRESSION_KEYWORD, AZO_KEYWORD_RETURN)) {
		*result = resolve_return (comp, expr, flags);
	} else if (expr->term.type == EXPRESSION_ASSIGN) {
		*result = resolve_assign (comp, expr, flags);
	} else if (expr->term.type == EXPRESSION_PREFIX) {
		if ((expr->term.subtype == PREFIX_INCREMENT) || (expr->term.subtype == PREFIX_DECREMENT)) {
			*result = resolve_prefix_suffix (comp, expr, flags);
		} else {
			/* fixme: implement sub-expression resolve in prefix resolver */
			resolve_children (comp, expr, flags);
			azo_compiler_resolve_prefix (expr);
		}
	} else if (expr->term.type == EXPRESSION_SUFFIX) {
		*result = resolve_prefix_suffix (comp, expr, flags);
	} else {
		resolve_children (comp, expr, flags);
		if (expr->term.type == EXPRESSION_BINARY) {
			/* fixme: implement sub-expression resolve in literal resolver */
			azo_compiler_resolve_binary (expr);
		} else if (expr->term.type == EXPRESSION_LITERAL_ARRAY) {
			azo_compiler_resolve_array_literal (expr);
		}
	}
	return expr;
}

AZOExpression *
azo_compiler_resolve_frame (AZOCompiler *comp, AZOExpression *expr)
{
	AZOExpression *child;
	unsigned int result, ret_is_last = 0;
	for (AZOVariable *var = comp->current->scope->variables; var; var = var->next) {
		var->const_expr = NULL;
	}
	for (child = expr->children; child; child = child->next) {
		if (AZO_EXPRESSION_IS (child, EXPRESSION_KEYWORD, AZO_KEYWORD_RETURN)) {
			result = resolve_return (comp, child, 0);
			ret_is_last = 1;
		} else {
			child = azo_compiler_resolve_expression (comp, child, 0, &result);
			ret_is_last = 0;
		}
		if (result) break;
	}
	if (comp->current->ret_type && !ret_is_last) {
		fprintf (stderr, "azo_compiler_resolve_frame: Missing return statement\n");
	}
	azo_frame_reserve_data (comp->current, comp->current->n_parent_vars);
	return expr;
}
