#define __AZO_COMPARE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/bytecode.h>

#include <azo/compare.h>

static const uint32_t false_value = 0;
static const uint32_t true_value = 1;
static const void *null_ptr = NULL;

static void
compile_type_is_in_range (AZOCompiler *comp, unsigned int pos, uint32_t min_type, uint32_t max_type, unsigned int *jmp_lt, unsigned int *jmp_gt)
{
	azo_compiler_write_TYPE_OF (comp, pos);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &min_type);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	*jmp_lt = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
	azo_compiler_write_TYPE_OF (comp, pos);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_UINT32, (const AZValue *) &max_type);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	*jmp_gt = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
}

/* Compare whether stack(0) is equal to None */
/* On exception the tested element is left in stack */

static void
compile_compare_eq_any_none (AZOCompiler *comp, unsigned int comp_type, unsigned int *invalid_type)
{
	unsigned int none_cmp_none, block_cmp_none, finished_1, finished_2;
	/* null - null */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE);
	none_cmp_none = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* block - null */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IS_IMMEDIATE, 0, AZ_TYPE_BLOCK);
	block_cmp_none = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* pointer - null */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_POINTER, (const AZValue *) &null_ptr);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_POINTER);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
	finished_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* Equal */
	azo_compiler_update_JMP_32 (comp, none_cmp_none);
	azo_compiler_write_POP (comp, 1);
	if (comp_type == COMPARISON_E) {
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value);
	} else {
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value);
	}
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* Not equal */
	azo_compiler_update_JMP_32 (comp, block_cmp_none);
	azo_compiler_write_POP (comp, 1);
	if (comp_type == COMPARISON_E) {
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value);
	} else {
		azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value);
	}
	azo_compiler_update_JMP_32 (comp, finished_1);
	azo_compiler_update_JMP_32 (comp, finished_2);
}

static void
compile_compare_eq_any_boolean (AZOCompiler *comp, unsigned int comp_type, unsigned int *invalid_type)
{
	unsigned int lhs_is_boolean;
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_BOOLEAN);
	lhs_is_boolean = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	azo_compiler_write_POP (comp, 2);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	azo_compiler_update_JMP_32 (comp, lhs_is_boolean);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_BOOLEAN);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

/* On exception the tested elements are left in stack */

static void
compile_compare_eq_any_pointer (AZOCompiler *comp, unsigned int comp_type, unsigned int *invalid_type)
{
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_POINTER);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_POINTER);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

/* On exception the tested element is left in stack */

static void
compile_comparison_eq_any_const_pointer (AZOCompiler *comp, const void *ptr, unsigned int comp_type, unsigned int *invalid_type)
{
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_POINTER, (const AZValue *) ptr);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_POINTER);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

/* On exception the tested elements are left in stack */

static void
compile_compare_eq_any_block (AZOCompiler *comp, unsigned int comp_type, unsigned int *invalid_type)
{
	unsigned int rhs_is_subtype;

	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_TEST_TYPE (comp, AZO_TC_TYPE_IS, 2);
	rhs_is_subtype = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_TEST_TYPE (comp, AZO_TC_TYPE_IS, 1);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_update_JMP_32 (comp, rhs_is_subtype);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_BLOCK);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

/* On exception the tested element is left in stack */

static void
compile_comparison_eq_any_const_block (AZOCompiler *comp, unsigned int type, const void *block, unsigned int comp_type, unsigned int *invalid_type)
{
	unsigned int is_subtype;
	/* RHS is const block */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IS_IMMEDIATE, 0, type);
	is_subtype = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IS_SUPER_IMMEDIATE, 0, type);
	*invalid_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NOT, 0);
	azo_compiler_update_JMP_32 (comp, is_subtype);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BLOCK, (const AZValue *) block);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_BLOCK);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

/* Expects argument types to be checked */

static void
compile_comparison_eq_arithmetic_arithmetic (AZOCompiler *comp, unsigned int comp_type)
{
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	/* if LHS.type == RHS.type goto types_equal */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal_1 = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	lhs_type_gt_rhs_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	/* Promote LHS and goto types_equal */
	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_PROMOTE (comp, 2);
	types_equal_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* Promote RHS */
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_rhs_type);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);
	/* types_equal */
	azo_compiler_update_JMP_32 (comp, types_equal_1);
	azo_compiler_update_JMP_32 (comp, types_equal_2);
	azo_compiler_write_instruction_1 (comp, EQUAL);
	if (comp_type == COMPARISON_NE) {
		azo_compiler_write_instruction_1 (comp, AZO_TC_LOGICAL_NOT);
	}
}

static unsigned int
azo_compiler_compile_comparison_eq_any_any (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, unsigned int comp_type, const AZOSource *src, unsigned int reg)
{
	unsigned int rhs_is_none, lhs_is_none, invalid_type_cmp_none;
	unsigned int rhs_is_boolean, invalid_type_cmp_boolean;
	unsigned int rhs_is_pointer, invalid_type_cmp_pointer;
	unsigned int rhs_is_block, invalid_type_cmp_block;
	unsigned int lhs_type_lt_i8, lhs_type_gt_cdouble, rhs_type_lt_i8, rhs_type_gt_cdouble;
	unsigned int finished_1, finished_2, finished_3, finished_4, finished_5;

	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, rhs, src)) return 0;

	/* if (RHS.type == None) goto rhs_is_none */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE);
	rhs_is_none = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if (LHS.type == None) goto lhs_is_none */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 1, AZ_TYPE_NONE);
	lhs_is_none = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if (RHS.type == Boolean) goto rhs_is_boolean */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_BOOLEAN);
	rhs_is_boolean = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if (RHS.type == Pointer) goto rhs_is_pointer */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_POINTER);
	rhs_is_pointer = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if (RHS.type == Block) goto rhs_is_block */
	azo_compiler_write_TEST_TYPE_IMMEDIATE (comp, AZO_TC_TYPE_IS_IMMEDIATE, 0, AZ_TYPE_BLOCK);
	rhs_is_block = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);

	/* Test LHS is in range */
	compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &lhs_type_lt_i8, &lhs_type_gt_cdouble);
	/* Test RHS is in range */
	compile_type_is_in_range (comp, 0, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &rhs_type_lt_i8, &rhs_type_gt_cdouble);
	/* Compare */
	compile_comparison_eq_arithmetic_arithmetic (comp, comp_type);
	finished_5 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* lhs_is_none */
	azo_compiler_update_JMP_32 (comp, lhs_is_none);
	azo_compiler_write_EXCHANGE (comp, 1);
	/* rhs_is_none */
	azo_compiler_update_JMP_32 (comp, rhs_is_none);
	azo_compiler_write_POP (comp, 1);
	compile_compare_eq_any_none (comp, comp_type, &invalid_type_cmp_none);
	finished_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* rhs_is_boolean */
	azo_compiler_update_JMP_32 (comp, rhs_is_boolean);
	compile_compare_eq_any_boolean (comp, comp_type, &invalid_type_cmp_boolean);
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* rhs_is_pointer */
	azo_compiler_update_JMP_32 (comp, rhs_is_pointer);
	compile_compare_eq_any_boolean (comp, comp_type, &invalid_type_cmp_pointer);
	finished_3 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* rhs_is_block */
	azo_compiler_update_JMP_32 (comp, rhs_is_block);
	compile_compare_eq_any_block (comp, comp_type, &invalid_type_cmp_block);
	finished_4 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* invalid_type */
	azo_compiler_update_JMP_32 (comp, invalid_type_cmp_none);
	azo_compiler_update_JMP_32 (comp, invalid_type_cmp_boolean);
	azo_compiler_update_JMP_32 (comp, invalid_type_cmp_pointer);
	azo_compiler_update_JMP_32 (comp, invalid_type_cmp_block);
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_i8);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_cdouble);
	azo_compiler_update_JMP_32 (comp, rhs_type_lt_i8);
	azo_compiler_update_JMP_32 (comp, rhs_type_gt_cdouble);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);

	/* finished */
	azo_compiler_update_JMP_32 (comp, finished_1);
	azo_compiler_update_JMP_32 (comp, finished_2);
	azo_compiler_update_JMP_32 (comp, finished_3);
	azo_compiler_update_JMP_32 (comp, finished_4);
	azo_compiler_update_JMP_32 (comp, finished_5);

	return 1;
}


static unsigned int
compile_comparison_any_const_eq (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, unsigned int comp_type, const AZOSource *src, unsigned int reg)
{
	unsigned int invalid_type, finished;
	if (!rhs->value.impl) {
		if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
		compile_compare_eq_any_none (comp, comp_type, &invalid_type);
		return 1;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_POINTER) {
		if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
		compile_comparison_eq_any_const_pointer (comp, rhs->value.v.pointer_v, comp_type, &invalid_type);
	} else if (AZ_IMPL_TYPE(rhs->value.impl) == AZ_TYPE_BLOCK) {
		if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
		compile_comparison_eq_any_const_block (comp, AZ_IMPL_TYPE(rhs->value.impl), rhs->value.v.block, comp_type, &invalid_type);
	} else if (AZ_TYPE_IS_ARITHMETIC(AZ_PACKED_VALUE_TYPE(&rhs->value))) {
		unsigned int lhs_type_lt_i8, lhs_type_gt_cdouble;
		if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
		if (!azo_compiler_compile_expression (comp, rhs, src)) return 0;
		compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_COMPLEX_DOUBLE, &lhs_type_lt_i8, &lhs_type_gt_cdouble);
		compile_comparison_eq_arithmetic_arithmetic (comp, comp_type);
		finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		azo_compiler_update_JMP_32 (comp, lhs_type_lt_i8);
		azo_compiler_update_JMP_32 (comp, lhs_type_gt_cdouble);
		azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);
		azo_compiler_update_JMP_32 (comp, finished);
		return 1;
	} else {
		fprintf (stderr, "compile_comparison_any_const_eq: invalid type %u\n", AZ_IMPL_TYPE(rhs->value.impl));
		return 0;
	}
	finished = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	azo_compiler_update_JMP_32 (comp, invalid_type);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);
	azo_compiler_update_JMP_32 (comp, finished);
	return 1;
}

static unsigned int
azo_compiler_compile_comparison_eq (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, unsigned int comp_type, const AZOSource *src, unsigned int reg)
{
	if (rhs->term.type == EXPRESSION_CONSTANT) {
		return compile_comparison_any_const_eq (comp, lhs, rhs, comp_type, src, reg);
	} else if (lhs->term.type == EXPRESSION_CONSTANT) {
		return compile_comparison_any_const_eq (comp, rhs, lhs, comp_type, src, reg);
	} else {
		return azo_compiler_compile_comparison_eq_any_any (comp, lhs, rhs, comp_type, src, reg);
	}
}

static unsigned int
azo_compiler_compile_comparison_lg_any_any (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src, unsigned int reg)
{
	unsigned int lhs_type_lt_i8, lhs_type_gt_double, rhs_type_lt_i8, rhs_type_gt_double;
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	unsigned int is_true, is_false;
	unsigned int finished_1, finished_2;
	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, rhs, src)) return 0;

	//if (comp->debug) {
	//	azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any start");
	//	azo_compiler_write_DEBUG_STACK (comp);
	//}

	/* Test LHS is in range */
	compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, &lhs_type_lt_i8, &lhs_type_gt_double);
	/* Test RHS is in range */
	compile_type_is_in_range (comp, 0, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, &rhs_type_lt_i8, &rhs_type_gt_double);
	/*
	if LHS.type == RHS.type goto types_equal
	*/
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal_1 = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/*
	if LHS.type > RHS.type goto lhs_gt_rhs
	// LHS < RHS
	// Promote LHS->RHS
	PROMOTE LHS RHS.type
	goto types_equal
	*/
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	lhs_type_gt_rhs_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_PROMOTE (comp, 2);
	types_equal_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/*
	lhs_gt_rhs:
	// LHS > RHS
	PROMOTE RHS LHS
	*/

	azo_compiler_update_JMP_32 (comp, lhs_type_gt_rhs_type);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);

	/*
	types_equal:
	COMPARE RHS.type LHS RHS
	goto finished
	*/
	azo_compiler_update_JMP_32 (comp, types_equal_1);
	azo_compiler_update_JMP_32 (comp, types_equal_2);

	if (comp->debug) {
		azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any pre-compare");
		azo_compiler_write_DEBUG_STACK (comp);
	}

	azo_compiler_write_instruction_1 (comp, COMPARE);

	if (comp->debug) {
		azo_compiler_write_DEBUG_STRING (comp, "azo_compiler_compile_comparison_lg_any_any post-compare");
		azo_compiler_write_DEBUG_STACK (comp);
	}

	switch (expr->term.subtype) {
	case COMPARISON_LT:
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_LE:
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_GE:
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_GT:
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	default:
		fprintf (stderr, "azo_compiler_compile_comparison_lg_any_any: Invalid subtype %u\n", expr->term.subtype);
		return 0;
	}

	/* True */
	azo_compiler_update_JMP_32 (comp, is_true);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value);
	finished_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* False */
	azo_compiler_update_JMP_32 (comp, is_false);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value);
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* Invalid_type */
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_i8);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_double);
	azo_compiler_update_JMP_32 (comp, rhs_type_lt_i8);
	azo_compiler_update_JMP_32 (comp, rhs_type_gt_double);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);


	/* finished */
	azo_compiler_update_JMP_32 (comp, finished_1);
	azo_compiler_update_JMP_32 (comp, finished_2);

	return 1;
}

static unsigned int
compile_comparison_any_const_lg (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src, unsigned int reg)
{
	unsigned int lhs_type_lt_i8, lhs_type_gt_double;
	unsigned int types_equal_1, types_equal_2, lhs_type_gt_rhs_type;
	unsigned int is_true, is_false;
	unsigned int finished_1, finished_2;
	/* Stack: LHS RHS */
	if (!azo_compiler_compile_expression (comp, lhs, src)) return 0;
	azo_compiler_write_PUSH_IMMEDIATE (comp, (uint8_t) AZ_PACKED_VALUE_TYPE(&rhs->value), &rhs->value.v);
	/* Test LHS is in range */
	compile_type_is_in_range (comp, 1, AZ_TYPE_INT8, AZ_TYPE_DOUBLE, &lhs_type_lt_i8, &lhs_type_gt_double);
	/* if LHS.type == RHS.type goto types_equal */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_EQUAL_TYPED (comp, AZ_TYPE_UINT32);
	types_equal_1 = azo_compiler_write_JMP_32 (comp, JMP_32_IF, 0);
	/* if LHS.type > RHS.type goto lhs_gt_rhs */
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_COMPARE_TYPED (comp, AZ_TYPE_UINT32);
	lhs_type_gt_rhs_type = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
	/* Promothe LHS */
	azo_compiler_write_TYPE_OF (comp, 0);
	azo_compiler_write_PROMOTE (comp, 2);
	types_equal_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* Promote RHS */
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_rhs_type);
	azo_compiler_write_TYPE_OF (comp, 1);
	azo_compiler_write_PROMOTE (comp, 1);
	/* Compare */
	azo_compiler_update_JMP_32 (comp, types_equal_1);
	azo_compiler_update_JMP_32 (comp, types_equal_2);
	azo_compiler_write_instruction_1 (comp, COMPARE);

	switch (expr->term.subtype) {
	case COMPARISON_LT:
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_LE:
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_GE:
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32_IF_NEGATIVE, 0);
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	case COMPARISON_GT:
		is_true = azo_compiler_write_JMP_32 (comp, JMP_32_IF_POSITIVE, 0);
		is_false = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
		break;
	default:
		fprintf (stderr, "compile_comparison_any_const_lg: Invalid subtype %u\n", expr->term.subtype);
		return 0;
		break;
	}

	/* True */
	azo_compiler_update_JMP_32 (comp, is_true);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &true_value);
	finished_1 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);
	/* False */
	azo_compiler_update_JMP_32 (comp, is_false);
	azo_compiler_write_PUSH_IMMEDIATE (comp, AZ_TYPE_BOOLEAN, (const AZValue *) &false_value);
	finished_2 = azo_compiler_write_JMP_32 (comp, JMP_32, 0);

	/* Invalid_type */
	azo_compiler_update_JMP_32 (comp, lhs_type_lt_i8);
	azo_compiler_update_JMP_32 (comp, lhs_type_gt_double);
	azo_compiler_write_EXCEPTION (comp, AZO_EXCEPTION_INVALID_TYPE);


	/* finished */
	azo_compiler_update_JMP_32 (comp, finished_1);
	azo_compiler_update_JMP_32 (comp, finished_2);

	return 1;
}

unsigned int
azo_compiler_compile_comparison (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src, unsigned int reg)
{
	if ((expr->term.subtype == COMPARISON_E) || (expr->term.subtype == COMPARISON_NE)) {
		return azo_compiler_compile_comparison_eq (comp, lhs, rhs, expr->term.subtype, src, reg);
	} else {
#if 0
		if ((lhs->term.type != EXPRESSION_CONSTANT) && (rhs->term.type == EXPRESSION_CONSTANT)) {
			if (!compile_comparison_any_const_lg (comp, lhs, rhs, expr, text, reg)) return 0;
			write_PUSH_FROM (comp, reg);
			return 1;
		}
#endif
		return azo_compiler_compile_comparison_lg_any_any (comp, lhs, rhs, expr, src, reg);
	}
}

