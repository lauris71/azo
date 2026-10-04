#define __AZO_ARITHMETIC_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <stdint.h>

#include <azo/bytecode.h>

#include <azo/compiler/arithmetic.h>
#include <azo/compiler/helpers.h>

static uint8_t uint8_one = 1;
static uint32_t uint16_type = AZ_TYPE_UINT16;
static uint32_t int32_type = AZ_TYPE_INT32;

static unsigned int
compile_arithmetic_any_any (AZOCode *code, unsigned int operation, const AZONode *node)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max, rhs_type_lt_min, rhs_type_gt_max;
	unsigned int max_ge_i32, types_equal_1, types_equal_2, lhs_type_gt_rhs_type, types_equal_3;
	unsigned int finished;

	/* Test LHS and RHS is in range */
	if (operation == AZO_TERM_ARITHMETIC_PERCENT) {
		azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, node, &lhs_type_lt_min, &lhs_type_gt_max);
		azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, node, &rhs_type_lt_min, &rhs_type_gt_max);
	} else {
		azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, node, &lhs_type_lt_min, &lhs_type_gt_max);
		azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, node, &rhs_type_lt_min, &rhs_type_gt_max);
	}

	/* Determine max type */
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_MINMAX_TYPED (code, AZO_TC_MAX_TYPED, AZ_TYPE_UINT32, node);
	/* If max < Int32 promote both */
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_UINT32, (const AZValue *) &uint16_type, node);
	azo_code_write_COMPARE_TYPED (code, AZ_TYPE_UINT32, node);
	max_ge_i32 = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, node);
	/* Promote both to int32 */
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_UINT32, (const AZValue *) &int32_type, node);
	azo_code_write_PROMOTE (code, 2, node);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_UINT32, (const AZValue *) &int32_type, node);
	azo_code_write_PROMOTE (code, 1, node);
	types_equal_1 = azo_code_write_JMP32 (code, JMP_32, 0, node);
	/* if LHS.type == RHS.type goto types_equal */
	azo_code_update_JMP32 (code, max_ge_i32);
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, node);
	types_equal_2 = azo_code_write_JMP32 (code, JMP_32_IF, 0, node);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_COMPARE_TYPED (code, AZ_TYPE_UINT32, node);
	lhs_type_gt_rhs_type = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, node);
	/* Promote LHS and goto types_equal */
	azo_code_write_TYPE_OF (code, 0, node);
	azo_code_write_PROMOTE (code, 2, node);
	types_equal_3 = azo_code_write_JMP32 (code, JMP_32, 0, node);
	/* Promote RHS */
	azo_code_update_JMP32 (code, lhs_type_gt_rhs_type);
	azo_code_write_TYPE_OF (code, 1, node);
	azo_code_write_PROMOTE (code, 1, node);
	/* Types_equal */
	azo_code_update_JMP32 (code, types_equal_1);
	azo_code_update_JMP32 (code, types_equal_2);
	azo_code_update_JMP32 (code, types_equal_3);
	if (operation == AZO_TERM_ARITHMETIC_PLUS) {
		azo_code_write_ic (code, AZO_TC_ADD, node);
	} else if (operation == AZO_TERM_ARITHMETIC_MINUS) {
		azo_code_write_ic (code, AZO_TC_SUBTRACT, node);
	} else if (operation == AZO_TERM_ARITHMETIC_STAR) {
		azo_code_write_ic (code, AZO_TC_MULTIPLY, node);
	} else if (operation == AZO_TERM_ARITHMETIC_SLASH) {
		azo_code_write_ic (code, AZO_TC_DIVIDE, node);
	} else if (operation == AZO_TERM_ARITHMETIC_PERCENT) {
		azo_code_write_ic (code, AZO_TC_MODULO, node);
	}
	finished = azo_code_write_JMP32 (code, JMP_32, 0, node);

	/* invalid_type */
	azo_code_update_JMP32 (code, lhs_type_lt_min);
	azo_code_update_JMP32 (code, lhs_type_gt_max);
	azo_code_update_JMP32 (code, rhs_type_lt_min);
	azo_code_update_JMP32 (code, rhs_type_gt_max);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, node);

	/* finished */
	azo_code_update_JMP32 (code, finished);

	return 1;
}

static unsigned int
compile_arithmetic_boolean (AZOCode *code, unsigned int operation, const AZONode *expr)
{
	unsigned int not_boolean_1, not_boolean_2, finished;

	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_BOOLEAN, expr);
	not_boolean_1 = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_BOOLEAN, expr);
	not_boolean_2 = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	if (operation == AZO_TERM_ARITHMETIC_ANDAND) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_AND, expr);
	} else if (operation == AZO_TERM_ARITHMETIC_OROR) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_OR, expr);
	}
	finished = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* invalid_type */
	azo_code_update_JMP32 (code, not_boolean_1);
	azo_code_update_JMP32 (code, not_boolean_2);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);
	/* finished */
	azo_code_update_JMP32 (code, finished);
	return 1;
}

unsigned int
azo_compiler_compile_arithmetic (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src)
{
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
	/* LHS RHS */
	switch (expr->term.subtype) {
	case AZO_TERM_ARITHMETIC_PLUS:
	case AZO_TERM_ARITHMETIC_MINUS:
	case AZO_TERM_ARITHMETIC_SLASH:
	case AZO_TERM_ARITHMETIC_STAR:
	case AZO_TERM_ARITHMETIC_PERCENT:
	case AZO_TERM_ARITHMETIC_SHIFT_LEFT:
	case AZO_TERM_ARITHMETIC_SHIFT_RIGHT:
	case AZO_TERM_ARITHMETIC_AND:
	case AZO_TERM_ARITHMETIC_OR:
	case AZO_TERM_ARITHMETIC_CARET:
		return compile_arithmetic_any_any (&ctx->frame->code, expr->term.subtype, expr);
	case AZO_TERM_ARITHMETIC_ANDAND:
	case AZO_TERM_ARITHMETIC_OROR:
		return compile_arithmetic_boolean (&ctx->frame->code, expr->term.subtype, expr);
	default:
		fprintf (stderr, "azo_compiler_compile_arithmetic: Unknown subtype %u\n", expr->term.subtype);
		break;
	}
	return 0;
}

unsigned int
azo_compiler_compile_tilde (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	unsigned int lt_i8, gt_i64, gt_cd, finished_1, finished_2;
	AZOCode *code = &ctx->frame->code;
	/* Stack: RHS */
	if (!azo_compiler_compile_expression (comp, ctx, expr, src)) return 0;
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_INT64, expr, &lt_i8, &gt_i64);
	azo_code_write_ic (code, AZO_TC_BITWISE_NOT, expr);
	finished_1 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_update_JMP32 (code, gt_i64);
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_COMPLEX_FLOAT, AZ_TYPE_COMPLEX_DOUBLE, expr, NULL, &gt_cd);
	azo_code_write_ic (code, AZO_TC_CONJUGATE, expr);
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_update_JMP32 (code, lt_i8);
	azo_code_update_JMP32 (code, gt_cd);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);
	azo_code_update_JMP32 (code, finished_1);
	azo_code_update_JMP32 (code, finished_2);
	return 1;
}

unsigned int
azo_compiler_compile_increment (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *expr, AZOSource *src)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max;
	unsigned int types_equal;
	unsigned int finished;
	AZOCode *code = &ctx->frame->code;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_min, &lhs_type_gt_max);

	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_INT8, (const AZValue *) &uint8_one, expr);

	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, expr);
	types_equal = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* Promote RHS */
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_PROMOTE (code, 1, expr);
	/* Types_equal */
	azo_code_update_JMP32 (code, types_equal);

	azo_code_write_ic (code, AZO_TC_ADD, expr);
	finished = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* Invalid_type */
	azo_code_update_JMP32 (code, lhs_type_lt_min);
	azo_code_update_JMP32 (code, lhs_type_gt_max);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);

	/* finished */
	azo_code_update_JMP32 (code, finished);

	return 1;
}

unsigned int
azo_compiler_compile_decrement (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *expr, AZOSource *src)
{
	unsigned int lhs_type_lt_min, lhs_type_gt_max;
	unsigned int types_equal;
	unsigned int finished;
	AZOCode *code = &ctx->frame->code;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_min, &lhs_type_gt_max);

	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_INT8, (const AZValue *) &uint8_one, expr);

	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, expr);
	types_equal = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* Promote RHS */
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_PROMOTE (code, 1, expr);
	/* Types_equal */
	azo_code_update_JMP32 (code, types_equal);

	azo_code_write_ic (code, AZO_TC_SUBTRACT, expr);
	finished = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* Invalid_type */
	azo_code_update_JMP32 (code, lhs_type_lt_min);
	azo_code_update_JMP32 (code, lhs_type_gt_max);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);

	/* finished */
	azo_code_update_JMP32 (code, finished);

	return 1;
}
