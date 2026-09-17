#define __AZO_CALCULATE_CONSTANTS_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021-2026
*/


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <arikkei/arikkei-strlib.h>

#include <az/class.h>
#include <az/complex.h>
#include <az/field.h>
#include <az/string.h>
#include <az/primitives.h>
#include <az/classes/value-array.h>

#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>
#include <azo/keyword.h>
#include <azo/node.h>
#include <azo/source.h>

static int calculate_logic (AZOOptimizer *opt, AZONode *expr);
static int calculate_shift (AZOOptimizer *opt, AZONode *expr, AZOSource *src);
static int calculate_arithmetic (AZOOptimizer *opt, AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, AZOSource *src);

/* Arithmetic operator symbols indexed by AZO_TERM_ARITHMETIC_* subtype */
static const char *arithmetic_op_names[] = { "+", "-", "/", "*", "%", "<<", ">>", "&", "&&", "|", "||", "^" };

/* Human-readable name of an AZ type (via the class table) */
static const char *
type_name (unsigned int type)
{
	AZClass *klass = az_type_get_class (type);
	return klass ? (const char *) klass->name : "?";
}

/* Render an AZ value of the given type into buf */
static const char *
value_string (unsigned int type, const AZValue *val, unsigned char *buf, unsigned int buflen)
{
	const AZImplementation *impl = AZ_IMPL_FROM_TYPE (type);
	if (!impl) {
		snprintf ((char *) buf, buflen, "?");
		return (const char *) buf;
	}
	az_instance_to_string (impl, az_value_get_inst (impl, val), buf, buflen);
	buf[buflen - 1] = 0;
	return (const char *) buf;
}

/*
 * Report a constant-folding error or warning
 *
 * severity - "ERROR" or "WARNING"
 * Prints the source location and the offending source text, then a description
 * of the operation as "type value op type value", then the message. If the
 * operation produced a result by rounding or clamping, the result is shown too.
 */
static void
calculate_report (const char *severity, const AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, const char *msg, AZOSource *src)
{
	unsigned char a_buf[128], b_buf[128];
	/* Find source location from the node */
	unsigned int line = 0;
	if (azo_source_find_line_range (src, expr->term.start, expr->term.end, &line, NULL)) {
		fprintf (stderr, "%s at line %u: ", severity, line);
	} else {
		fprintf (stderr, "%s: ", severity);
	}
	/* Print the operation being folded */
	fprintf (stderr, "%s %s %s %s %s: %s\n",
		type_name (lhs_type), value_string (lhs_type, lhs, a_buf, sizeof (a_buf)),
		arithmetic_op_names[expr->term.subtype],
		type_name (rhs_type), value_string (rhs_type, rhs, b_buf, sizeof (b_buf)),
		msg);
	/* Print the source text of the node */
	azo_source_print_lines_of_token (src, expr->term.start, expr->term.end, stderr);
}

/* Warn that a constant conversion was inexact (rounded or clamped), showing the result */
static void
calculate_warn_conversion (const AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int result_type, const AZValue *result, unsigned int conv, AZOSource *src)
{
	unsigned char r_buf[128];
	unsigned char msg[256];
	snprintf ((char *) msg, sizeof (msg), "conversion is %s, result %s %s",
		(conv == AZ_CONVERSION_ROUNDED) ? "rounded" : "clamped",
		type_name (result_type), value_string (result_type, result, r_buf, sizeof (r_buf)));
	calculate_report ("WARNING", expr, lhs_type, lhs, rhs_type, rhs, (const char *) msg, src);
}

/*
 * Warn if a floating-point operation on finite operands overflows to infinity
 * or produces NaN (e.g. 1e38 * 1e38, 0.0 / 0.0). Operands that were already
 * infinite/NaN do not trigger the warning - their arithmetic is well-defined.
 * a/b/c are the operands converted to the common type and the result.
 */
static void
calculate_warn_fp_result (const AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int type, const AZValue *a, const AZValue *b, const AZValue *c, AZOSource *src)
{
	int a_finite, b_finite, c_finite, c_nan;
	switch (type) {
	case AZ_TYPE_FLOAT:
		a_finite = isfinite (a->float_v); b_finite = isfinite (b->float_v);
		c_finite = isfinite (c->float_v); c_nan = isnan (c->float_v);
		break;
	case AZ_TYPE_DOUBLE:
		a_finite = isfinite (a->double_v); b_finite = isfinite (b->double_v);
		c_finite = isfinite (c->double_v); c_nan = isnan (c->double_v);
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		a_finite = isfinite (a->cfloat_v.r) && isfinite (a->cfloat_v.i);
		b_finite = isfinite (b->cfloat_v.r) && isfinite (b->cfloat_v.i);
		c_finite = isfinite (c->cfloat_v.r) && isfinite (c->cfloat_v.i);
		c_nan = isnan (c->cfloat_v.r) || isnan (c->cfloat_v.i);
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		a_finite = isfinite (a->cdouble_v.r) && isfinite (a->cdouble_v.i);
		b_finite = isfinite (b->cdouble_v.r) && isfinite (b->cdouble_v.i);
		c_finite = isfinite (c->cdouble_v.r) && isfinite (c->cdouble_v.i);
		c_nan = isnan (c->cdouble_v.r) || isnan (c->cdouble_v.i);
		break;
	default:
		return;
	}
	if (a_finite && b_finite && !c_finite) {
		calculate_report ("WARNING", expr, lhs_type, lhs, rhs_type, rhs,
			c_nan ? "result is NaN" : "result is infinite (overflow)", src);
	}
}

static void
node_replace_with_constant(AZONode *node, unsigned int type, const AZValue *val)
{
	azo_node_clear_children(node);
	node->term.type = AZO_TERM_CONSTANT;
	node->term.subtype = type;
	az_packed_value_set_from_type_value(&node->value, type, val);
}

/* Get needed precision if type is converted to floating point */

static unsigned int
get_float_promotion (unsigned int type)
{
	if ((type == AZ_TYPE_INT64) || (type == AZ_TYPE_UINT64) || (type == AZ_TYPE_DOUBLE) || (type == AZ_TYPE_COMPLEX_DOUBLE)) return AZ_TYPE_DOUBLE;
	return AZ_TYPE_FLOAT;
}

static int
calculate_logic (AZOOptimizer *opt, AZONode *expr)
{
	AZONode *lhs, *rhs;
	lhs = expr->children;
	rhs = lhs->next;
	if ((lhs->term.type != AZO_TERM_CONSTANT) || (rhs->term.type != AZO_TERM_CONSTANT)) return 1;
	/* Logical binary operators */
	if ((lhs->term.subtype != AZ_TYPE_BOOLEAN) || (rhs->term.subtype != AZ_TYPE_BOOLEAN)) {
		fprintf (stderr, "calculate: Invalid types for logical binary operator %u - lhs %u rhs %u\n", expr->term.subtype, lhs->term.subtype, rhs->term.subtype);
		return 1;
	}
	expr->term.type = AZO_TERM_CONSTANT;
	expr->term.subtype = AZ_TYPE_BOOLEAN;
	if (expr->term.subtype == AZO_TERM_ARITHMETIC_ANDAND) {
		az_packed_value_set_boolean (&expr->value, lhs->value.v.boolean_v && rhs->value.v.boolean_v);
	} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_OROR) {
		az_packed_value_set_boolean (&expr->value, lhs->value.v.boolean_v || rhs->value.v.boolean_v);
	}
	opt->n_const_subst += 1;
	return 0;
}

static int
calculate_shift (AZOOptimizer *opt, AZONode *expr, AZOSource *src)
{
	AZONode *lhs, *rhs;
	AZValue a, b, c;
	unsigned int type;

	lhs = expr->children;
	rhs = lhs->next;
	if ((lhs->term.type != AZO_TERM_CONSTANT) || (rhs->term.type != AZO_TERM_CONSTANT)) return 1;

	/* Both operands are constant expressions */
	if (!AZ_TYPE_IS_INTEGRAL (lhs->term.subtype) || !AZ_TYPE_IS_INTEGRAL (rhs->term.subtype)) {
		calculate_report ("ERROR", expr, lhs->term.subtype, &lhs->value.v, rhs->term.subtype, &rhs->value.v, "shift requires integral operands", src);
		return 1;
	}
	/* The shift count is always read as int32 (it is independent of the value width) */
	az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->term.subtype, &rhs->value.v);
	if (lhs->term.subtype == AZ_TYPE_INT64) {
		/* Int64 */
		az_convert_arithmetic_type (AZ_TYPE_INT64, &a, lhs->term.subtype, &lhs->value.v);
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.int64_v = a.int64_v << b.int32_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.int64_v = a.int64_v >> b.int32_v;
		} else {
			calculate_report ("ERROR", expr, lhs->term.subtype, &lhs->value.v, rhs->term.subtype, &rhs->value.v, "invalid shift operator", src);
			expr->term.type = AZO_TERM_INVALID;
			return 1;
		}
		type = AZ_TYPE_INT64;
	} else if (lhs->term.subtype == AZ_TYPE_UINT64) {
		/* Uint64 */
		az_convert_arithmetic_type (AZ_TYPE_UINT64, &a, lhs->term.subtype, &lhs->value.v);
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.uint64_v = a.uint64_v << b.int32_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.uint64_v = a.uint64_v >> b.int32_v;
		} else {
			calculate_report ("ERROR", expr, lhs->term.subtype, &lhs->value.v, rhs->term.subtype, &rhs->value.v, "invalid shift operator", src);
			expr->term.type = AZO_TERM_INVALID;
			return 1;
		}
		type = AZ_TYPE_UINT64;
	} else if (AZ_TYPE_IS_SIGNED (lhs->term.subtype)) {
		/* Int32 - any signed type */
		az_convert_arithmetic_type (AZ_TYPE_INT32, &a, lhs->term.subtype, &lhs->value.v);
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.int32_v = a.int32_v << b.int32_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.int32_v = a.int32_v >> b.int32_v;
		} else {
			calculate_report ("ERROR", expr, lhs->term.subtype, &lhs->value.v, rhs->term.subtype, &rhs->value.v, "invalid shift operator", src);
			expr->term.type = AZO_TERM_INVALID;
			return 1;
		}
		type = AZ_TYPE_INT32;
	} else {
		/* Uint32 - small unsigned types */
		az_convert_arithmetic_type (AZ_TYPE_UINT32, &a, lhs->term.subtype, &lhs->value.v);
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.uint32_v = a.uint32_v << b.int32_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.uint32_v = a.uint32_v >> b.int32_v;
		} else {
			calculate_report ("ERROR", expr, lhs->term.subtype, &lhs->value.v, rhs->term.subtype, &rhs->value.v, "invalid shift operator", src);
			expr->term.type = AZO_TERM_INVALID;
			return 1;
		}
		type = AZ_TYPE_UINT32;
	}
	node_replace_with_constant(expr, type, &c);
	opt->n_const_subst += 1;
	return 0;
}

/* #define DEBUG_CALCULATE_ARITHMETIC */

/* Checked integer arithmetic - returns 1 (and sets *out) on success, 0 on overflow/divide-by-zero */

static int
int_add_i32 (int32_t x, int32_t y, int32_t *out) { return !__builtin_add_overflow (x, y, out); }
static int
int_sub_i32 (int32_t x, int32_t y, int32_t *out) { return !__builtin_sub_overflow (x, y, out); }
static int
int_mul_i32 (int32_t x, int32_t y, int32_t *out) { return !__builtin_mul_overflow (x, y, out); }
static int
int_add_i64 (int64_t x, int64_t y, int64_t *out) { return !__builtin_add_overflow (x, y, out); }
static int
int_sub_i64 (int64_t x, int64_t y, int64_t *out) { return !__builtin_sub_overflow (x, y, out); }
static int
int_mul_i64 (int64_t x, int64_t y, int64_t *out) { return !__builtin_mul_overflow (x, y, out); }
static int
int_add_u32 (uint32_t x, uint32_t y, uint32_t *out) { return !__builtin_add_overflow (x, y, out); }
static int
int_sub_u32 (uint32_t x, uint32_t y, uint32_t *out) { return !__builtin_sub_overflow (x, y, out); }
static int
int_mul_u32 (uint32_t x, uint32_t y, uint32_t *out) { return !__builtin_mul_overflow (x, y, out); }
static int
int_add_u64 (uint64_t x, uint64_t y, uint64_t *out) { return !__builtin_add_overflow (x, y, out); }
static int
int_sub_u64 (uint64_t x, uint64_t y, uint64_t *out) { return !__builtin_sub_overflow (x, y, out); }
static int
int_mul_u64 (uint64_t x, uint64_t y, uint64_t *out) { return !__builtin_mul_overflow (x, y, out); }

static int
calculate_arithmetic (AZOOptimizer *opt, AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, AZOSource *src)
{
	AZValue a, b, c;
	unsigned int type;

	/* Both operands are constant expressions */
	if (!AZ_TYPE_IS_ARITHMETIC (lhs_type) || !AZ_TYPE_IS_ARITHMETIC (rhs_type)) {
		calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "operands are not arithmetic", src);
		return 1;
	}
	unsigned int lhs_fp = get_float_promotion(lhs_type);
	unsigned int rhs_fp = get_float_promotion (rhs_type);
	if ((lhs_type == AZ_TYPE_COMPLEX_DOUBLE) || (rhs_type == AZ_TYPE_COMPLEX_DOUBLE) ||
		((lhs_type == AZ_TYPE_COMPLEX_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs_type == AZ_TYPE_COMPLEX_FLOAT))) {
		/*
		 * The result is complex double if:
		 *   - either argument is complex double
		 *   - one argument is complex float and the other is double or 64-bit integer
		 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_COMPLEX_DOUBLE, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_COMPLEX_DOUBLE, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to complex double", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' */
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_PLUS) {
			az_complexd_add (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_MINUS) {
			az_complexd_sub (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_STAR) {
			az_complexd_mul (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SLASH) {
			az_complexd_div (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for complex double", src);
			return 1;
		}
		type = AZ_TYPE_COMPLEX_DOUBLE;
		calculate_warn_fp_result (expr, lhs_type, lhs, rhs_type, rhs, type, &a, &b, &c, src);
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if ((lhs_type == AZ_TYPE_COMPLEX_FLOAT) || (rhs_type == AZ_TYPE_COMPLEX_FLOAT)) {
		/* The result is complex float */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_COMPLEX_FLOAT, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_COMPLEX_FLOAT, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to complex float", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' */
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_PLUS) {
			az_complexf_add (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_MINUS) {
			az_complexf_sub (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_STAR) {
			az_complexf_mul (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SLASH) {
			az_complexf_div (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for complex float", src);
			return 1;
		}
		type = AZ_TYPE_COMPLEX_FLOAT;
		calculate_warn_fp_result (expr, lhs_type, lhs, rhs_type, rhs, type, &a, &b, &c, src);
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if ((lhs_type == AZ_TYPE_DOUBLE) || (rhs_type == AZ_TYPE_DOUBLE) ||
		((lhs_type == AZ_TYPE_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs_type == AZ_TYPE_FLOAT))) {
		/*
		 * The result is double if:
		 *   - either argument is double
		 *   - one argument is float and the other is 64-bit integer
		 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_DOUBLE, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_DOUBLE, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to double", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' */
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_PLUS) {
			c.double_v = a.double_v + b.double_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_MINUS) {
			c.double_v = a.double_v - b.double_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_STAR) {
			c.double_v = a.double_v * b.double_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SLASH) {
			c.double_v = a.double_v / b.double_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_PERCENT) {
			c.double_v = fmod (a.double_v, b.double_v);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for double", src);
			return 1;
		}
		type = AZ_TYPE_DOUBLE;
		calculate_warn_fp_result (expr, lhs_type, lhs, rhs_type, rhs, type, &a, &b, &c, src);
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if ((lhs_type == AZ_TYPE_FLOAT) || (rhs_type == AZ_TYPE_FLOAT)) {
		/* The result is float */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_FLOAT, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_FLOAT, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to float", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' */
		if (expr->term.subtype == AZO_TERM_ARITHMETIC_PLUS) {
			c.float_v = a.float_v + b.float_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_MINUS) {
			c.float_v = a.float_v - b.float_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_STAR) {
			c.float_v = a.float_v * b.float_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_SLASH) {
			c.float_v = a.float_v / b.float_v;
		} else if (expr->term.subtype == AZO_TERM_ARITHMETIC_PERCENT) {
			c.float_v = fmodf (a.float_v, b.float_v);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for float", src);
			return 1;
		}
		type = AZ_TYPE_FLOAT;
		calculate_warn_fp_result (expr, lhs_type, lhs, rhs_type, rhs, type, &a, &b, &c, src);
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if ((lhs_type == AZ_TYPE_INT64) || (rhs_type == AZ_TYPE_INT64) ||
		(((lhs_type == AZ_TYPE_UINT64) || (lhs_type == AZ_TYPE_UINT32)) && AZ_TYPE_IS_SIGNED(rhs_type)) ||
		(AZ_TYPE_IS_SIGNED(lhs_type) && ((rhs_type == AZ_TYPE_UINT64) || (rhs_type == AZ_TYPE_UINT32)))) {
		/* The result is int64 if:
		 *   - either argument is int64
		 *   - one argument is signed and the other is either uint64 or uint32
		 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_INT64, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_INT64, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to int64", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' '&' '|' '^' */
		int ok = 1;
		switch (expr->term.subtype) {
		case AZO_TERM_ARITHMETIC_PLUS: ok = int_add_i64 (a.int64_v, b.int64_v, &c.int64_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: ok = int_sub_i64 (a.int64_v, b.int64_v, &c.int64_v); break;
		case AZO_TERM_ARITHMETIC_STAR: ok = int_mul_i64 (a.int64_v, b.int64_v, &c.int64_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (b.int64_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer division by zero", src); return 1; }
			c.int64_v = a.int64_v / b.int64_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (b.int64_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer modulo by zero", src); return 1; }
			c.int64_v = a.int64_v % b.int64_v; break;
		case AZO_TERM_ARITHMETIC_AND: c.int64_v = a.int64_v & b.int64_v; break;
		case AZO_TERM_ARITHMETIC_OR: c.int64_v = a.int64_v | b.int64_v; break;
		case AZO_TERM_ARITHMETIC_CARET: c.int64_v = a.int64_v ^ b.int64_v; break;
		default:
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for int64", src);
			return 1;
		}
		if (!ok) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer overflow", src); return 1; }
		type = AZ_TYPE_INT64;
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if ((lhs_type == AZ_TYPE_UINT64) || (rhs_type == AZ_TYPE_UINT64)) {
		/* The result is uint64 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_UINT64, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_UINT64, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to uint64", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' '&' '|' '^' */
		int ok = 1;
		switch (expr->term.subtype) {
		case AZO_TERM_ARITHMETIC_PLUS: ok = int_add_u64 (a.uint64_v, b.uint64_v, &c.uint64_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: ok = int_sub_u64 (a.uint64_v, b.uint64_v, &c.uint64_v); break;
		case AZO_TERM_ARITHMETIC_STAR: ok = int_mul_u64 (a.uint64_v, b.uint64_v, &c.uint64_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (b.uint64_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer division by zero", src); return 1; }
			c.uint64_v = a.uint64_v / b.uint64_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (b.uint64_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer modulo by zero", src); return 1; }
			c.uint64_v = a.uint64_v % b.uint64_v; break;
		case AZO_TERM_ARITHMETIC_AND: c.uint64_v = a.uint64_v & b.uint64_v; break;
		case AZO_TERM_ARITHMETIC_OR: c.uint64_v = a.uint64_v | b.uint64_v; break;
		case AZO_TERM_ARITHMETIC_CARET: c.uint64_v = a.uint64_v ^ b.uint64_v; break;
		default:
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for uint64", src);
			return 1;
		}
		if (!ok) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "unsigned integer overflow", src); return 1; }
		type = AZ_TYPE_UINT64;
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else if (AZ_TYPE_IS_SIGNED (lhs_type) || AZ_TYPE_IS_SIGNED (rhs_type)) {
		/* The result is int32 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_INT32, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to int32", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' '&' '|' '^' */
		int ok = 1;
		switch (expr->term.subtype) {
		case AZO_TERM_ARITHMETIC_PLUS: ok = int_add_i32 (a.int32_v, b.int32_v, &c.int32_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: ok = int_sub_i32 (a.int32_v, b.int32_v, &c.int32_v); break;
		case AZO_TERM_ARITHMETIC_STAR: ok = int_mul_i32 (a.int32_v, b.int32_v, &c.int32_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (b.int32_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer division by zero", src); return 1; }
			c.int32_v = a.int32_v / b.int32_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (b.int32_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer modulo by zero", src); return 1; }
			c.int32_v = a.int32_v % b.int32_v; break;
		case AZO_TERM_ARITHMETIC_AND: c.int32_v = a.int32_v & b.int32_v; break;
		case AZO_TERM_ARITHMETIC_OR: c.int32_v = a.int32_v | b.int32_v; break;
		case AZO_TERM_ARITHMETIC_CARET: c.int32_v = a.int32_v ^ b.int32_v; break;
		default:
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for int32", src);
			return 1;
		}
		if (!ok) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer overflow", src); return 1; }
		type = AZ_TYPE_INT32;
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	} else {
		/* The result is uint32 */
		unsigned int conv_a = az_convert_arithmetic_type (AZ_TYPE_UINT32, &a, lhs_type, lhs);
		unsigned int conv_b = az_convert_arithmetic_type (AZ_TYPE_UINT32, &b, rhs_type, rhs);
		if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "cannot convert operand to uint32", src);
			return 1;
		}
		/* Allowed operators: '+' '-' '*' '/' '%' '&' '|' '^' */
		int ok = 1;
		switch (expr->term.subtype) {
		case AZO_TERM_ARITHMETIC_PLUS: ok = int_add_u32 (a.uint32_v, b.uint32_v, &c.uint32_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: ok = int_sub_u32 (a.uint32_v, b.uint32_v, &c.uint32_v); break;
		case AZO_TERM_ARITHMETIC_STAR: ok = int_mul_u32 (a.uint32_v, b.uint32_v, &c.uint32_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (b.uint32_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer division by zero", src); return 1; }
			c.uint32_v = a.uint32_v / b.uint32_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (b.uint32_v == 0) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer modulo by zero", src); return 1; }
			c.uint32_v = a.uint32_v % b.uint32_v; break;
		case AZO_TERM_ARITHMETIC_AND: c.uint32_v = a.uint32_v & b.uint32_v; break;
		case AZO_TERM_ARITHMETIC_OR: c.uint32_v = a.uint32_v | b.uint32_v; break;
		case AZO_TERM_ARITHMETIC_CARET: c.uint32_v = a.uint32_v ^ b.uint32_v; break;
		default:
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid operation for uint32", src);
			return 1;
		}
		if (!ok) { calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "unsigned integer overflow", src); return 1; }
		type = AZ_TYPE_UINT32;
		if ((conv_a != AZ_CONVERSION_EXACT) || (conv_b != AZ_CONVERSION_EXACT))
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, (conv_a != AZ_CONVERSION_EXACT) ? conv_a : conv_b, src);
	}
	/* The children are freed here, after all operand reads */
	node_replace_with_constant(expr, type, &c);
	opt->n_const_subst += 1;
	return 0;
}

int
azo_compiler_calculate_constant_binary (AZOOptimizer *opt, AZONode *node)
{
	AZONode *lhs = node->children;
	AZONode *rhs = lhs->next;
	assert((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT));
	unsigned int lhs_type = AZ_IMPL_TYPE(lhs->value.impl);
	unsigned int rhs_type = AZ_IMPL_TYPE(rhs->value.impl);
	if ((node->term.subtype == AZO_TERM_ARITHMETIC_ANDAND) || (node->term.subtype == AZO_TERM_ARITHMETIC_OROR)) {
		return calculate_logic (opt, node);
	} else if ((node->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) || (node->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT)) {
		calculate_shift (opt, node, opt->comp->src);
	} else {
		return calculate_arithmetic (opt, node, lhs_type, &lhs->value.v, rhs_type, &rhs->value.v, opt->comp->src);
	}
	return 0;
}

int
azo_compiler_calculate_rvalue_prefix(AZOOptimizer *opt, AZONode *node)
{
	AZONode *rhs = node->children;
	if (rhs->term.type != AZO_TERM_CONSTANT) return 0;
	/* Operand is constant expressions */
	if (node->term.subtype == AZO_TERM_PREFIX_NOT) {
		if (rhs->term.subtype != AZ_TYPE_BOOLEAN) {
			fprintf (stderr, "calculate_prefix_not: expression is not boolean (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
			return 1;
		}
		node->term.type = AZO_TERM_CONSTANT;
		node->term.subtype = AZ_TYPE_BOOLEAN;
		az_packed_value_set_boolean(&node->value, !rhs->value.v.boolean_v);
		azo_node_clear_children(node);
		opt->n_const_subst += 1;
		return 0;
	}
	/* '+' '-' '~' require arithmetic types */
	if (!AZ_TYPE_IS_ARITHMETIC (rhs->term.subtype)) {
		fprintf (stderr, "calculate_rvalue_prefix: rhs is not arithmetic type (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
		return 1;
	}
	/* '+' is no-op */
	if (node->term.subtype == AZO_TERM_PREFIX_PLUS) {
		node->term.type = AZO_TERM_CONSTANT;
		node->term.subtype = rhs->term.subtype;
		node->value = rhs->value;
		azo_node_clear_children(node);
		opt->n_const_subst += 1;
		return 0;
	}
	/* '-' is not allowed for unsigned types */
	if (node->term.subtype == AZO_TERM_PREFIX_MINUS) {
		if (AZ_TYPE_IS_UNSIGNED(rhs->term.subtype)) {
			fprintf (stderr, "calculate_rvalue_prefix: Invalid operator %u for unsigned type\n", node->term.subtype);
			return 1;
		}
		node->term.type = AZO_TERM_CONSTANT;
		node->term.subtype = rhs->term.subtype;
		switch (rhs->term.subtype) {
			/* fixme: overflow */
			case AZ_TYPE_INT8:
				node->value.v.int8_v = -rhs->value.v.int8_v;
				break;
			case AZ_TYPE_INT16:
				node->value.v.int16_v = -rhs->value.v.int16_v;
				break;
			case AZ_TYPE_INT32:
				node->value.v.int32_v = -rhs->value.v.int32_v;
				break;
			case AZ_TYPE_INT64:
				node->value.v.int64_v = -rhs->value.v.int64_v;
				break;
			case AZ_TYPE_FLOAT:
				node->value.v.float_v = -rhs->value.v.float_v;
				break;
			case AZ_TYPE_DOUBLE:
				node->value.v.double_v = -rhs->value.v.double_v;
				break;
			case AZ_TYPE_COMPLEX_FLOAT:
				node->value.v.cfloat_v.r = -rhs->value.v.cfloat_v.r;
				node->value.v.cfloat_v.i = -rhs->value.v.cfloat_v.i;
				break;
			case AZ_TYPE_COMPLEX_DOUBLE:
				node->value.v.cdouble_v.r = -rhs->value.v.cdouble_v.r;
				node->value.v.cdouble_v.i = -rhs->value.v.cdouble_v.i;
				break;
			default:
				assert(0);
				return 1;
		}
		azo_node_clear_children(node);
		opt->n_const_subst += 1;
		return 0;
	}
	if (node->term.subtype == AZO_TERM_PREFIX_TILDE) {
		if ((rhs->term.subtype == AZ_TYPE_FLOAT) || (rhs->term.subtype == AZ_TYPE_DOUBLE)) {
			fprintf (stderr, "calculate_rvalue_prefix: Invalid operator %u for float/double\n", node->term.subtype);
			return 1;
		}
		node->term.type = AZO_TERM_CONSTANT;
		node->term.subtype = rhs->term.subtype;
		switch (rhs->term.subtype) {
			/* fixme: overflow */
			case AZ_TYPE_INT8:
			case AZ_TYPE_UINT8:
				node->value.v.uint8_v = ~rhs->value.v.uint8_v;
				break;
			case AZ_TYPE_INT16:
			case AZ_TYPE_UINT16:
				node->value.v.uint16_v = ~rhs->value.v.uint16_v;
				break;
			case AZ_TYPE_INT32:
			case AZ_TYPE_UINT32:
				node->value.v.uint32_v = ~rhs->value.v.uint32_v;
				break;
			case AZ_TYPE_INT64:
			case AZ_TYPE_UINT64:
				node->value.v.uint64_v = ~rhs->value.v.uint64_v;
				break;
			case AZ_TYPE_COMPLEX_FLOAT:
				node->value.v.cfloat_v.r = rhs->value.v.cfloat_v.r;
				node->value.v.cfloat_v.i = -rhs->value.v.cfloat_v.i;
				break;
			case AZ_TYPE_COMPLEX_DOUBLE:
				node->value.v.cdouble_v.r = rhs->value.v.cdouble_v.r;
				node->value.v.cdouble_v.i = -rhs->value.v.cdouble_v.i;
				break;
			default:
				assert(0);
				return 1;
		}
		azo_node_clear_children(node);
		opt->n_const_subst += 1;
		return 0;
	}
	assert(0);
	return 1;
}
