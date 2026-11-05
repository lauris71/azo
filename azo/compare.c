#define __AZO_COMPARE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <math.h>
#include <string.h>

#include <az/az.h>
#include <az/boxed-value.h>
#include <az/class.h>
#include <az/value.h>

#include <azo/bytecode.h>

#include <azo/compare.h>
#include <azo/compiler/helpers.h>

static const uint32_t false_value = 0;
static const uint32_t true_value = 1;

/* Value families for comparison */
enum {
	AZO_FAMILY_BOOLEAN,
	AZO_FAMILY_POINTER,
	AZO_FAMILY_ARITHMETIC,
	AZO_FAMILY_BLOCK,
	AZO_FAMILY_VALUE
};

static unsigned int
get_family (unsigned int type)
{
	if (type == AZ_TYPE_BOOLEAN) return AZO_FAMILY_BOOLEAN;
	if (type == AZ_TYPE_POINTER) return AZO_FAMILY_POINTER;
	if (AZ_TYPE_IS_ARITHMETIC (type)) return AZO_FAMILY_ARITHMETIC;
	if (AZ_TYPE_IS_BLOCK (type)) return AZO_FAMILY_BLOCK;
	return AZO_FAMILY_VALUE;
}

/* A value is null if it has no type, no value storage, or is a NULL pointer/block */
/* Interfaces are block subtypes (blocks living inside another instance) and handled here as blocks */

static unsigned int
value_is_null (const AZImplementation *impl, const AZValue *val)
{
	unsigned int type;
	/* NULL impl is the canonical NONE */
	if (!impl) return 1;
	type = AZ_IMPL_TYPE (impl);
	if (!val) return 1;
	if (type == AZ_TYPE_POINTER) return !val->pointer_v;
	if (AZ_TYPE_IS_BLOCK (type)) return !val->block;
	return 0;
}

/* Unbox boxed values and interfaces for transparent comparison
 * Boxed value: the content is (box->klass->impl, &box->val)
 * Boxed interface: the interface view (box->impl, box->inst) - the instance pointer is
 * materialized into a block value so block comparison applies uniformly
 * Returns the effective implementation, stores the effective value to effective
 * (tmp provides storage for a materialized boxed interface value)
 */

static const AZImplementation *
compare_unbox (const AZImplementation *impl, const AZValue *val, AZValue *tmp, const AZValue **effective)
{
	/* Boxes are storage containers, never typed NULL themselves */
	if (impl == AZ_BOXED_VALUE_IMPL) {
		const AZBoxedValue *box = (const AZBoxedValue *) val->block;
		*effective = &box->val;
		return &box->klass->impl;
	}
	if (impl == AZ_BOXED_INTERFACE_IMPL) {
		const AZBoxedInterface *box = (const AZBoxedInterface *) val->block;
		tmp->block = box->inst;
		*effective = tmp;
		return box->impl;
	}
	*effective = val;
	return impl;
}

/* Arithmetic value classes */
enum {
	ARITH_SIGNED_INT,
	ARITH_UNSIGNED_INT,
	ARITH_REAL,
	ARITH_COMPLEX
};

static unsigned int
arith_class (unsigned int type)
{
	if (AZ_TYPE_IS_UNSIGNED (type)) return ARITH_UNSIGNED_INT;
	if (AZ_TYPE_IS_INTEGRAL (type)) return ARITH_SIGNED_INT;
	if ((type == AZ_TYPE_COMPLEX_FLOAT) || (type == AZ_TYPE_COMPLEX_DOUBLE)) return ARITH_COMPLEX;
	return ARITH_REAL;
}

static int64_t
get_signed (const AZValue *val, unsigned int type)
{
	switch (type) {
	case AZ_TYPE_INT8: return val->int8_v;
	case AZ_TYPE_INT16: return val->int16_v;
	case AZ_TYPE_INT32: return val->int32_v;
	default: return val->int64_v;
	}
}

static uint64_t
get_unsigned (const AZValue *val, unsigned int type)
{
	switch (type) {
	case AZ_TYPE_UINT8: return val->uint8_v;
	case AZ_TYPE_UINT16: return val->uint16_v;
	case AZ_TYPE_UINT32: return val->uint32_v;
	default: return val->uint64_v;
	}
}

static double
get_real (const AZValue *val, unsigned int type)
{
	return (type == AZ_TYPE_FLOAT) ? val->float_v : val->double_v;
}

static void
get_complex (const AZValue *val, unsigned int type, double *r, double *i)
{
	if (type == AZ_TYPE_COMPLEX_FLOAT) {
		*r = val->cfloat_v.r;
		*i = val->cfloat_v.i;
	} else {
		*r = val->cdouble_v.r;
		*i = val->cdouble_v.i;
	}
}

/* Double equality: NaN == NaN is true for now, -0.0 == +0.0 */

static unsigned int
dbl_eq (double lhs, double rhs)
{
	return (lhs == rhs) || (isnan (lhs) && isnan (rhs));
}

/* Exact integer/double equality (no precision loss for big integers) */

static unsigned int
eq_i64_dbl (int64_t i, double d)
{
	if (isnan (d) || (d != floor (d))) return 0;
	if ((d < -9223372036854775808.0) || (d >= 9223372036854775808.0)) return 0;
	return i == (int64_t) d;
}

static unsigned int
eq_u64_dbl (uint64_t u, double d)
{
	if (isnan (d) || (d != floor (d))) return 0;
	if ((d < 0.0) || (d >= 18446744073709551616.0)) return 0;
	return u == (uint64_t) d;
}

/* Exact equality of a complex (re, im) and a real value */

static unsigned int
eq_complex_real (double re, double im, unsigned int rclass, const AZValue *rval, unsigned int rtype)
{
	if (im != 0.0) return 0;
	switch (rclass) {
	case ARITH_SIGNED_INT: return eq_i64_dbl (get_signed (rval, rtype), re);
	case ARITH_UNSIGNED_INT: return eq_u64_dbl (get_unsigned (rval, rtype), re);
	default: return dbl_eq (re, get_real (rval, rtype));
	}
}

/* Exact mathematical value equality across the arithmetic family */

static unsigned int
arith_equal (unsigned int ltype, const AZValue *lval, unsigned int rtype, const AZValue *rval)
{
	unsigned int lclass = arith_class (ltype);
	unsigned int rclass = arith_class (rtype);
	if ((lclass == ARITH_COMPLEX) || (rclass == ARITH_COMPLEX)) {
		if ((lclass == ARITH_COMPLEX) && (rclass == ARITH_COMPLEX)) {
			double lr, li, rr, ri;
			get_complex (lval, ltype, &lr, &li);
			get_complex (rval, rtype, &rr, &ri);
			return dbl_eq (lr, rr) && dbl_eq (li, ri);
		} else if (lclass == ARITH_COMPLEX) {
			double lr, li;
			get_complex (lval, ltype, &lr, &li);
			return eq_complex_real (lr, li, rclass, rval, rtype);
		} else {
			double rr, ri;
			get_complex (rval, rtype, &rr, &ri);
			return eq_complex_real (rr, ri, lclass, lval, ltype);
		}
	}
	switch (lclass) {
	case ARITH_SIGNED_INT: {
		int64_t i = get_signed (lval, ltype);
		if (rclass == ARITH_SIGNED_INT) return i == get_signed (rval, rtype);
		if (rclass == ARITH_UNSIGNED_INT) {
			int64_t r = (int64_t) get_unsigned (rval, rtype);
			return (i >= 0) && ((uint64_t) i == (uint64_t) r);
		}
		return eq_i64_dbl (i, get_real (rval, rtype));
	}
	case ARITH_UNSIGNED_INT: {
		uint64_t u = get_unsigned (lval, ltype);
		if (rclass == ARITH_SIGNED_INT) {
			int64_t r = get_signed (rval, rtype);
			return (r >= 0) && (u == (uint64_t) r);
		}
		if (rclass == ARITH_UNSIGNED_INT) return u == get_unsigned (rval, rtype);
		return eq_u64_dbl (u, get_real (rval, rtype));
	}
	default: {
		double d = get_real (lval, ltype);
		if (rclass == ARITH_SIGNED_INT) return eq_i64_dbl (get_signed (rval, rtype), d);
		if (rclass == ARITH_UNSIGNED_INT) return eq_u64_dbl (get_unsigned (rval, rtype), d);
		return dbl_eq (d, get_real (rval, rtype));
	}
	}
}

/* The sizes of arithmetic types for bitwise identity comparison */

static unsigned int
arith_size (unsigned int type)
{
	switch (type) {
	case AZ_TYPE_INT8:
	case AZ_TYPE_UINT8:
		return 1;
	case AZ_TYPE_INT16:
	case AZ_TYPE_UINT16:
		return 2;
	case AZ_TYPE_INT32:
	case AZ_TYPE_UINT32:
	case AZ_TYPE_FLOAT:
		return 4;
	case AZ_TYPE_INT64:
	case AZ_TYPE_UINT64:
	case AZ_TYPE_DOUBLE:
	case AZ_TYPE_COMPLEX_FLOAT:
		return 8;
	default: /* AZ_TYPE_COMPLEX_DOUBLE */
		return 16;
	}
}

unsigned int
azo_compare_equal (const AZImplementation *lhs_impl, const AZValue *lhs, const AZImplementation *rhs_impl, const AZValue *rhs, AZValue *result)
{
	unsigned int lnull, rnull, lfamily, rfamily;
	AZValue ltmp, rtmp;
	const AZValue *lval = lhs, *rval = rhs;
	result->boolean_v = 0;
	/* Unbox storage containers (boxed value/interface) for transparent comparison */
	if (lhs_impl && lhs) lhs_impl = compare_unbox (lhs_impl, lhs, &ltmp, &lval);
	if (rhs_impl && rhs) rhs_impl = compare_unbox (rhs_impl, rhs, &rtmp, &rval);
	lnull = value_is_null (lhs_impl, lval);
	rnull = value_is_null (rhs_impl, rval);
	if (lnull || rnull) {
		/* All nulls form one equality class, null vs. non-null is a valid comparison */
		result->boolean_v = lnull && rnull;
		return AZO_COMPARE_OK;
	}
	lfamily = get_family (AZ_IMPL_TYPE (lhs_impl));
	rfamily = get_family (AZ_IMPL_TYPE (rhs_impl));
	if (lfamily != rfamily) return AZO_COMPARE_INVALID_TYPE;
	switch (lfamily) {
	case AZO_FAMILY_BOOLEAN:
		result->boolean_v = ((lval->boolean_v != 0) == (rval->boolean_v != 0));
		return AZO_COMPARE_OK;
	case AZO_FAMILY_POINTER:
		result->boolean_v = (lval->pointer_v == rval->pointer_v);
		return AZO_COMPARE_OK;
	case AZO_FAMILY_ARITHMETIC:
		result->boolean_v = arith_equal (AZ_IMPL_TYPE (lhs_impl), lval, AZ_IMPL_TYPE (rhs_impl), rval);
		return AZO_COMPARE_OK;
	case AZO_FAMILY_BLOCK:
		result->boolean_v = (lval->block == rval->block);
		return AZO_COMPARE_OK;
	case AZO_FAMILY_VALUE:
		if (lhs_impl != rhs_impl) return AZO_COMPARE_DIFFERENT_TYPE;
		result->boolean_v = !memcmp (lval, rval, AZ_CLASS_FROM_IMPL (lhs_impl)->instance_size);
		return AZO_COMPARE_OK;
	default:
		return AZO_COMPARE_INVALID_TYPE;
	}
}

unsigned int
azo_compare_identical (const AZImplementation *lhs_impl, const AZValue *lhs, const AZImplementation *rhs_impl, const AZValue *rhs, AZValue *result)
{
	unsigned int lnull, rnull, type;
	AZValue ltmp, rtmp;
	const AZValue *lval = lhs, *rval = rhs;
	result->boolean_v = 0;
	/* Unbox storage containers (boxed value/interface) for transparent comparison */
	if (lhs_impl && lhs) lhs_impl = compare_unbox (lhs_impl, lhs, &ltmp, &lval);
	if (rhs_impl && rhs) rhs_impl = compare_unbox (rhs_impl, rhs, &rtmp, &rval);
	lnull = value_is_null (lhs_impl, lval);
	rnull = value_is_null (rhs_impl, rval);
	if (lnull != rnull) {
		/* Null vs. non-null is a valid comparison */
		return AZO_COMPARE_OK;
	}
	if (lnull) {
		/* Both null: identical only with the same implementation (untyped === untyped) */
		if (lhs_impl == rhs_impl) {
			result->boolean_v = 1;
			return AZO_COMPARE_OK;
		}
		return AZO_COMPARE_DIFFERENT_TYPE;
	}
	/* Different implementations cannot be identical: for non-interfaces these are
	 * different classes, for interfaces implementations of different classes -
	 * cheap test before going to the type */
	if (lhs_impl != rhs_impl) return AZO_COMPARE_DIFFERENT_TYPE;
	/* Same implementation: same type and family */
	type = AZ_IMPL_TYPE (lhs_impl);
	switch (get_family (type)) {
	case AZO_FAMILY_BOOLEAN:
		result->boolean_v = ((lval->boolean_v != 0) == (rval->boolean_v != 0));
		return AZO_COMPARE_OK;
	case AZO_FAMILY_POINTER:
		result->boolean_v = (lval->pointer_v == rval->pointer_v);
		return AZO_COMPARE_OK;
	case AZO_FAMILY_ARITHMETIC:
		/* Bitwise: -0.0 !== +0.0, NaN === NaN (same bit pattern) */
		result->boolean_v = !memcmp (lval, rval, arith_size (type));
		return AZO_COMPARE_OK;
	case AZO_FAMILY_BLOCK:
		result->boolean_v = (lval->block == rval->block);
		return AZO_COMPARE_OK;
	case AZO_FAMILY_VALUE:
		result->boolean_v = !memcmp (lval, rval, AZ_CLASS_FROM_IMPL (lhs_impl)->instance_size);
		return AZO_COMPARE_OK;
	default:
		return AZO_COMPARE_INVALID_TYPE;
	}
}


/* Equality comparison (== and !=)
 * Statically known same primitive type: typed fast path (EQUAL_TYPED)
 * Everything else: the full azo_compare_equal semantics (EQUAL) - null equality
 * class, exact cross-type arithmetic value equality, blocks by pointer, value
 * types by content, INVALID_TYPE on incomparable types */

static unsigned int
azo_compiler_compile_comparison_eq (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, unsigned int comp_type, AZOSource *src, unsigned int reg)
{
	AZOCode *code = &ctx->frame->code;
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
	if ((lhs->type == rhs->type) && AZ_TYPE_IS_PRIMITIVE (lhs->type)) {
		azo_code_write_EQUAL_TYPED (code, lhs->type, expr);
	} else {
		azo_code_write_ic (code, EQUAL, expr);
	}
	if (comp_type == AZO_TERM_COMPARISON_NE) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
	return 1;
}

/* Identity comparison (=== and !==): always the full azo_compare_identical
 * semantics (implementation + value identity) - no typed variant needed because
 * there are no costly non-matching type arithmetic comparisons */

static unsigned int
azo_compiler_compile_comparison_identical (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, unsigned int comp_type, AZOSource *src, unsigned int reg)
{
	AZOCode *code = &ctx->frame->code;
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
	azo_code_write_ic (code, IDENTICAL, expr);
	if (comp_type == AZO_TERM_COMPARISON_NOT_IDENTICAL) {
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	}
	return 1;
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
	azo_code_write_EXCEPTION (code, AZ_EXCEPTION_INVALID_TYPE, expr);


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
	azo_code_write_EXCEPTION (code, AZ_EXCEPTION_INVALID_TYPE, expr);


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
		return azo_compiler_compile_comparison_identical (comp, ctx, lhs, rhs, expr, expr->term.subtype, src, reg);
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

