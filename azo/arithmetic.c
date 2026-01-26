#define __AZO_ARITHMETIC_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <stdint.h>

#include <azo/bytecode.h>

#include <azo/arithmetic.h>

static uint8_t uint8_one = 1;
static uint32_t uint16_type = AZ_TYPE_UINT16;
static uint32_t int32_type = AZ_TYPE_INT32;

static void
compile_type_is_in_range (AZOCompiler *comp, unsigned int pos, uint32_t min_type, uint32_t max_type, unsigned int *jmp_lt, unsigned int *jmp_gt)
{
	if (jmp_lt) {
		azo_compiler_write_TYPE_OF (comp, pos);
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, ( const AZValue *) & min_type);
		azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
		*jmp_lt = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
	}
	if (jmp_gt) {
		azo_compiler_write_TYPE_OF (comp, pos);
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, ( const AZValue *) & max_type);
		azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
		*jmp_gt = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	}
}

static unsigned int
azo_compiler_compile_arithmetic_any_any (AZOCompiler *comp, unsigned int operation)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max, rhs_type_lt_min, rhs_type_gt_max;
	unsigned int max_ge_i32, types_equal_1, types_equal_2, lhs_type_gt_rhs_type, types_equal_3;
	unsigned int finished;

	/* Test LHS and RHS is in range */
	if (operation == ARITHMETIC_PERCENT) {
		compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, &lhs_type_lt_min, &lhs_type_gt_max);
		compile_type_is_in_range (comp, 0, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, &rhs_type_lt_min, &rhs_type_gt_max);
	} else {
		compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &lhs_type_lt_min, &lhs_type_gt_max);
		compile_type_is_in_range (comp, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &rhs_type_lt_min, &rhs_type_gt_max);
	}

	/* Determine max type */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_MINMAX_TYPED (comp, MAX_TYPED, AZ_TYPE_UINT32);
	/* If max < Int32 promote both */
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &uint16_type);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	max_ge_i32 = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	/* Promote both to int32 */
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &int32_type);
	azo_compiler_write_PROMOTE (comp, 2);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &int32_type);
	azo_compiler_write_PROMOTE (comp, 1);
	types_equal_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* if LHS.type == RHS.type goto types_equal */
	azo_compiler_update_JMP_32 (comp, max_ge_i32);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal_2 = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	lhs_type_gt_rhs_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	/* Promote LHS and goto types_equal */
	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_PROMOTE (comp, 2);
	types_equal_3 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* Promote RHS */
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_rhs_type);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);
	/* Types_equal */
	azo_compiler_update_JMP_32 (comp, types_equal_1);
	azo_compiler_update_JMP_32 (comp, types_equal_2);
	azo_compiler_update_JMP_32 (comp, types_equal_3);
#if 0
	azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_arithmetic_any_any");
	azo_compiler_write_DEBUG_STACK (comp);
#endif
	if (operation == ARITHMETIC_PLUS) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_ADD);
	} else if (operation == ARITHMETIC_MINUS) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_SUBTRACT);
	} else if (operation == ARITHMETIC_STAR) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_MULTIPLY);
	} else if (operation == ARITHMETIC_SLASH) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_DIVIDE);
	} else if (operation == ARITHMETIC_PERCENT) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_MODULO);
	}
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* invalid_type */
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_min);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_max);
	azo_compiler_update_JMP_32 (comp, rhs_type_lt_min);
	azo_compiler_update_JMP_32 (comp, rhs_type_gt_max);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);

	/* finished */
	azo_compiler_update_JMP_32 (comp, finished);

	return 1;
}

static unsigned int
azo_compiler_compile_arithmetic_boolean (AZOCompiler *comp, unsigned int operation)
{
	unsigned int not_boolean_1, not_boolean_2, finished;
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_BOOLEAN);
	not_boolean_1 = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_BOOLEAN);
	not_boolean_2 = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	if (operation == ARITHMETIC_ANDAND) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_AND);
	} else if (operation == ARITHMETIC_OROR) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_OR);
	}
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* invalid_type */
	azo_compiler_update_JMP_32 (comp, not_boolean_1);
	azo_compiler_update_JMP_32 (comp, not_boolean_2);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);
	/* finished */
	azo_compiler_update_JMP_32 (comp, finished);
	return 1;
}

unsigned int
azo_compiler_compile_arithmetic (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src)
{
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, rhs, src)) return 0;
	/* LHS RHS */
	switch (expr->term.subtype) {
	case ARITHMETIC_PLUS:
	case ARITHMETIC_MINUS:
	case ARITHMETIC_SLASH:
	case ARITHMETIC_STAR:
	case ARITHMETIC_PERCENT:
	case ARITHMETIC_SHIFT_LEFT:
	case ARITHMETIC_SHIFT_RIGHT:
	case ARITHMETIC_AND:
	case ARITHMETIC_OR:
	case ARITHMETIC_CARET:
		return azo_compiler_compile_arithmetic_any_any (comp, expr->term.subtype);
	case ARITHMETIC_ANDAND:
	case ARITHMETIC_OROR:
		return azo_compiler_compile_arithmetic_boolean (comp, expr->term.subtype);
	default:
		fprintf (stderr, "azo_compiler_compile_arithmetic: Unknown subtype %u\n", expr->term.subtype);
		break;
	}
	return 0;
}

unsigned int
azo_compiler_compile_tilde (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src)
{
	unsigned int lt_i8, gt_i64, gt_cd, finished_1, finished_2;
	if (!azo_compiler_compile_expression (comp, expr, src)) return 0;
	compile_type_is_in_range (comp, 0, AZ_TYPE_INT8, AZ_TYPE_INT64, &lt_i8, &gt_i64);
	azo_compiler_write_instruction_1 (comp, AZO_TC_BITWISE_NOT);
	finished_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	azo_compiler_update_JMP_32 (comp, gt_i64);
	compile_type_is_in_range (comp, 0, AZ_TYPE_COMPLEX_FLOAT, AZ_TYPE_COMPLEX_DOUBLE, NULL, &gt_cd);
	azo_compiler_write_instruction_1 (comp, AZO_TC_CONJUGATE);
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	azo_compiler_update_JMP_32 (comp, lt_i8);
	azo_compiler_update_JMP_32 (comp, gt_cd);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);
	azo_compiler_update_JMP_32 (comp, finished_1);
	azo_compiler_update_JMP_32 (comp, finished_2);
	return 1;
}

unsigned int
azo_compiler_compile_increment (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *expr, const AZOSource *src)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max;
	unsigned int types_equal;
	unsigned int finished;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &lhs_type_lt_min, &lhs_type_gt_max);

	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_INT8, (const AZValue *) &uint8_one);

	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* Promote RHS */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);
	/* Types_equal */
	azo_compiler_update_JMP_32 (comp, types_equal);

	azo_compiler_write_instruction_1 (comp, AZO_TC_ADD);
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* Invalid_type */
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_min);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_max);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);

	/* finished */
	azo_compiler_update_JMP_32 (comp, finished);

	return 1;
}

unsigned int
azo_compiler_compile_decrement (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *expr, const AZOSource *src)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max;
	unsigned int types_equal;
	unsigned int finished;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &lhs_type_lt_min, &lhs_type_gt_max);

	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_INT8, (const AZValue *) &uint8_one);

	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* Promote RHS */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);
	/* Types_equal */
	azo_compiler_update_JMP_32 (comp, types_equal);

	azo_compiler_write_instruction_1 (comp, AZO_TC_SUBTRACT);
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* Invalid_type */
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_min);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_max);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);

	/* finished */
	azo_compiler_update_JMP_32 (comp, finished);

	return 1;
}
