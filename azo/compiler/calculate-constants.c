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

#include <azo/calculate.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>
#include <azo/keyword.h>
#include <azo/node.h>
#include <azo/source.h>

static int calculate_logic (AZOOptimizer *opt, AZONode *expr, AZOSource *src);
static int calculate_shift (AZOOptimizer *opt, AZONode *expr, AZOSource *src);
static int calculate_arithmetic (AZOOptimizer *opt, AZOCompilerContext *ctx, AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, AZOSource *src);

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
 * Whether a non-finite floating-point result is NaN (as opposed to infinite)
 */

static int
fp_result_is_nan (unsigned int type, const AZValue *c)
{
	switch (type) {
	case AZ_TYPE_FLOAT:
		return isnan (c->float_v);
	case AZ_TYPE_DOUBLE:
		return isnan (c->double_v);
	case AZ_TYPE_COMPLEX_FLOAT:
		return isnan (c->cfloat_v.r) || isnan (c->cfloat_v.i);
	case AZ_TYPE_COMPLEX_DOUBLE:
		return isnan (c->cdouble_v.r) || isnan (c->cdouble_v.i);
	default:
		return 0;
	}
}

static void
node_replace_with_constant(AZONode *node, unsigned int type, const AZValue *val)
{
	node->term.type = AZO_TERM_CONSTANT;
	node->type = type;
	az_packed_value_set_from_type_value(&node->value, type, val);
	/* Children mus be cleared after the value is set in case the value refers into the subtree */
	azo_node_clear_children(node);
}

static int
calculate_logic (AZOOptimizer *opt, AZONode *expr, AZOSource *src)
{
	AZONode *lhs = expr->children;
	AZONode *rhs = lhs->next;
	assert((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT));
	AZValue result;
	unsigned int result_code = azo_calculate_logic (lhs->type, &lhs->value.v, rhs->type, &rhs->value.v, expr->term.subtype, &result);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) {
		calculate_report ("ERROR", expr, lhs->type, &lhs->value.v, rhs->type, &rhs->value.v, "invalid type", src);
		return 1;
	}
	node_replace_with_constant(expr, AZ_TYPE_BOOLEAN, &result);
	opt->n_const_subst += 1;
	return 0;
}

static int
calculate_shift (AZOOptimizer *opt, AZONode *expr, AZOSource *src)
{
	AZValue c;
	unsigned int type;

	AZONode *lhs = expr->children;
	AZONode *rhs = lhs->next;
	assert((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT));

	unsigned int result_code = azo_calculate_shift (lhs->type, &lhs->value.v, rhs->type, &rhs->value.v, expr->term.subtype, &type, &c);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) {
		calculate_report ("ERROR", expr, lhs->type, &lhs->value.v, rhs->type, &rhs->value.v, "invalid type", src);
		return 1;
	}
	node_replace_with_constant(expr, type, &c);
	opt->n_const_subst += 1;
	return 0;
}

/* #define DEBUG_CALCULATE_ARITHMETIC */

static int
calculate_arithmetic (AZOOptimizer *opt, AZOCompilerContext *ctx, AZONode *expr, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, AZOSource *src)
{
	AZValue c;
	unsigned int type;
	unsigned int flags;
	unsigned int result_code = azo_calculate_binary (ctx->calc_mode, lhs_type, lhs, rhs_type, rhs, expr->term.subtype, &type, &c, &flags);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) {
		calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "invalid type", src);
		return 1;
	}
	if (result_code == AZO_CALCULATE_DIVIDE_BY_ZERO) {
		calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer division by zero", src);
		return 1;
	}
	if (flags & AZO_CALCULATE_FLAG_NON_FINITE) {
		const char *msg = fp_result_is_nan (type, &c) ? "result is NaN" : "result is infinite (overflow)";
		if (ctx->exception_mask & AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_NOT_FINITE)) {
			calculate_report ("WARNING", expr, lhs_type, lhs, rhs_type, rhs, msg, src);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, msg, src);
			return 1;
		}
	}
	if (flags & AZO_CALCULATE_FLAG_MAGNITUDE) {
		if (ctx->exception_mask & AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_CHANGE_OF_MAGNITUDE)) {
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, AZ_CONVERSION_CLAMPED, src);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "value clamping", src);
			return 1;
		}
	}
	if (flags & AZO_CALCULATE_FLAG_OVERFLOW) {
		if (ctx->exception_mask & AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_INTEGER_OVERFLOW)) {
			calculate_report ("WARNING", expr, lhs_type, lhs, rhs_type, rhs, "integer overflow", src);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "integer overflow", src);
			return 1;
		}
	}
	if (flags & AZO_CALCULATE_FLAG_ROUNDED) {
		if (ctx->exception_mask & AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_CHANGE_OF_PRECISION)) {
			calculate_warn_conversion (expr, lhs_type, lhs, rhs_type, rhs, type, &c, AZ_CONVERSION_ROUNDED, src);
		} else {
			calculate_report ("ERROR", expr, lhs_type, lhs, rhs_type, rhs, "conversion changes precision", src);
			return 1;
		}
	}
	/* The children are freed here, after all operand reads */
	node_replace_with_constant(expr, type, &c);
	opt->n_const_subst += 1;
	return 0;
}

int
azo_compiler_calculate_constant_binary (AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node)
{
	AZONode *lhs = node->children;
	AZONode *rhs = lhs->next;
	assert((lhs->term.type == AZO_TERM_CONSTANT) && (rhs->term.type == AZO_TERM_CONSTANT));
	if ((node->term.subtype == AZO_TERM_ARITHMETIC_ANDAND) || (node->term.subtype == AZO_TERM_ARITHMETIC_OROR)) {
		return calculate_logic (opt, node, opt->comp->src);
	} else if ((node->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_LEFT) || (node->term.subtype == AZO_TERM_ARITHMETIC_SHIFT_RIGHT)) {
		return calculate_shift (opt, node, opt->comp->src);
	} else {
		unsigned int lhs_type = AZ_IMPL_TYPE(lhs->value.impl);
		unsigned int rhs_type = AZ_IMPL_TYPE(rhs->value.impl);
		return calculate_arithmetic (opt, ctx, node, lhs_type, &lhs->value.v, rhs_type, &rhs->value.v, opt->comp->src);
	}
	return 0;
}

int
azo_compiler_calculate_rvalue_prefix(AZOOptimizer *opt, AZOOptimizerCtx *ctx, AZONode *node)
{
	AZValue result;
	AZONode *rhs = node->children;
	if (rhs->term.type != AZO_TERM_CONSTANT) return 0;
	/* Operand is constant expressions */
	if (node->term.subtype == AZO_TERM_PREFIX_NOT) {
		if (rhs->type != AZ_TYPE_BOOLEAN) {
			fprintf (stderr, "calculate_rvalue_prefix: expression is not boolean (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
			return 1;
		}
		result.boolean_v = !rhs->value.v.boolean_v;
		node_replace_with_constant(node, AZ_TYPE_BOOLEAN, &result);
		opt->n_const_subst += 1;
		return 0;
	}
	/* '+' '-' '~' require arithmetic types */
	if (!AZ_TYPE_IS_ARITHMETIC (rhs->type)) {
		fprintf (stderr, "calculate_rvalue_prefix: rhs is not arithmetic type (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
		return 1;
	}
	/* '+' is no-op */
	if (node->term.subtype == AZO_TERM_PREFIX_PLUS) {
		node_replace_with_constant(node, rhs->type, &rhs->value.v);
		opt->n_const_subst += 1;
		return 0;
	}
	/* '-' negates in place; overflow (unsigned, or most negative signed) follows the mode */
	if (node->term.subtype == AZO_TERM_PREFIX_MINUS) {
		unsigned int flags = 0;
		unsigned int result_code = azo_calculate_negate (ctx->calc_mode, rhs->type, &rhs->value.v, &result, &flags);
		if (result_code != AZO_CALCULATE_OK) {
			fprintf (stderr, "calculate_rvalue_prefix: invalid type for negation (%d)\n", rhs->type);
			return 1;
		}
		if (flags & AZO_CALCULATE_FLAG_OVERFLOW) {
			if (ctx->exception_mask & AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_INTEGER_OVERFLOW)) {
				fprintf (stderr, "WARNING: unary minus overflows (value clamped/wrapped)\n");
			} else {
				fprintf (stderr, "ERROR: unary minus overflows\n");
				return 1;
			}
		}
		node_replace_with_constant(node, rhs->type, &result);
		opt->n_const_subst += 1;
		return 0;
	}
	if (node->term.subtype == AZO_TERM_PREFIX_TILDE) {
		if ((rhs->type == AZ_TYPE_FLOAT) || (rhs->type == AZ_TYPE_DOUBLE)) {
			fprintf (stderr, "calculate_rvalue_prefix: Invalid operator %u for float/double\n", node->term.subtype);
			return 1;
		}
		switch (rhs->type) {
		case AZ_TYPE_INT8:
		case AZ_TYPE_UINT8:
			result.uint8_v = ~rhs->value.v.uint8_v;
			break;
		case AZ_TYPE_INT16:
		case AZ_TYPE_UINT16:
			result.uint16_v = ~rhs->value.v.uint16_v;
			break;
		case AZ_TYPE_INT32:
		case AZ_TYPE_UINT32:
			result.uint32_v = ~rhs->value.v.uint32_v;
			break;
		case AZ_TYPE_INT64:
		case AZ_TYPE_UINT64:
			result.uint64_v = ~rhs->value.v.uint64_v;
			break;
		case AZ_TYPE_COMPLEX_FLOAT:
			result.cfloat_v.r = rhs->value.v.cfloat_v.r;
			result.cfloat_v.i = -rhs->value.v.cfloat_v.i;
			break;
		case AZ_TYPE_COMPLEX_DOUBLE:
			result.cdouble_v.r = rhs->value.v.cdouble_v.r;
			result.cdouble_v.i = -rhs->value.v.cdouble_v.i;
			break;
		default:
			assert(0);
			return 1;
	}
		node_replace_with_constant(node, rhs->type, &result);
		opt->n_const_subst += 1;
		return 0;
	}
	assert(0);
	return 1;
}
