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

static void
compile_arithmetic_op (AZOCode *code, unsigned int use_typed, unsigned int typed_op, unsigned int generic_op, unsigned int type, const AZONode *expr)
{
	if (use_typed) {
		azo_code_write_ic_u8 (code, typed_op, AZ_TYPE_INDEX (type), expr);
	} else {
		azo_code_write_ic (code, generic_op, expr);
	}
}

static unsigned int
compile_arithmetic_boolean (AZOCode *code, unsigned int operation, const AZONode *expr, unsigned int lhs_is_boolean, unsigned int rhs_is_boolean)
{
	if (!lhs_is_boolean) {
		azo_code_write_ic_u32_u32 (code, AZO_TC_EXCEPTION_IF_TYPE_IS_NOT, 1, AZ_TYPE_BOOLEAN, expr);
	}
	if (!rhs_is_boolean) {
		azo_code_write_ic_u32_u32 (code, AZO_TC_EXCEPTION_IF_TYPE_IS_NOT, 0, AZ_TYPE_BOOLEAN, expr);
	}
	if (operation == AZO_TERM_ARITHMETIC_ANDAND) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_AND, expr);
	} else if (operation == AZO_TERM_ARITHMETIC_OROR) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_OR, expr);
	}
	return 1;
}

unsigned int
azo_compiler_compile_arithmetic (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src)
{
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
	/* LHS RHS */
	/* Use a promotion-free TYPED opcode when both operands are already the same
	 * promoted type (int32..complex double) */
	unsigned int use_typed = (lhs->type == rhs->type) &&
		(AZ_TYPE_INDEX (lhs->type) >= AZ_TYPE_IDX_INT32) &&
		(AZ_TYPE_INDEX (lhs->type) <= AZ_TYPE_IDX_COMPLEX_DOUBLE);
	switch (expr->term.subtype) {
	case AZO_TERM_ARITHMETIC_PLUS:
		compile_arithmetic_op (&ctx->frame->code, use_typed, AZO_TC_ADD_TYPED, AZO_TC_ADD, lhs->type, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_MINUS:
		compile_arithmetic_op (&ctx->frame->code, use_typed, AZO_TC_SUBTRACT_TYPED, AZO_TC_SUBTRACT, lhs->type, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_SLASH:
		compile_arithmetic_op (&ctx->frame->code, use_typed, AZO_TC_DIVIDE_TYPED, AZO_TC_DIVIDE, lhs->type, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_STAR:
		compile_arithmetic_op (&ctx->frame->code, use_typed, AZO_TC_MULTIPLY_TYPED, AZO_TC_MULTIPLY, lhs->type, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_PERCENT:
		compile_arithmetic_op (&ctx->frame->code, use_typed, AZO_TC_MODULO_TYPED, AZO_TC_MODULO, lhs->type, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_SHIFT_LEFT:
		azo_code_write_ic (&ctx->frame->code, AZO_TC_SHIFT_LEFT, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_SHIFT_RIGHT:
		azo_code_write_ic (&ctx->frame->code, AZO_TC_SHIFT_RIGHT, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_AND:
		azo_code_write_ic (&ctx->frame->code, AZO_TC_BITWISE_AND, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_OR:
		azo_code_write_ic (&ctx->frame->code, AZO_TC_BITWISE_OR, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_CARET:
		azo_code_write_ic (&ctx->frame->code, AZO_TC_BITWISE_XOR, expr);
		return 1;
	case AZO_TERM_ARITHMETIC_ANDAND:
	case AZO_TERM_ARITHMETIC_OROR:
		return compile_arithmetic_boolean (&ctx->frame->code, expr->term.subtype, expr, lhs->type == AZ_TYPE_BOOLEAN, rhs->type == AZ_TYPE_BOOLEAN);
	default:
		fprintf (stderr, "azo_compiler_compile_arithmetic: operation %u is not implemented in bytecode\n", expr->term.subtype);
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
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_min, &lhs_type_gt_max);

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
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_min, &lhs_type_gt_max);

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
