#define __AZO_COMPILER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

static const int debug = 0;

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <arikkei/arikkei-iolib.h>
#include <arikkei/arikkei-strlib.h>
#include <az/classes/active-object.h>
#include <az/class.h>
#include <az/string.h>
#include <az/function.h>
#include <az/function-value.h>
#include <az/field.h>

#include <azo/parser.h>
#include <azo/optimizer.h>
#include <azo/compiled-function.h>
/* Bytecodes */
#include <azo/bytecode.h>
#include <azo/keyword.h>

#include <azo/compiler/arithmetic.h>
#include <azo/compare.h>

#include <azo/compiler/compiler.h>

typedef struct _LValue LValue;

enum {
	/* Stack variable, left is relative position */
	LVALUE_STACK,
	/* Property or attribute, stack(1) is object, stack(0) is property name */
	LVALUE_MEMBER,
	/* Array element, stack(1) is array, stack(0) is index */
	LVALUE_ELEMENT,
	/* Constant */
	LVALUE_VALUE
};

struct _LValue {
	unsigned int type;
	/* Variable location */
	unsigned int pos;
	/* Number of elements pushed */
	unsigned int n_elements;
};

void
azo_compiler_init(AZOCompiler *compiler, AZOContext *ctx)
{
	memset (compiler, 0, sizeof (AZOCompiler));
	compiler->ctx = ctx;
	compiler->check_args = 1;
}

void
azo_compiler_finalize(AZOCompiler *compiler)
{
	if (compiler->current) azo_frame_delete_tree(compiler->current);
}

void
azo_compiler_push_frame (AZOCompiler *comp, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type)
{
	AZOFrame *frame = azo_frame_new (comp->current, this_impl, this_inst, ret_type);
	frame->parent = comp->current;
	comp->current = frame;
}

AZOFrame *
azo_compiler_pop_frame (AZOCompiler *comp)
{
	assert (comp->current->parent);
	AZOFrame *frame = comp->current;
	comp->current = comp->current->parent;
	return frame;
}

void
azo_compiler_declare_variable (AZOCompiler *comp, AZString *name, unsigned int type)
{
	unsigned int result;
	if (!azo_frame_declare_variable (comp->current, name, type, &result)) {
		fprintf (stderr, "azo_compiler_declare_variable: Variable %s is already defined in current scope\n", name->str);
	}
}

void
azo_compiler_write_ic(AZOCompiler *comp, unsigned int ic, const AZOExpression *expr)
{
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic(&comp->current->code, ic, expr);
}

/* New stack methods */

static void
write_tc_u8 (AZOCompiler *comp, unsigned int ic, uint8_t val, const AZOExpression *expr)
{
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic_u8(&comp->current->code, ic, val, expr);
}

static void
write_tc_u32 (AZOCompiler *comp, unsigned int ic, uint32_t val, const AZOExpression *expr)
{
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic_u32 (&comp->current->code, ic, val, expr);
}

static void
write_tc_i32 (AZOCompiler *comp, uint8_t ic, int32_t val, const AZOExpression *expr)
{
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic_i32 (&comp->current->code, ic, val, expr);
}

static void
write_tc_u8_u32 (AZOCompiler *comp, unsigned int ic, uint8_t val1, uint32_t val2, const AZOExpression *expr)
{
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic_u8_u32(&comp->current->code, ic, val1, val2, expr);
}

#define write_DEBUG_STACK azo_compiler_write_DEBUG_STACK
#define write_DEBUG_STRING azo_compiler_write_DEBUG_STRING

void
azo_compiler_write_DEBUG_STACK (AZOCompiler *comp)
{
	write_tc_u32 (comp, AZO_TC_DEBUG, 1, NULL);
}

void
azo_compiler_write_DEBUG_STRING (AZOCompiler *comp, const char *text, const AZOExpression *expr)
{
	AZString *str = az_string_new((const uint8_t *) text);
	unsigned int pos = azo_frame_append_string(comp->current, str);
	azo_code_write_ic_u32_u32(&comp->current->code, AZO_TC_DEBUG, 2, pos, expr);
	az_string_unref (str);
}

void
azo_compiler_write_DEBUG_STRING_len (AZOCompiler *comp, const uint8_t *text, unsigned int len, const AZOExpression *expr)
{
	AZString *str = az_string_new_length(text, len);
	unsigned int pos = azo_frame_append_string(comp->current, str);
	azo_code_write_ic_u32_u32(&comp->current->code, AZO_TC_DEBUG, 2, pos, expr);
	az_string_unref (str);
}

void
azo_compiler_write_EXCEPTION (AZOCompiler *comp, uint32_t type, const AZOExpression *expr)
{
	azo_code_write_ic_u32(&comp->current->code, AZO_TC_EXCEPTION, type, expr);
}

void
azo_compiler_write_EXCEPTION_COND (AZOCompiler *comp, unsigned int tc, uint32_t type, const AZOExpression *expr)
{
	azo_code_write_ic_u32 (&comp->current->code, tc, type, expr);
}

void
azo_compiler_write_PUSH_FRAME (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u32 (comp, AZO_TC_PUSH_FRAME, pos, NULL);
}

void
azo_compiler_write_POP (AZOCompiler *comp, uint32_t n_values, const AZOExpression *expr)
{
	write_tc_u32 (comp, AZO_TC_POP, n_values, expr);
}

void
azo_compiler_write_REMOVE (AZOCompiler *comp, unsigned int first, unsigned int n_values, const AZOExpression *expr)
{
	unsigned int ic = AZO_TC_REMOVE;
	if (comp->check_args) ic |= AZO_TC_CHECK_ARGS;
	azo_code_write_ic_u32_u32(&comp->current->code, ic, first, n_values, expr);
}

void
azo_compiler_write_PUSH_EMPTY (AZOCompiler *comp, uint32_t type, const AZOExpression *expr)
{
	write_tc_u32 (comp, AZO_TC_PUSH_EMPTY, type, expr);
}

void
azo_compiler_write_PUSH_IMMEDIATE (AZOCompiler *comp, unsigned int type, const AZValue *val, const AZOExpression *expr)
{
	uint8_t ic8 = PUSH_IMMEDIATE;
	if (comp->check_args) ic8 |= AZO_TC_CHECK_ARGS;
	azo_code_write_bc(&comp->current->code, &ic8, 1, expr);
	uint8_t t8 = type;
	azo_code_write_bc(&comp->current->code, &t8, 1, expr);
	if (type) {
		unsigned int val_size = az_class_value_size(AZ_CLASS_FROM_TYPE(type));
		if (val_size) {
			azo_code_write_bc(&comp->current->code, val, val_size, expr);
		}
	}
}

void
azo_compiler_write_DUPLICATE (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u32 (comp, DUPLICATE, pos, NULL);
}

void
azo_compiler_write_DUPLICATE_FRAME (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u32 (comp, DUPLICATE_FRAME, pos, NULL);
}

void
azo_compiler_write_EXCHANGE (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u32 (comp, EXCHANGE, pos, NULL);
}

void
azo_compiler_write_TEST_TYPE (AZOCompiler *comp, unsigned int typecode, unsigned int pos)
{
	write_tc_u8 (comp, typecode, (uint8_t) pos, NULL);
}

void
azo_compiler_write_TEST_TYPE_IMMEDIATE (AZOCompiler *comp, unsigned int typecode, unsigned int pos, unsigned int type)
{
	write_tc_u8_u32 (comp, typecode, (uint8_t) pos, type, NULL);
}

void
azo_compiler_write_TYPE_OF (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u8 (comp, TYPE_OF, (uint8_t) pos, NULL);
}

unsigned int
azo_compiler_write_JMP_32 (AZOCompiler *comp, unsigned int ic, unsigned int to, const AZOExpression *expr)
{
	unsigned int pos = comp->current->code.bc_len;
	int32_t raddr = (int) to - (int) (pos + 5);
	write_tc_i32 (comp, ic, raddr, expr);
	return pos;
}

void
azo_compiler_update_JMP_32 (AZOCompiler *comp, unsigned int from)
{
	azo_frame_update_JMP_to (comp->current, from);
}

void
azo_compiler_write_PROMOTE (AZOCompiler *comp, uint8_t pos)
{
	write_tc_u8 (comp, PROMOTE, pos, NULL);
}

void
azo_compiler_write_EQUAL_TYPED (AZOCompiler *comp, uint8_t type)
{
	write_tc_u8 (comp, EQUAL_TYPED, type, NULL);
}

void
azo_compiler_write_COMPARE_TYPED (AZOCompiler *comp, uint8_t type)
{
	write_tc_u8 (comp, COMPARE_TYPED, type, NULL);
}

void
azo_compiler_write_ARITHMETIC_TYPED (AZOCompiler *comp, unsigned int typecode, uint8_t type)
{
	write_tc_u8 (comp, typecode, type, NULL);
}

void
azo_compiler_write_MINMAX_TYPED (AZOCompiler *comp, unsigned int typecode, uint8_t type)
{
	write_tc_u8 (comp, typecode, type, NULL);
}

/* End new stack methods */

static void
azo_compiler_write_PUSH_VALUE (AZOCompiler *comp, unsigned int pos)
{
	write_tc_u32 (comp, AZO_TC_PUSH_VALUE, pos, NULL);
}

static void
compile_PUSH_VALUE (AZOCompiler *comp, unsigned int type, const AZValue *val)
{
	unsigned int pos = azo_frame_append_value (comp->current, type, val);
	write_tc_u32 (comp, AZO_TC_PUSH_VALUE, pos, NULL);
}

static void
compile_PUSH_VALUE_string (AZOCompiler *comp, AZString *str)
{
	unsigned int pos = azo_frame_append_string (comp->current, str);
	write_tc_u32 (comp, AZO_TC_PUSH_VALUE, pos, NULL);
}

static void
compile_PUSH_VALUE_object (AZOCompiler *comp, AZObject *obj)
{
	unsigned int pos = azo_frame_append_object (comp->current, obj);
	write_tc_u32 (comp, AZO_TC_PUSH_VALUE, pos, NULL);
}

/* End temporary */

static unsigned int compile_program (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_sentences (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_sentence (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_block (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_step_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_declaration (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_single_declaration (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src, unsigned int type);
static unsigned int compile_silent_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_assign (AZOCompiler *comp, const AZOExpression *left, const AZOExpression *right, const AZOSource *src);
/* Expressions */
static unsigned int compile_variable_reference (AZOCompiler *comp, const AZOExpression *expr, unsigned int type, const AZOSource *src);
static unsigned int compile_singular_reference (AZOCompiler *comp, const AZOExpression *expr);
static unsigned int compile_member_reference (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);
static unsigned int compile_array_reference (AZOCompiler *comp, const AZOExpression *aref, const AZOExpression *index, const AZOSource *src);
static unsigned int compile_array_literal (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);

static unsigned int compile_new (AZOCompiler *comp, const AZOExpression *klass, const AZOExpression *list, const AZOSource *src);
static unsigned int compile_function_call (AZOCompiler *comp, const AZOExpression *function, const AZOExpression *list, const AZOSource *src, unsigned int silent);
static unsigned int compile_expression_lvalue (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);

static unsigned int
azo_compiler_compile_constant (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (expr->term.subtype == AZ_TYPE_NONE) {
		/* Constant none is NULL */
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_NONE, NULL, expr);
	} else if (AZ_TYPE_IS_PRIMITIVE (expr->term.subtype)) {
		azo_compiler_write_PUSH_IMMEDIATE (comp, expr->term.subtype, &expr->value.v, expr);
	} else if (expr->term.subtype == AZ_TYPE_STRING) {
		compile_PUSH_VALUE_string (comp, expr->value.v.string);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_OBJECT)) {
		compile_PUSH_VALUE_object (comp, ( AZObject *) expr->value.v.reference);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_REFERENCE)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE (comp, expr->term.subtype, &expr->value.v);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_BLOCK)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE (comp, expr->term.subtype, &expr->value.v);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_FUNCTION_VALUE)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE (comp, expr->term.subtype, &expr->value.v);
	} else {
		fprintf (stderr, "azo_compiler_compile_constant: Unknown constant type %u\n", expr->term.subtype);
		return 0;
	}
	return 1;
}

/**
 * @brief Assign top of stack to already compiled lvalue
 * 
 * Only LVALUE_STACK, LVALUE_MEMBER and LVALUE_ELEMENT allowed
 * 
 */

static unsigned int
compile_assign_to_lvalue (AZOCompiler *comp, LValue *lval, const AZOExpression *expr)
{
	if (lval->type == LVALUE_STACK) {
		/* [..., prev, ..., value] */
		write_tc_u32 (comp, AZO_TC_EXCHANGE_FRAME, lval->pos, expr);
		/* [..., value, ..., prev] */
		azo_compiler_write_POP (comp, 1, expr);
		/* [..., value, ...] */
	} else if (lval->type == LVALUE_MEMBER) {
		/* [instance, key, value] */
		azo_compiler_write_ic (comp, AZO_TC_SET_PROPERTY, expr);
		/* [true] */
		/* [instance, key, value, false] */
		unsigned int finished = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0, expr);
		/* [instance, key, value] */
		azo_compiler_write_ic (comp, AZO_TC_SET_ATTRIBUTE, expr);
		/* [] */
		azo_compiler_update_JMP_32 (comp, finished);
	} else if (lval->type == LVALUE_ELEMENT) {
		/* [array, index, value] */
		azo_compiler_write_ic (comp, WRITE_ARRAY_ELEMENT, expr);
		/* [array] */
		azo_compiler_write_POP (comp, 1, expr);
		/* [] */
	} else {
		fprintf (stderr, "Unassignable lvalue type\n");
		return 0;
	}
	return 1;
}

/* Compile LValue expression into LValue structure */
/* Only variable, reference, function call and array element expressions are allowed here */

static unsigned int
compile_lvalue (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src, LValue *lvalue, unsigned int read_only)
{
	if (expr->term.type == EXPRESSION_VARIABLE) {
		if (expr->term.subtype == VARIABLE_LOCAL) {
			/*
			 * Declared in current instance
			 *
			 * No code, bare LValue
			 */
			lvalue->type = LVALUE_STACK;
			//lvalue->pos = comp->current->n_sig_vars + comp->current->n_parent_vars + expr->var_pos;
			lvalue->pos = expr->var_pos;
			lvalue->n_elements = 0;
			return 1;
		} else {
			/*
			 * Declared in parent frame
			 *
			 * No code, bare LValue
			 */
			if (!read_only) {
				fprintf (stderr, "Parent variable in writable lvalue\n");
				return 0;
			}
			lvalue->type = LVALUE_VALUE;
			lvalue->pos = expr->var_pos;
			lvalue->n_elements = 0;
			return 1;
		}
	} else if (expr->term.type == EXPRESSION_REFERENCE) {
		if (expr->term.subtype == REFERENCE_VARIABLE) {
			/* Did not resolve to stack variable */
			/* Interpret as this member */
			lvalue->type = LVALUE_MEMBER;
			azo_compiler_write_DUPLICATE_FRAME (comp, 0);
			compile_PUSH_VALUE_string (comp, expr->value.v.string);
			lvalue->n_elements = 2;
			return 1;
		} else if (expr->term.subtype == REFERENCE_MEMBER) {
			lvalue->type = LVALUE_MEMBER;
			AZOExpression *left = expr->children;
			AZOExpression *right = left->next;
			if (!azo_compiler_compile_expression (comp, left, src)) return 0;
			compile_PUSH_VALUE_string (comp, right->value.v.string);
			lvalue->n_elements = 2;
		} else {
			fprintf (stderr, "compile_lvalue: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
	} else if (expr->term.type == EXPRESSION_ARRAY_ELEMENT) {
		lvalue->type = LVALUE_ELEMENT;
		AZOExpression *left = expr->children;
		AZOExpression *right = left->next;
		if (!azo_compiler_compile_expression (comp, left, src)) return 0;
		if (!azo_compiler_compile_expression (comp, right, src)) return 0;
		lvalue->n_elements = 2;
	} else {
		fprintf (stderr, "compile_lvalue: Invalid expression type\n");
		return 0;
	}
	return 1;
}

static void
compile_type_exception(AZOCompiler *comp, const AZOExpression *expr, unsigned int type, const AZOSource *src)
{
	azo_code_write_ic_u32_u32 (&comp->current->code, AZO_TC_EXCEPTION_IF_TYPE_IS_NOT, 0, type, expr);
}

static unsigned int
compile_expression_boolean (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (!azo_compiler_compile_expression (comp, expr, src)) return 0;
	compile_type_exception(comp, expr, AZ_TYPE_BOOLEAN, src);
	return 1;
}

static unsigned int
compile_call (AZOCompiler *comp, const AZOExpression *list, const AZOSource *src, unsigned int has_this, unsigned int test_implementation)
{
	unsigned int is_function;
	unsigned int n_args;
	AZOExpression *child;
	/* [func] */
#if 0
	azo_compiler_write_DEBUG_STRING (comp, "compile_call 1");
	azo_compiler_write_DEBUG_STACK (comp);
#endif
	/* [func] */
	if (test_implementation) {
		azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 0, AZ_TYPE_FUNCTION);
		is_function = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0, NULL);
		azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE, NULL);
		azo_compiler_update_JMP_32 (comp, is_function);
	}
	if (has_this) {
		azo_compiler_write_DUPLICATE (comp, 1);
	} else {
		azo_compiler_write_DUPLICATE_FRAME (comp, 0);
	}
	/* [func, this] */
	n_args = 1;
	for (child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, child, src);
		n_args += 1;
	}
	if (n_args > 32) {
		fprintf(stderr, "Too many function arguments: %d\n", n_args);
		return 1;
	}
	/* [func, this, arg1...] */
	azo_compiler_write_PUSH_FRAME (comp, n_args);
	/* [func : this, arg1...] */
	write_tc_u8 (comp, AZO_TC_INVOKE, n_args, NULL);
	/* [func : this, arg1..., result] */
	azo_compiler_write_ic (comp, AZO_TC_POP_FRAME, NULL);
	/* [func, this, arg1..., result] */
	azo_compiler_write_REMOVE (comp, 1, n_args + 1, NULL);
	/* [result] */
	return 0;
}

static void
compile_call_inst_func_args (AZOCompiler *comp, unsigned int n_args)
{
	/* [inst, funcobj, arg1...] */
	azo_compiler_write_PUSH_FRAME (comp, n_args);
	/* [inst, funcobj : arg1...] */
	write_tc_u8 (comp, AZO_TC_INVOKE, n_args, NULL);
	/* [inst, funcobj : arg1..., retval] */
	azo_compiler_write_ic (comp, AZO_TC_POP_FRAME, NULL);
	/* [inst, funcobj, arg1..., retval] */
	azo_compiler_write_REMOVE (comp, 1, n_args + 2, NULL);
	/* [retval] */
}

static void
compile_TEST_TYPE_IMMEDIATE (AZOCompiler *comp, unsigned int tc, unsigned int pos, unsigned int type, unsigned int *jmp_if, unsigned int *jmp_if_not)
{
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, tc, pos, type);
	if (jmp_if) {
		*jmp_if = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0, NULL);
		if (jmp_if_not) {
			*jmp_if_not = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);
		}
	} else if (jmp_if_not) {
		*jmp_if_not = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, NULL);
	}
}

static void
compile_IS_NONE (AZOCompiler *comp, unsigned int *jmp_if, unsigned int *jmp_if_not)
{
	compile_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, jmp_if, jmp_if_not);
}

static unsigned int
compile_call_member (AZOCompiler *comp, const AZOSource *src, unsigned int n_args)
{
	unsigned int is_member_function, is_class, not_active_obj, no_static_function, invalid_type, finished, finished_2, finished_3;

	/* Try GET_FUNCTION */
	/* Instance, String, Arguments */
#if 0
	azo_compiler_write_DEBUG_STRING (comp, "compile_call_member 1");
	azo_compiler_write_DEBUG_STACK (comp);
#endif
	write_tc_u8 (comp, AZO_TC_GET_FUNCTION, n_args, NULL);
	/* Instance, String, Arguments, Function|null */
	compile_IS_NONE (comp, NULL, &is_member_function);

	/* Instance, String, Arguments, null */
	azo_compiler_write_POP (comp, 1, NULL);
	/* Instance, String, Arguments */
	compile_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, n_args + 1, AZ_TYPE_CLASS, &is_class, NULL);

	/* Try GET_ATTRIBUTE */
	/* Instance, String, Arguments */
	compile_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, n_args + 1, AZ_TYPE_ATTRIBUTE_DICT, NULL, &not_active_obj);
	/* ActiveObj, String, Arguments */
	azo_compiler_write_DUPLICATE (comp, n_args + 1);
	azo_compiler_write_DUPLICATE (comp, n_args + 1);
	azo_compiler_write_ic (comp, GET_ATTRIBUTE, NULL);
	/* ActiveObj, String, Arguments, Value|null */
	compile_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 0, AZ_TYPE_FUNCTION, NULL, &invalid_type);
	/* ActiveObj, String, Arguments, Function */

	/* Invoke member function */
	azo_compiler_update_JMP_32 (comp, is_member_function);
	/* Instance, String, Arguments, Function */
	azo_compiler_write_EXCHANGE (comp, n_args + 1);
	/* Instance, Function, Arguments, String */
	azo_compiler_write_POP (comp, 1, NULL);
	/* Instance, Function, Arguments */
	compile_call_inst_func_args (comp, n_args);
	/* Retval */
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);

	/* Try GET_STATIC_FUNCTION */
	azo_compiler_update_JMP_32 (comp, is_class);
	n_args -= 1;
	/* Class, String, Class, Arguments */
	azo_compiler_write_REMOVE (comp, n_args, 1, NULL);
	/* Class, String, Arguments */
	write_tc_u8 (comp, AZO_TC_GET_STATIC_FUNCTION, n_args, NULL);
	/* Class, String, Arguments, Function | null */
	compile_IS_NONE (comp, &no_static_function, NULL);

	/* Class, String, Arguments, Function */
	azo_compiler_write_EXCHANGE (comp, n_args + 1);
	/* Class, Function, Arguments, String */
	azo_compiler_write_POP (comp, 1, NULL);
	/* Class, Function, Arguments */
	compile_call_inst_func_args (comp, n_args);
	/* Retval */
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);

	/* Value */
	azo_compiler_update_JMP_32 (comp, no_static_function);
	azo_compiler_update_JMP_32 (comp, not_active_obj);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_PROPERTY, NULL);
	finished_3 = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);
	azo_compiler_update_JMP_32 (comp, invalid_type);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE, NULL);
	azo_compiler_update_JMP_32 (comp, finished);
	azo_compiler_update_JMP_32 (comp, finished_2);
	azo_compiler_update_JMP_32 (comp, finished_3);
	return 1;


}

#define noDEBUG_NEW

static unsigned int
compile_new (AZOCompiler *comp, const AZOExpression *klass, const AZOExpression *list, const AZOSource *src)
{
	AZOExpression *child;
	unsigned int not_class, invalid_type_2, finished;
	static AZString *newstr = NULL;
	unsigned int n_args = 0;
	if (!newstr) newstr = az_string_new ((const unsigned char *) "new");

#ifdef DEBUG_NEW
	write_DEBUG_STRING (comp, "compile_new: 1\n");
	write_DEBUG_STACK (comp);
#endif
	azo_compiler_compile_expression (comp, klass, src);
	/* [Value] */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_CLASS);
	not_class = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, NULL);
	/* [Class] */
	compile_PUSH_VALUE_string (comp, newstr);
	/* [Class, "new"] */
	for (child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, child, src);
		n_args += 1;
	}
	if (n_args > 32) {
		fprintf(stderr, "Too many function arguments: %d\n", n_args);
		return 0;
	}
	/* [Class, "new", arg1...] */
	write_tc_u8 (comp, AZO_TC_GET_STATIC_FUNCTION, n_args, NULL);
	/* [Class, "new", arg1..., funcobj | null] */
	compile_IS_NONE (comp, &invalid_type_2, NULL);
	/* [Class, "new", arg1..., funcobj] */
	azo_compiler_write_EXCHANGE (comp, n_args + 1);
	/* [Class, funcobj, arg1..., "new"] */
	azo_compiler_write_POP (comp, 1, NULL);
	/* [Class, funcobj, arg1...] */
	compile_call_inst_func_args (comp, n_args);
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);
	/* Invalid type */
	azo_compiler_update_JMP_32 (comp, not_class);
	azo_compiler_update_JMP_32 (comp, invalid_type_2);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE, NULL);
	/* Finished */
	azo_compiler_update_JMP_32 (comp, finished);

	return 1;
}

static unsigned int
compile_prefix_arithmetic (AZOCompiler *comp, const AZOExpression *expr, const AZOExpression *left, const AZOSource *src, unsigned int silent)
{
	LValue lval;
	if (silent) {
		if (!compile_lvalue (comp, left, src, &lval, 0)) return 0;
		/* LValue */
		if (expr->term.subtype == PREFIX_INCREMENT) {
			if (!azo_compiler_compile_increment (comp, left, expr, src)) return 0;
		} else if (expr->term.subtype == PREFIX_DECREMENT) {
			if (!azo_compiler_compile_decrement (comp, left, expr, src)) return 0;
		} else {
			fprintf (stderr, "compile_prefix_arithmetic: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
		/* LValue, Value */
		compile_assign_to_lvalue (comp, &lval, NULL);
	} else {
		azo_compiler_write_PUSH_EMPTY (comp, AZ_TYPE_NONE, expr);
		/* null */
		if (!compile_lvalue (comp, left, src, &lval, 0)) return 0;
		/* null, [LValue] */
		if (expr->term.subtype == PREFIX_INCREMENT) {
			if (!azo_compiler_compile_increment (comp, left, expr, src)) return 0;
		} else if (expr->term.subtype == PREFIX_DECREMENT) {
			if (!azo_compiler_compile_decrement (comp, left, expr, src)) return 0;
		} else {
			fprintf (stderr, "compile_prefix_arithmetic: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
		/* null, [LValue], Value */
		azo_compiler_write_EXCHANGE (comp, lval.n_elements + 1);
		/* Value, [LValue], null */
		azo_compiler_write_DUPLICATE (comp, lval.n_elements + 1);
		/* Value, [LValue], Value */
		compile_assign_to_lvalue (comp, &lval, NULL);
		/* Value */
	}

	return 1;
}

static unsigned int
compile_prefix (AZOCompiler *comp, const AZOExpression *expr, const AZOExpression *left, const AZOSource *src)
{
	if (expr->term.subtype == PREFIX_PLUS) {
		if (!azo_compiler_compile_expression (comp, left, src)) return 0;
		/* NOP */
	} else if (expr->term.subtype == PREFIX_MINUS) {
		if (!azo_compiler_compile_expression (comp, left, src)) return 0;
		azo_compiler_write_ic (comp, AZO_TC_NEGATE, NULL);
	} else if (expr->term.subtype == PREFIX_NOT) {
		if (!azo_compiler_compile_expression (comp, left, src)) return 0;
		azo_compiler_write_ic (comp, AZO_TC_LOGICAL_NOT, NULL);
	} else if (expr->term.subtype == PREFIX_TILDE) {
		if (!azo_compiler_compile_tilde (comp, left, src)) return 0;
	} else if (expr->term.subtype == PREFIX_INCREMENT) {
		if (!compile_prefix_arithmetic (comp, expr, left, src, 0)) return 0;
	} else if (expr->term.subtype == PREFIX_DECREMENT) {
		if (!compile_prefix_arithmetic (comp, expr, left, src, 0)) return 0;
	} else {
		fprintf (stderr, "compile_prefix: Unimplemented or unknown prefix type %u\n", left->term.subtype);
		return 0;
	}
	return 1;
}

#define noDEBUG_SUFFIX

static unsigned int
compile_suffix (AZOCompiler *comp, const AZOExpression *expr, const AZOExpression *left, const AZOSource *src, unsigned int silent)
{
	LValue lval;
	if (!silent) {
		/* Push original value */
		/* fixme: Do it more intelligently */
		if (!azo_compiler_compile_expression (comp, left, src)) return 0;
	}
	/* Set variable target */
	if (!compile_lvalue (comp, left, src, &lval, 0)) return 0;
	/* Calculate new value */
	if (expr->term.subtype == SUFFIX_INCREMENT) {
#ifdef DEBUG_SUFFIX
		write_DEBUG_STRING (comp, "compile_suffix: 1\n");
		write_DEBUG_STACK (comp);
#endif
		if (!azo_compiler_compile_increment (comp, left, expr, src)) return 0;
#ifdef DEBUG_SUFFIX
		write_DEBUG_STRING (comp, "compile_suffix: 2\n");
		write_DEBUG_STACK (comp);
#endif
	} else if (expr->term.subtype == SUFFIX_DECREMENT) {
		if (!azo_compiler_compile_decrement (comp, left, expr, src)) return 0;
	} else {
		fprintf (stderr, "compile_suffix: Invalid expression subtype %u\n", expr->term.subtype);
		return 0;
	}
	compile_assign_to_lvalue (comp, &lval, NULL);
#ifdef DEBUG_SUFFIX
	write_DEBUG_STRING (comp, "compile_suffix: 3\n");
	write_DEBUG_STACK (comp);
#endif
	return 1;
}

static unsigned int
compile_reference_lookup (AZOCompiler *comp, const AZOExpression *expr, AZString *str)
{
	unsigned int property_not_null, not_active_obj, finished;

	/* InstanceA */
	azo_compiler_write_DUPLICATE (comp, 0);
	compile_PUSH_VALUE_string (comp, str);
	/* InstanceA, InstanceA, String */
	azo_compiler_write_ic (comp, AZO_TC_GET_PROPERTY, NULL);
	/* InstanceA, Value|null */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE);
	property_not_null = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, NULL);

	/* InstanceA, null */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 1, AZ_TYPE_ATTRIBUTE_DICT);
	not_active_obj = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, NULL);
	/* ActiveObj, null */
	azo_compiler_write_POP (comp, 1, NULL);
	compile_PUSH_VALUE_string (comp, str);
	/* ActiveObj, String */
	azo_compiler_write_ic (comp, GET_ATTRIBUTE, NULL);
	/* Value */
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0, NULL);

	azo_compiler_update_JMP_32 (comp, property_not_null);
	azo_compiler_update_JMP_32 (comp, not_active_obj);
	/* InstanceA, Value */
	azo_compiler_write_REMOVE (comp, 1, 1, NULL);
	/* Value */
	azo_compiler_update_JMP_32 (comp, finished);
	return 1;
}

static unsigned int
compile_this_reference (AZOCompiler *comp, const AZOExpression *expr, AZString *id)
{
	/* */
	azo_compiler_write_DUPLICATE_FRAME (comp, 0);
	/* This */
	if (!compile_reference_lookup (comp, expr, id)) return 0;
	/* Value */
	return 1;
}

#define noDEBUG_PARENT_LVAL

static unsigned int
compile_function_call (AZOCompiler *comp, const AZOExpression *func, const AZOExpression *list, const AZOSource *src, unsigned int silent)
{
	AZOExpression *child;
	unsigned int n_args, result;
	LValue lval;

	// fixme: Handle in lvalue?
	// fixme: Implement separate expression type for this handling?
	if (func->term.type == EXPRESSION_CONSTANT) {
		if (func->children) {
			assert (func->children->term.type == EXPRESSION_CONSTANT);
			if (!azo_compiler_compile_constant (comp, func->children, src)) return 0;
			if (!azo_compiler_compile_constant (comp, func, src)) return 0;
			result = compile_call (comp, list, src, 1, 0);
			if (result) return 0;
			azo_compiler_write_REMOVE (comp, 1, 1, NULL);
		} else {
			if (!azo_compiler_compile_constant (comp, func, src)) return 0;
			result = compile_call (comp, list, src, 0, 0);
			if (result) return 0;
		}
		if (silent) {
			azo_compiler_write_POP (comp, 1, NULL);
		}
		return 1;
	}

	if (!compile_lvalue (comp, func, src, &lval, 1)) return 0;
	switch (lval.type) {
	case LVALUE_STACK:
		/* - */
		write_tc_u32 (comp, DUPLICATE_FRAME, lval.pos, NULL);
		/* Value */
		result = compile_call (comp, list, src, 0, 1);
		if (result) return 0;
		break;
	case LVALUE_MEMBER:
		/* Instance, Key */
		azo_compiler_write_DUPLICATE (comp, 1);
		n_args = 1;
		for (child = list->children; child; child = child->next) {
			azo_compiler_compile_expression (comp, child, src);
			n_args += 1;
		}
		/* Instance, Key, Instance, Arguments */
		compile_call_member (comp, src, n_args);
		break;
	case LVALUE_ELEMENT:
		/* Array, Index */
		azo_compiler_write_ic (comp, LOAD_ARRAY_ELEMENT, NULL);
		/* Array, Value */
		azo_compiler_write_REMOVE (comp, 1, 1, NULL);
		/* Value */
		/* fixme: Handle object.array[index](args) this types */
		result = compile_call (comp, list, src, 0, 1);
		if (result) return 0;
		break;
	case LVALUE_VALUE:
		/* - */
#ifdef DEBUG_PARENT_LVAL
		write_DEBUG_STRING (comp, "Parent lval 1\n");
		write_DEBUG_STACK (comp);
#endif
		azo_compiler_write_PUSH_VALUE (comp, lval.pos);
#ifdef DEBUG_PARENT_LVAL
		write_DEBUG_STRING (comp, "Parent lval 2\n");
		write_DEBUG_STACK (comp);
#endif
		/* Value */
		result = compile_call (comp, list, src, 0, 1);
		if (result) return 0;
		break;
	default:
		break;
	}
	if (silent) {
		azo_compiler_write_POP (comp, 1, NULL);
	}
	return 1;
}

#define noDEBUG_FUNCTION

static unsigned int
compile_function (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *obj, *type, *args, *body;
	AZOExpression *child;
	AZOProgram *prog;
	AZOCompiledFunction *cfunc;
	static AZString *bound_str = NULL;
	unsigned int bound;
	if (!bound_str) bound_str = az_string_new ((const unsigned char *) "bound");
#ifdef DEBUG_FUNCTION
	write_DEBUG_STRING (comp, "Function 1");
	write_DEBUG_STACK (comp);
#endif
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
	assert (type->term.type == EXPRESSION_TYPE);
	unsigned int ret_type = type->term.subtype;

	unsigned int n_args = 0;
	for (child = args->children; child; child = child->next) n_args += 1;

	AZOFrame *prev = comp->current;
	comp->current = expr->frame;

	prog = azo_compiler_compile (comp, body, 0, src);
	if (!prog) {
		fprintf (stderr, "compile_function: error compiling function\n");
		comp->current = prev;
		return 0;
	}
	cfunc = azo_compiled_function_new (comp->ctx, prog, ret_type, n_args);
	comp->current = prev;

	compile_PUSH_VALUE_object (comp, AZ_OBJECT (cfunc));
	/* Function */
	azo_compiler_write_DUPLICATE (comp, 0);
	/* Function, Function */
	compile_PUSH_VALUE_string (comp, bound_str);
	/* Function, Function, "bound" */
	azo_compiler_write_ic (comp, AZO_TC_GET_PROPERTY, NULL);
	/* Function, Boolean */
	bound = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0, NULL);
	/* Function */
	/* Bind function */

	for (AZOVariable *var = expr->frame->parent_vars; var; var = var->next) {
		if (var->parent_is_val) {
			azo_compiler_write_PUSH_VALUE (comp, var->parent_pos);
		} else {
			azo_compiler_write_DUPLICATE_FRAME (comp, var->parent_pos);
		}
	}
	write_tc_u32 (comp, AZO_TC_BIND, expr->frame->n_parent_vars, NULL);
	//write_tc_u32 (comp, AZO_TC_BIND, 0);

	/* Function is already bound */
	azo_compiler_update_JMP_32 (comp, bound);
#ifdef DEBUG_FUNCTION
	write_DEBUG_STRING (comp, "Function\n");
	write_DEBUG_STACK (comp);
#endif
	az_object_unref ((AZObject *) cfunc);
	return 1;
}

static unsigned int
compile_array_literal (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	const AZOExpression *child;
	unsigned int size = 0, idx = 0;
	for (child = expr->children; child; child = child->next) size += 1;
	/* Create new array */
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &size, NULL);
	azo_compiler_write_ic (comp, NEW_ARRAY, NULL);
	for (child = expr->children; child; child = child->next) {
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &idx, NULL);
		azo_compiler_compile_expression (comp, child, src);
		azo_compiler_write_ic (comp, WRITE_ARRAY_ELEMENT, NULL);
		idx += 1;
	}
	return 1;
}

#define noDEBUG_TEST

static unsigned int
compile_test (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *lhs, *rhs;
	lhs = expr->children;
	rhs = lhs->next;
	/* fixme: Implement expression type? */
#ifdef DEBUG_TEST
	write_DEBUG_STRING (comp, "Test 1\n");
	write_DEBUG_STACK (comp);
#endif
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, rhs, src)) return 0;
#ifdef DEBUG_TEST
	write_DEBUG_STRING (comp, "Test 2\n");
	write_DEBUG_STACK (comp);
#endif
	write_tc_u8 (comp, TYPE_OF_CLASS, 0, expr);
#ifdef DEBUG_TEST
	write_DEBUG_STRING (comp, "Test 3\n");
	write_DEBUG_STACK (comp);
#endif
	if (expr->term.subtype == AZO_TYPE_IS) {
		azo_compiler_write_TEST_TYPE (comp, AZO_TC_TYPE_IS, 2);
	} else {
		azo_compiler_write_TEST_TYPE (comp, AZO_TC_TYPE_IMPLEMENTS, 2);
	}
#ifdef DEBUG_TEST
	write_DEBUG_STRING (comp, "Test 4\n");
	write_DEBUG_STACK (comp);
#endif
	azo_compiler_write_REMOVE (comp, 1, 2, expr);
#ifdef DEBUG_TEST
	write_DEBUG_STRING (comp, "Test 5\n");
	write_DEBUG_STACK (comp);
#endif
	return 1;
}

/* Compile rvalue expression (this, null, literal...) */

#define noDEBUG_PARENT_VAR

static unsigned int
compile_expression_rvalue (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (expr->term.type == EXPRESSION_VARIABLE) {
		if (expr->term.subtype == VARIABLE_LOCAL) {
			azo_compiler_write_DUPLICATE_FRAME (comp, expr->var_pos);
		} else {
#ifdef DEBUG_PARENT_VAR
			write_DEBUG_STRING (comp, "Parent var 1\n");
			write_DEBUG_STACK (comp);
#endif
			azo_compiler_write_PUSH_VALUE (comp, expr->var_pos);
#ifdef DEBUG_PARENT_VAR
			write_DEBUG_STRING (comp, "Parent var 2\n");
			write_DEBUG_STACK (comp);
#endif
		}
	} else if (expr->term.type == EXPRESSION_CONSTANT) {
		if (!azo_compiler_compile_constant (comp, expr, src)) return 0;
	} else if (expr->term.type == EXPRESSION_KEYWORD) {
		if (expr->term.subtype == AZO_KEYWORD_THIS) {
			azo_compiler_write_DUPLICATE_FRAME (comp, 0);
		} else if (expr->term.subtype == AZO_KEYWORD_NEW) {
			if (!compile_new (comp, expr->children, expr->children->next, src)) return 0;
		} else {
			fprintf (stderr, "compile_expression_rvalue: Unknown keyword subtype %u\n", expr->term.subtype);
			return 0;
		}
	} else if (expr->term.type == EXPRESSION_FUNCTION) {
		if (expr->term.subtype == FUNCTION_STATIC) {
			if (!compile_function (comp, expr, src)) return 0;
		} else {
			if (!compile_function (comp, expr, src)) return 0;
		}
	} else if (expr->term.type == EXPRESSION_FUNCTION_CALL) {
		if (!compile_function_call (comp, expr->children, expr->children->next, src, 0)) return 0;
	} else if (expr->term.type == EXPRESSION_LITERAL_ARRAY) {
		if (!compile_array_literal (comp, expr, src)) return 0;
		/* fixme: Do we allow operators here? (Lauris) */
	} else if (expr->term.type == EXPRESSION_PREFIX) {
		if (!compile_prefix (comp, expr, expr->children, src)) return 0;
	} else if (expr->term.type == EXPRESSION_SUFFIX) {
		if (!compile_suffix (comp, expr, expr->children, src, 0)) return 0;
	} else if (expr->term.type == EXPRESSION_COMPARISON) {
		if (!azo_compiler_compile_comparison (comp, expr->children, expr->children->next, expr, src, 11)) return 0;
	} else if (expr->term.type == EXPRESSION_BINARY) {
		if (!azo_compiler_compile_arithmetic (comp, expr->children, expr->children->next, expr, src)) return 0;
	} else if (expr->term.type == AZO_EXPRESSION_TEST) {
		if (!compile_test (comp, expr, src)) return 0;
	} else {
		fprintf (stderr, "compile_expression_rvalue: Invalid expression type %u\n", expr->term.type);
		return 0;
	}
	return 1;
}

static unsigned int
compile_array_reference (AZOCompiler *comp, const AZOExpression *array_ref, const AZOExpression *idx, const AZOSource *src)
{
	azo_compiler_compile_expression (comp, array_ref, src);
	/* Array */
	azo_compiler_compile_expression (comp, idx, src);
	/* Array, Index */
	azo_compiler_write_ic (comp, LOAD_ARRAY_ELEMENT, NULL);
	/* Array, Value */
	azo_compiler_write_REMOVE (comp, 1, 1, NULL);
	return 1;
}

static unsigned int
compile_member_reference (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *left, *right;
	left = expr->children;
	right = left->next;
	azo_compiler_compile_expression (comp, left, src);
	if (!compile_reference_lookup (comp, expr, right->value.v.string)) return 0;
	return 1;
}

static unsigned int
compile_singular_reference (AZOCompiler *comp, const AZOExpression *expr)
{
	AZOVariable *var = azo_frame_lookup_var (comp->current, expr->value.v.string);
	if (var) {
		/* We have variable reference that is not resolved */
		fprintf (stderr, "compile_singular_reference: Internal error - variable %s is not resolved\n", expr->value.v.string->str);
		return 0;
	} else {
		compile_this_reference (comp, expr, expr->value.v.string);
	}
	return 1;
}

static unsigned int
compile_variable_reference (AZOCompiler *comp, const AZOExpression *expr, unsigned int type, const AZOSource *src)
{
	if (type == REFERENCE_VARIABLE) {
		if (!compile_singular_reference (comp, expr)) return 0;
	} else if (type == REFERENCE_MEMBER) {
		if (!compile_member_reference (comp, expr, src)) return 0;
	} else {
		fprintf (stderr, "azo_compiler_compile_expression: Unknown reference subtype %u\n", expr->term.subtype);
		return 0;
	}
	return 1;
}

/* Compile lvalue expression (i.e. reference) */

static unsigned int
compile_expression_lvalue (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (expr->term.type == EXPRESSION_REFERENCE) {
		/* LValue types */
		if (!compile_variable_reference (comp, expr, expr->term.subtype, src)) return 0;
	} else if (expr->term.type == EXPRESSION_ARRAY_ELEMENT) {
		if (!compile_array_reference (comp, expr->children, expr->children->next, src)) return 0;
	} else {
		fprintf (stderr, "compile_expression_lvalue: Invalid expression type %u\n", expr->term.type);
		return 0;
	}
	return 1;
}

unsigned int
azo_compiler_compile_expression (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (expr->term.type == EXPRESSION_REFERENCE) {
		/* LValue types */
		if (!compile_expression_lvalue (comp, expr, src)) return 0;
	} else if (expr->term.type == EXPRESSION_ARRAY_ELEMENT) {
		if (!compile_expression_lvalue (comp, expr, src)) return 0;
	} else {
		/* RValue types */
		if (!compile_expression_rvalue (comp, expr, src)) return 0;
	}
	return 1;
}

static unsigned int
compile_assign (AZOCompiler *comp, const AZOExpression *left, const AZOExpression *right, const AZOSource *src)
{
	LValue lval;
	/* Push necessary components (instance+key or array+index) */
	if (!compile_lvalue (comp, left, src, &lval, 0)) return 0;
	/* Push value */
	if (!azo_compiler_compile_expression (comp, right, src)) return 0;
	/* Actual assignment */
	if (!compile_assign_to_lvalue (comp, &lval, left)) return 0;
	return 1;
}

static unsigned int
compile_silent_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	switch (expr->term.type) {
	case AZO_TERM_EMPTY:
		break;
	case EXPRESSION_ASSIGN:
		if (!compile_assign (comp, expr->children, expr->children->next, src)) return 0;
		break;
	case EXPRESSION_FUNCTION_CALL:
		if (!compile_function_call (comp, expr->children, expr->children->next, src, 1)) return 0;
		break;
	case EXPRESSION_SUFFIX:
		if (!compile_suffix (comp, expr, expr->children, src, 1)) return 0;
		break;
	case EXPRESSION_PREFIX:
		if (!compile_prefix_arithmetic (comp, expr, expr->children, src, 1)) return 0;
		break;
	default:
		fprintf (stderr, "compile_silent_statement: invalid expression type %u\n", expr->term.type);
		return 0;
	}
	return 1;
}

static unsigned int
compile_single_declaration (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src, unsigned int type)
{
	AZOExpression *name, *value;
	name = expr->children;
	value = name->next;
	if (value) {
		azo_compiler_compile_expression (comp, value, src);
	} else {
		/* fixme: Implement runtime (or at least compile-time) type */
		azo_compiler_write_PUSH_EMPTY (comp, AZ_TYPE_NONE, expr);
		//azo_compiler_write_PUSH_EMPTY (comp, type);
	}
	return 1;
}

static unsigned int
compile_declaration (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *type, *child;
	type = expr->children;

	if (type->term.type != EXPRESSION_TYPE) {
		fprintf (stderr, "compile_declaration: Type is not resolved (%u/%u)\n", type->term.type, type->term.subtype);
		return 0;
	}

	for (child = type->next; child; child = child->next) {
		if (!compile_single_declaration (comp, child, src, type->term.subtype)) return 0;
	}
	return 1;
}

static unsigned int
compile_step_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (expr->term.type == EXPRESSION_DECLARATION_LIST) {
		if (!compile_declaration (comp, expr, src)) return 0;
	} else {
		if (!compile_silent_statement (comp, expr, src)) return 0;
	}
	return 1;
}

#define noDEBUG_STATEMENT

static unsigned int
compile_statement (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (AZO_EXPRESSION_IS(expr, EXPRESSION_KEYWORD, AZO_KEYWORD_RETURN)) {
		if (expr->children) {
			if (!compile_expression_rvalue (comp, expr->children, src)) return 0;
		}
		azo_compiler_write_ic (comp, AZO_TC_RETURN, expr);
		return 1;
	} else if (AZO_EXPRESSION_IS(expr, EXPRESSION_KEYWORD, AZO_KEYWORD_DEBUG)) {
		comp->debug = 1;
		return 1;
	} else {
		if (!compile_step_statement (comp, expr, src)) return 0;
	}
	return 1;
}

static unsigned int
compile_block (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	unsigned int result;
	result = compile_sentences (comp, expr->children, src);
	azo_compiler_write_POP (comp, expr->scope_size, NULL);
	return result;
}

#define noDEBUG_FOR

static unsigned int
compile_cycle (AZOCompiler *comp, const AZOExpression *expr,
	const AZOExpression *init, const AZOExpression *test, const AZOExpression *step, const AZOExpression *content,
	const AZOSource *src)
{
	unsigned int test_condition, end_cycle;

	/* Initialization */
	if (init) compile_step_statement (comp, init, src);
	/* Test condition */
	test_condition = azo_frame_get_current_ip (comp->current);
	if (test) {
		compile_expression_boolean (comp, test, src);
		/* Jump out of cycle if condition was FALSE */
		end_cycle = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, NULL);
	}
	/* Cycle content */
	compile_sentence (comp, content, src);
	/* Step */
	if (step) compile_silent_statement (comp, step, src);
	/* Go back to condition testing */
	azo_compiler_write_JMP_32 (comp, JMP_32, test_condition, NULL);
	azo_compiler_update_JMP_32 (comp, end_cycle);

	azo_compiler_write_POP (comp, expr->scope_size, NULL);
	return 1;
}

/*
 * FOR
 *   + init
 *   + condition
 *   + step
 *   + content
 */

static unsigned int
compile_for (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *init, *test, *step, *content;
	init = expr->children;
	test = init->next;
	step = test->next;
	content = step->next;
	return compile_cycle(comp, expr, init, test, step, content, src);
}

/*
 * WHILE
 *   + condition
 *   + content
 */

static unsigned int
compile_while (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *test, *content;
	test = expr->children;
	content = test->next;
	return compile_cycle(comp, expr, NULL, test, NULL, content, src);
}

/*
 * IF
 *   + condition
 *   + iftrue
 *   + iffalse
 */

 static unsigned int
compile_if (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	AZOExpression *cond = expr->children;
	AZOExpression *iftrue = cond->next;
	AZOExpression *iffalse = iftrue->next;

	compile_expression_boolean (comp, cond, src);
	/* Jump conditionally to NOT TRUE statement */
	unsigned int not_true = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0, cond);
	/* TRUE sentence */
	compile_sentence (comp, iftrue, src);
	if (iffalse) {
		/* Jump to end if TRUE */
		unsigned int else_loc = azo_compiler_write_JMP_32 (comp, JMP_32, 0, iftrue);
		/* Land here if FALSE */
		azo_compiler_update_JMP_32 (comp, not_true);
		compile_sentence (comp, iffalse, src);
		azo_compiler_update_JMP_32 (comp, else_loc);
	} else {
		/* Simply land here if FALSE */
		azo_compiler_update_JMP_32 (comp, not_true);
	}
	return 1;
}

/*
 * Sentence:
 *   Block
 *   Line
 *   for
 *   while
 *   if
 */

static unsigned int
compile_sentence (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	if (AZO_EXPRESSION_IS(expr, AZO_EXPRESSION_BLOCK, 0)) {
		if (!compile_block (comp, expr, src)) return 0;
	} else if (AZO_EXPRESSION_IS(expr, EXPRESSION_KEYWORD, AZO_KEYWORD_FOR)) {
		if (!compile_for (comp, expr, src)) return 0;
	} else if (AZO_EXPRESSION_IS(expr, EXPRESSION_KEYWORD, AZO_KEYWORD_WHILE)) {
		if (!compile_while (comp, expr, src)) return 0;
	} else if (AZO_EXPRESSION_IS(expr, EXPRESSION_KEYWORD, AZO_KEYWORD_IF)) {
		if (!compile_if (comp, expr, src)) return 0;
	} else {
		/* Line is the same as statement because semicolon is processed by parser */
		if (!compile_statement (comp, expr, src)) return 0;
	}
	return 1;
}

/* 
 * Sentences:
 *   [Sentence...]
 */

  static unsigned int
compile_sentences (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	while (expr) {
		if (!compile_sentence (comp, expr, src)) return 0;
		expr = expr->next;
	}
	return 1;
}

/*
 * Program:
 *   Sentences
 */

static unsigned int
compile_program (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	compile_sentences (comp, expr->children, src);
	return 1;
}

AZOProgram *
azo_compiler_compile (AZOCompiler *comp, AZOExpression *root, unsigned int need_resolve, const AZOSource *src)
{
	AZOProgram *prog;

	if (need_resolve) {
		root = azo_compiler_resolve_frame (comp, root);
	}

	/* Have to reserve closure before compilation */
	/* fixme: Here we probably do not have parent vars */
	azo_frame_reserve_data (comp->current, comp->current->n_parent_vars);

	if (root->term.type == AZO_EXPRESSION_PROGRAM) {
		/* Programs are lists of sentences */
		if (!compile_program (comp, root, src)) return NULL;
	} else if (root->term.type == AZO_EXPRESSION_BLOCK) {
		/* Function bodies are blocks */
		if (!compile_sentence (comp, root, src)) return NULL;
	} else {
		fprintf (stderr, "azo_compiler_compile: Invalid expression type %u\n", root->term.type);
		return NULL;
	}
	prog = (AZOProgram *) malloc (sizeof (AZOProgram));
	memset (prog, 0, sizeof (AZOProgram));
	prog->ctx = comp->ctx;
	prog->tcode = comp->current->code.bc;
	prog->tcode_length = comp->current->code.bc_len;
	comp->current->code.bc = NULL;
	comp->current->code.bc_len = 0;
	comp->current->code.bc_size = 0;
	prog->values = comp->current->code.data;
	prog->nvalues = comp->current->code.data_len;
	comp->current->code.data = NULL;
	comp->current->code.data_size = 0;
	comp->current->code.data_len = 0;
	return prog;
}
