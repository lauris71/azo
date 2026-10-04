#define __AZO_COMPARE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/bytecode.h>

#include <azo/compare.h>
#include <azo/compiler/helpers.h>

static const uint32_t false_value = 0;
static const uint32_t true_value = 1;
static const void *null_ptr = NULL;

/* Compare whether stack(0) is equal to None */
/* On exception the tested element is left in stack */

static void
compile_compare_eq_any_none (AZOCode *code, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	unsigned int none_cmp_none, block_cmp_none, finished_1, finished_2;
	/* null - null */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, expr);
	none_cmp_none = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* block - null */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IS_IMMEDIATE, 0, AZ_TYPE_BLOCK, expr);
	block_cmp_none = azo_code_write_JMP32 (code, JMP_32_IF, 0, NULL);
	/* pointer - null */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_POINTER, (const AZValue *) &null_ptr, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_POINTER, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
	finished_1 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* Equal */
	azo_code_update_JMP32 (code, none_cmp_none);
	azo_code_write_POP (code, 1, expr);
	if (comp_type == AZO_TERM_COMPARISON_E) {
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value, expr);
	} else {
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value, expr);
	}
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* Not equal */
	azo_code_update_JMP32 (code, block_cmp_none);
	azo_code_write_POP (code, 1, expr);
	if (comp_type == AZO_TERM_COMPARISON_E) {
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value, expr);
	} else {
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value, expr);
	}
	azo_code_update_JMP32 (code, finished_1);
	azo_code_update_JMP32 (code, finished_2);
}

static void
compile_compare_eq_any_boolean (AZOCode *code, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	unsigned int lhs_is_boolean;
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_BOOLEAN, expr);
	lhs_is_boolean = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	azo_code_write_POP (code, 2, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_update_JMP32 (code, lhs_is_boolean);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_BOOLEAN, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

/* On exception the tested elements are left in stack */

static void
compile_compare_eq_any_pointer (AZOCode *code, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_POINTER, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_POINTER, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

/* On exception the tested element is left in stack */

static void
compile_comparison_eq_any_const_pointer (AZOCode *code, const void *ptr, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_POINTER, (const AZValue *) ptr, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_POINTER, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

/* On exception the tested elements are left in stack */

static void
compile_compare_eq_any_block (AZOCode *code, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	unsigned int rhs_is_subtype;

	azo_code_write_TYPE_OF (code, 0, expr);
	azo_code_write_TEST_TYPE (code, AZO_TC_TYPE_IS, 2, expr);
	rhs_is_subtype = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	azo_code_write_TYPE_OF (code, 0, expr);
	azo_code_write_TEST_TYPE (code, AZO_TC_TYPE_IS, 1, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_update_JMP32 (code, rhs_is_subtype);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_BLOCK, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

/* On exception the tested element is left in stack */

static void
compile_comparison_eq_any_const_block (AZOCode *code, unsigned int type, const void *block, unsigned int comp_type, unsigned int *invalid_type, const AZONode *expr)
{
	unsigned int is_subtype;
	/* RHS is const block */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IS_IMMEDIATE, 0, type, expr);
	is_subtype = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IS_SUPER_IMMEDIATE, 0, type, expr);
	*invalid_type = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	azo_code_update_JMP32 (code, is_subtype);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BLOCK, (const AZValue *) block, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_BLOCK, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

/* Expects argument types to be checked */

static void
compile_comparison_eq_arithmetic_arithmetic (AZOCode *code, unsigned int comp_type, const AZONode *expr)
{
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	/* if LHS.type == RHS.type goto types_equal */
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, expr);
	types_equal_1 = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_COMPARE_TYPED (code, AZ_TYPE_UINT32, expr);
	lhs_type_gt_rhs_type = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
	/* Promote LHS and goto types_equal */
	azo_code_write_TYPE_OF (code, 0, expr);
	azo_code_write_PROMOTE (code, 2, expr);
	types_equal_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* Promote RHS */
	azo_code_update_JMP32 (code, lhs_type_gt_rhs_type);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_PROMOTE (code, 1, expr);
	/* types_equal */
	azo_code_update_JMP32 (code, types_equal_1);
	azo_code_update_JMP32 (code, types_equal_2);
	azo_code_write_ic (code, EQUAL, expr);
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
}

static unsigned int
azo_compiler_compile_comparison_eq_any_any (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, unsigned int comp_type, AZOSource *src, unsigned int reg)
{
	unsigned int rhs_is_none, lhs_is_none, invalid_type_cmp_none;
	unsigned int rhs_is_boolean, invalid_type_cmp_boolean;
	unsigned int rhs_is_pointer, invalid_type_cmp_pointer;
	unsigned int rhs_is_block, invalid_type_cmp_block;
	unsigned int lhs_type_lt_i8, lhs_type_gt_cdouble, rhs_type_lt_i8, rhs_type_gt_cdouble;
	unsigned int finished_1, finished_2, finished_3, finished_4, finished_5;
	AZOCode *code = &ctx->frame->code;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;

	/* if (RHS.type == None) goto rhs_is_none */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, expr);
	rhs_is_none = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if (LHS.type == None) goto lhs_is_none */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_NONE, expr);
	lhs_is_none = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if (RHS.type == Boolean) goto rhs_is_boolean */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_BOOLEAN, expr);
	rhs_is_boolean = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if (RHS.type == Pointer) goto rhs_is_pointer */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER, expr);
	rhs_is_pointer = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if (RHS.type == Block) goto rhs_is_block */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IS_IMMEDIATE, 0, AZ_TYPE_BLOCK, expr);
	rhs_is_block = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);

	/* Test LHS is in range */
	azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_i8, &lhs_type_gt_cdouble);
	/* Test RHS is in range */
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &rhs_type_lt_i8, &rhs_type_gt_cdouble);
	/* Compare */
	compile_comparison_eq_arithmetic_arithmetic (code, comp_type, expr);
	finished_5 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* lhs_is_none */
	azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_write_EXCHANGE (code, 1, expr);
	/* rhs_is_none */
	azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_write_POP (code, 1, expr);
	compile_compare_eq_any_none (code, comp_type, &invalid_type_cmp_none, expr);
	finished_1 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* rhs_is_boolean */
	azo_code_write_JMP32 (code, JMP_32, 0, expr);
	compile_compare_eq_any_boolean (code, comp_type, &invalid_type_cmp_boolean, expr);
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* rhs_is_pointer */
	azo_code_write_JMP32 (code, JMP_32, 0, expr);
	compile_compare_eq_any_pointer (code, comp_type, &invalid_type_cmp_pointer, expr);
	finished_3 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* rhs_is_block */
	azo_code_write_JMP32 (code, JMP_32, 0, expr);
	compile_compare_eq_any_block (code, comp_type, &invalid_type_cmp_block, expr);
	finished_4 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* invalid_type */
	azo_code_update_JMP32 (code, invalid_type_cmp_none);
	azo_code_update_JMP32 (code, invalid_type_cmp_boolean);
	azo_code_update_JMP32 (code, invalid_type_cmp_pointer);
	azo_code_update_JMP32 (code, invalid_type_cmp_block);
	azo_code_update_JMP32 (code, lhs_type_lt_i8);
	azo_code_update_JMP32 (code, lhs_type_gt_cdouble);
	azo_code_update_JMP32 (code, rhs_type_lt_i8);
	azo_code_update_JMP32 (code, rhs_type_gt_cdouble);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);

	/* finished */
	azo_code_update_JMP32 (code, finished_1);
	azo_code_update_JMP32 (code, finished_2);
	azo_code_update_JMP32 (code, finished_3);
	azo_code_update_JMP32 (code, finished_4);
	azo_code_update_JMP32 (code, finished_5);

	return 1;
}


static unsigned int
compile_comparison_any_const_eq (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, unsigned int comp_type, AZOSource *src, unsigned int reg)
{
	unsigned int invalid_type, finished;
	AZOCode *code = &ctx->frame->code;
	if (!rhs->value.impl) {
		if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
		compile_compare_eq_any_none (code, comp_type, &invalid_type, expr);
		return 1;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_POINTER) {
		if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
		compile_comparison_eq_any_const_pointer (code, rhs->value.v.pointer_v, comp_type, &invalid_type, expr);
	} else if (AZ_IMPL_TYPE(rhs->value.impl) == AZ_TYPE_BLOCK) {
		if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
		compile_comparison_eq_any_const_block (code, AZ_IMPL_TYPE(rhs->value.impl), rhs->value.v.block, comp_type, &invalid_type, expr);
	} else if (AZ_TYPE_IS_ARITHMETIC(AZ_PACKED_VALUE_TYPE(&rhs->value))) {
		unsigned int lhs_type_lt_i8, lhs_type_gt_cdouble;
		if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
		if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
		azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, expr, &lhs_type_lt_i8, &lhs_type_gt_cdouble);
		compile_comparison_eq_arithmetic_arithmetic (code, comp_type, expr);
		finished = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		azo_code_update_JMP32 (code, lhs_type_lt_i8);
		azo_code_update_JMP32 (code, lhs_type_gt_cdouble);
		azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);
		azo_code_update_JMP32 (code, finished);
		return 1;
	} else {
		fprintf (stderr, "compile_comparison_any_const_eq: invalid type %u\n", AZ_IMPL_TYPE(rhs->value.impl));
		return 0;
	}
	finished = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	azo_code_update_JMP32 (code, invalid_type);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);
	azo_code_update_JMP32 (code, finished);
	return 1;
}

static unsigned int
azo_compiler_compile_comparison_eq (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, unsigned int comp_type, AZOSource *src, unsigned int reg)
{
	if (rhs->term.type == AZO_TERM_CONSTANT) {
		return compile_comparison_any_const_eq (comp, ctx, lhs, rhs, expr, comp_type, src, reg);
	} else if (lhs->term.type == AZO_TERM_CONSTANT) {
		return compile_comparison_any_const_eq (comp, ctx, rhs, lhs, expr, comp_type, src, reg);
	} else {
		return azo_compiler_compile_comparison_eq_any_any (comp, ctx, lhs, rhs, expr, comp_type, src, reg);
	}
}

static unsigned int
azo_compiler_compile_comparison_lg_any_any (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src, unsigned int reg)
{
	unsigned int lhs_type_lt_i8, lhs_type_gt_double, rhs_type_lt_i8, rhs_type_gt_double;
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	unsigned int is_true, is_false;
	unsigned int finished_1, finished_2;
	AZOCode *code = &ctx->frame->code;
	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;

	//if (comp->debug) {
	//	azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any start");
	//	azo_compiler_write_DEBUG_STACK (comp);
	//}

	/* Test LHS is in range */
	azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, expr, &lhs_type_lt_i8, &lhs_type_gt_double);
	/* Test RHS is in range */
	azo_code_compile_type_is_in_range (code, 0, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, expr, &rhs_type_lt_i8, &rhs_type_gt_double);
	/*
	if LHS.type == RHS.type goto types_equal
	*/
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, expr);
	types_equal_1 = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/*
	if LHS.type > RHS.type goto lhs_gt_rhs
	// LHS < RHS
	// Promote LHS->RHS
	PROMOTE LHS RHS.type
	goto types_equal
	*/
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_COMPARE_TYPED (code, AZ_TYPE_UINT32, expr);
	lhs_type_gt_rhs_type = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
	azo_code_write_TYPE_OF (code, 0, expr);
	azo_code_write_PROMOTE (code, 2, expr);
	types_equal_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/*
	lhs_gt_rhs:
	// LHS > RHS
	PROMOTE RHS LHS
	*/

	azo_code_update_JMP32 (code, lhs_type_gt_rhs_type);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_PROMOTE (code, 1, expr);

	/*
	types_equal:
	COMPARE RHS.type LHS RHS
	goto finished
	*/
	azo_code_update_JMP32 (code, types_equal_1);
	azo_code_update_JMP32 (code, types_equal_2);

#if 0
	if (comp->debug) {
		azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any pre-compare", NULL);
		azo_compiler_write_DEBUG_STACK (comp);
	}
#endif

	azo_code_write_ic (code, COMPARE, expr);

#if 0
	if (comp->debug) {
		azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any post-compare", NULL);
		azo_compiler_write_DEBUG_STACK (comp);
	}
#endif
	switch (expr->term.subtype) {
	case AZO_TERM_COMPARISON_LT:
		is_true = azo_code_write_JMP32 (code, JMP_32_IF_NEGATIVE, 0, expr);
		is_false = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_LE:
		is_false = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
		is_true = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_GE:
		is_false = azo_code_write_JMP32 (code, JMP_32_IF_NEGATIVE, 0, expr);
		is_true = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_GT:
		is_true = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
		is_false = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	default:
		fprintf (stderr, "azo_compiler_compile_comparison_lg_any_any: Invalid subtype %u\n", expr->term.subtype);
		return 0;
	}

	/* True */
	azo_code_update_JMP32 (code, is_true);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value, expr);
	finished_1 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* False */
	azo_code_update_JMP32 (code, is_false);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value, expr);
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* Invalid_type */
	azo_code_update_JMP32 (code, lhs_type_lt_i8);
	azo_code_update_JMP32 (code, lhs_type_gt_double);
	azo_code_update_JMP32 (code, rhs_type_lt_i8);
	azo_code_update_JMP32 (code, rhs_type_gt_double);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);


	/* finished */
	azo_code_update_JMP32 (code, finished_1);
	azo_code_update_JMP32 (code, finished_2);

	return 1;
}

static unsigned int
compile_comparison_any_const_lg (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src, unsigned int reg)
{
	unsigned int lhs_type_lt_i8, lhs_type_gt_double;
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	unsigned int is_true, is_false;
	unsigned int finished_1, finished_2;
	AZOCode *code = &ctx->frame->code;
	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	azo_code_write_PUSH_IMMEDIATE (code, AZ_PACKED_VALUE_TYPE(&rhs->value), &rhs->value.v, expr);
	/* Test LHS is in range */
	azo_code_compile_type_is_in_range (code, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, expr, &lhs_type_lt_i8, &lhs_type_gt_double);
	/* if LHS.type == RHS.type goto types_equal */
	azo_code_write_TYPE_OF (code, 1,expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_EQUAL_TYPED (code, AZ_TYPE_UINT32, expr);
	types_equal_1 = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_COMPARE_TYPED (code, AZ_TYPE_UINT32, expr);
	lhs_type_gt_rhs_type = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
	/* Promote LHS */
	azo_code_write_TYPE_OF (code, 0, expr);
	azo_code_write_PROMOTE (code, 2, expr);
	types_equal_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* Promote RHS */
	azo_code_update_JMP32 (code, lhs_type_gt_rhs_type);
	azo_code_write_TYPE_OF (code, 1, expr);
	azo_code_write_PROMOTE (code, 1, expr);
	/* Compare */
	azo_code_update_JMP32 (code, types_equal_1);
	azo_code_update_JMP32 (code, types_equal_2);
	azo_code_write_ic (code, COMPARE, expr);

	switch (expr->term.subtype) {
	case AZO_TERM_COMPARISON_LT:
		is_true = azo_code_write_JMP32 (code, JMP_32_IF_NEGATIVE, 0, expr);
		is_false = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_LE:
		is_false = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
		is_true = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_GE:
		is_false = azo_code_write_JMP32 (code, JMP_32_IF_NEGATIVE, 0, expr);
		is_true = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	case AZO_TERM_COMPARISON_GT:
		is_true = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, expr);
		is_false = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		break;
	default:
		fprintf (stderr, "compile_comparison_any_const_lg: Invalid subtype %u\n", expr->term.subtype);
		return 0;
		break;
	}

	/* True */
	azo_code_update_JMP32 (code, is_true);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value, expr);
	finished_1 = azo_code_write_JMP32 (code, JMP_32, 0, expr);
	/* False */
	azo_code_update_JMP32 (code, is_false);
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value, expr);
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, expr);

	/* Invalid_type */
	azo_code_update_JMP32 (code, lhs_type_lt_i8);
	azo_code_update_JMP32 (code, lhs_type_gt_double);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, expr);


	/* finished */
	azo_code_update_JMP32 (code, finished_1);
	azo_code_update_JMP32 (code, finished_2);

	return 1;
}

unsigned int
azo_compiler_compile_comparison (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src, unsigned int reg)
{
	if ((expr->term.subtype == AZO_TERM_COMPARISON_E) || (expr->term.subtype == AZO_TERM_COMPARISON_NE)) {
		return azo_compiler_compile_comparison_eq (comp, ctx, lhs, rhs, expr, expr->term.subtype, src, reg);
	} else if ((expr->term.subtype == AZO_TERM_COMPARISON_IDENTICAL) || (expr->term.subtype == AZO_TERM_COMPARISON_NOT_IDENTICAL)) {
		/* fixme: implement identity comparison (=== !==) */
		fprintf (stderr, "azo_compiler_compile_comparison: identity comparison is not implemented\n");
		return 0;
	} else {
#if 0
		if ((lhs->term.type != EXPRESSION_CONSTANT) && (rhs->term.type == EXPRESSION_CONSTANT)) {
			if (!compile_comparison_any_const_lg (comp, ctx, lhs, rhs, expr, src, reg)) return 0;
			write_PUSH_FROM (comp, reg);
			return 1;
		}
#endif
		return azo_compiler_compile_comparison_lg_any_any (comp, ctx, lhs, rhs, expr, src, reg);
	}
}

