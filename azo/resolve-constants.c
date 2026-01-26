#define __AZO_RESOLVE_CONSTANTS_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <arikkei/arikkei-strlib.h>

#include <az/class.h>
#include <az/complex.h>
#include <az/field.h>
#include <az/string.h>
#include <az/primitives.h>
#include <az/classes/value-array-ref.h>

#include <azo/compiler.h>
#include <azo/frame.h>
#include <azo/keyword.h>
#include <azo/optimizer.h>

static AZOExpression *calculate_logic (AZOExpression *expr);
static AZOExpression *calculate_shift (AZOExpression *expr);
static AZOExpression *calculate_arithmetic (AZOExpression *expr);

static void
substitute (AZOExpression *expr, AZOExpression *old, AZOExpression *parent, AZOExpression *ref)
{
	if (ref) {
		ref->next = expr;
	} else {
		parent->children = expr;
	}
	expr->next = old->next;
	azo_expression_free (old);
}

AZOExpression *
azo_compiler_resolve_binary (AZOExpression *expr)
{
	if ((expr->subtype == ARITHMETIC_ANDAND) || (expr->subtype == ARITHMETIC_OROR)) {
		return calculate_logic (expr);
	} else if ((expr->subtype == ARITHMETIC_SHIFT_LEFT) || (expr->subtype == ARITHMETIC_SHIFT_RIGHT)) {
		return calculate_shift (expr);
	} else {
		return calculate_arithmetic (expr);
	}
}

AZOExpression *
azo_compiler_resolve_prefix (AZOExpression *expr)
{
	AZOExpression *rhs;
	AZValue v;
	unsigned int type;
	rhs = expr->children;
	if (rhs->type != EXPRESSION_CONSTANT) return expr;
	/* Operand is constant expressions */
	if (expr->subtype == PREFIX_NOT) {
		/* Logical not */
		if (rhs->subtype != AZ_TYPE_BOOLEAN) {
			fprintf (stderr, "calculate: rhs is not boolean type (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
			return expr;
		}
		v.boolean_v = !rhs->value.v.boolean_v;
		type = AZ_TYPE_BOOLEAN;
	}
	if (!AZ_TYPE_IS_ARITHMETIC (rhs->subtype)) {
		fprintf (stderr, "calculate_prefix: rhs is not arithmetic type (%d)\n", AZ_PACKED_VALUE_TYPE(&rhs->value));
		return expr;
	}
	if (expr->subtype == PREFIX_PLUS) {
		v = rhs->value.v;
		type = rhs->subtype;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_COMPLEX_DOUBLE) {
		if (expr->subtype == PREFIX_MINUS) {
			v.cdouble_v.r = -rhs->value.v.cdouble_v.r;
			v.cdouble_v.i = -rhs->value.v.cdouble_v.i;
		} else if (expr->subtype == PREFIX_TILDE) {
			v.cdouble_v.r = rhs->value.v.cdouble_v.r;
			v.cdouble_v.i = -rhs->value.v.cdouble_v.i;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for complex double\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_COMPLEX_DOUBLE;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_COMPLEX_FLOAT) {
		if (expr->subtype == PREFIX_MINUS) {
			v.cfloat_v.r = -rhs->value.v.cfloat_v.r;
			v.cfloat_v.i = -rhs->value.v.cfloat_v.i;
		} else if (expr->subtype == PREFIX_TILDE) {
			v.cfloat_v.r = rhs->value.v.cfloat_v.r;
			v.cfloat_v.i = -rhs->value.v.cfloat_v.i;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for complex float\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_COMPLEX_FLOAT;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_DOUBLE) {
		if (expr->subtype == PREFIX_MINUS) {
			v.double_v = -rhs->value.v.double_v;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for double\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_DOUBLE;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_FLOAT) {
		if (expr->subtype == PREFIX_MINUS) {
			v.float_v = -rhs->value.v.float_v;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for float\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_FLOAT;
	} else if (expr->subtype == PREFIX_TILDE) {
		v.int64_v = ~rhs->value.v.int64_v;
		type = rhs->subtype;
	} else if (AZ_TYPE_IS_UNSIGNED (rhs->subtype)) {
		fprintf (stderr, "calculate_prefix: Invalid operator %u for unsigned type\n", expr->subtype);
		return expr;
	} else if (AZ_PACKED_VALUE_TYPE(&rhs->value) == AZ_TYPE_INT64) {
		if (expr->subtype == PREFIX_MINUS) {
			v.int64_v = -rhs->value.v.int64_v;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for int64\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_INT64;
	} else {
		if (expr->subtype == PREFIX_MINUS) {
			v.int32_v = -rhs->value.v.int32_v;
		} else {
			fprintf (stderr, "calculate_prefix: Invalid operator %u for int32\n", expr->subtype);
			return expr;
		}
		type = AZ_TYPE_INT32;
	}
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = type;
	az_packed_value_set_from_type_value (&expr->value, type, &v);
	expr->children = NULL;
	azo_expression_free (rhs);
	return expr;
}

/* Get needed precision if type is converted to floating point */

static unsigned int
get_float_promotion (unsigned int type)
{
	if ((type >= AZ_TYPE_INT32) && (type <= AZ_TYPE_UINT64)) return AZ_TYPE_DOUBLE;
	if ((type == AZ_TYPE_DOUBLE) || (type == AZ_TYPE_COMPLEX_DOUBLE)) return AZ_TYPE_DOUBLE;
	return AZ_TYPE_FLOAT;
}

static AZOExpression *
calculate_logic (AZOExpression *expr)
{
	AZOExpression *lhs, *rhs;
	lhs = expr->children;
	rhs = lhs->next;
	if ((lhs->type != EXPRESSION_CONSTANT) || (rhs->type != EXPRESSION_CONSTANT)) return expr;
	/* Logical binary operators */
	if ((lhs->subtype != AZ_TYPE_BOOLEAN) || (rhs->subtype != AZ_TYPE_BOOLEAN)) {
		fprintf (stderr, "calculate: Invalid types for logical binary operator %u - lhs %u rhs %u\n", expr->subtype, lhs->subtype, rhs->subtype);
		return expr;
	}
	if (expr->subtype == ARITHMETIC_ANDAND) {
		az_packed_value_set_boolean (&expr->value, lhs->value.v.boolean_v && rhs->value.v.boolean_v);
	} else if (expr->subtype == ARITHMETIC_OROR) {
		az_packed_value_set_boolean (&expr->value, lhs->value.v.boolean_v || rhs->value.v.boolean_v);
	}
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = AZ_TYPE_BOOLEAN;
	return expr;
}

static AZOExpression *
calculate_shift (AZOExpression *expr)
{
	AZOExpression *lhs, *rhs;
	AZValue a, b, c;
	unsigned int type;

	lhs = expr->children;
	rhs = lhs->next;
	if ((lhs->type != EXPRESSION_CONSTANT) || (rhs->type != EXPRESSION_CONSTANT)) return expr;

	/* Both operands are constant expressions */
	if (!AZ_TYPE_IS_INTEGRAL (lhs->subtype) || !AZ_TYPE_IS_INTEGRAL (rhs->subtype)) {
		fprintf (stderr, "calculate: Invalid types for shift operator %u - lhs %u rhs %u\n", expr->subtype, lhs->subtype, rhs->subtype);
		return expr;
	}
	if (lhs->subtype == AZ_TYPE_INT64) {
		/* Int64 */
		az_convert_arithmetic_type (AZ_TYPE_INT64, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_SHIFT_LEFT) {
			c.int64_v = a.int64_v << b.int32_v;
		} else if (expr->subtype == ARITHMETIC_SHIFT_RIGHT) {
			c.int64_v = a.int64_v >> b.int64_v;
		} else {
			fprintf (stderr, "calculate: Invalid shift operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_INT64;
	} else if (lhs->subtype == AZ_TYPE_UINT64) {
		/* Uint64 */
		az_convert_arithmetic_type (AZ_TYPE_UINT64, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_SHIFT_LEFT) {
			c.uint64_v = a.uint64_v << b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_SHIFT_RIGHT) {
			c.uint64_v = a.uint64_v >> b.uint64_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for uint64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_UINT64;
	} else if (AZ_TYPE_IS_SIGNED (lhs->subtype)) {
		/* Int32 - any signed type */
		az_convert_arithmetic_type (AZ_TYPE_INT32, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_SHIFT_LEFT) {
			c.int32_v = a.int32_v << b.int32_v;
		} else if (expr->subtype == ARITHMETIC_SHIFT_RIGHT) {
			c.int32_v = a.int32_v >> b.int32_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_INT32;
	} else {
		/* Uint32 - small unsigned types */
		az_convert_arithmetic_type (AZ_TYPE_UINT32, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_SHIFT_LEFT) {
			c.uint32_v = a.uint32_v << b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_SHIFT_RIGHT) {
			c.uint32_v = a.uint32_v >> b.uint32_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_UINT32;
	}
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = type;
	az_packed_value_set_from_type_value (&expr->value, type, &c);
	expr->children = NULL;
	azo_expression_free (lhs);
	azo_expression_free (rhs);
	return expr;
}

#define noDEBUG_CALCULATE_ARITHMETIC

static AZOExpression *
calculate_arithmetic (AZOExpression *expr)
{
	AZOExpression *lhs, *rhs;
	AZValue a, b, c;
	unsigned int type;

	lhs = expr->children;
	rhs = lhs->next;
	if ((lhs->type != EXPRESSION_CONSTANT) || (rhs->type != EXPRESSION_CONSTANT)) return expr;

	/* Both operands are constant expressions */
	if (!AZ_TYPE_IS_ARITHMETIC (lhs->subtype) || !AZ_TYPE_IS_ARITHMETIC (rhs->subtype)) {
		fprintf (stderr, "calculate: Invalid types for arithmetic binary operator %u - lhs %u rhs %u\n", expr->subtype, lhs->subtype, rhs->subtype);
		return expr;
	}
	unsigned int lhs_fp = get_float_promotion (lhs->subtype);
	unsigned int rhs_fp = get_float_promotion (rhs->subtype);
	if ((lhs->subtype == AZ_TYPE_COMPLEX_DOUBLE) || (rhs->subtype == AZ_TYPE_COMPLEX_DOUBLE) ||
		((lhs->subtype == AZ_TYPE_COMPLEX_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs->subtype == AZ_TYPE_COMPLEX_FLOAT))) {
		/* Complex double */
		az_convert_arithmetic_type (AZ_TYPE_COMPLEX_DOUBLE, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_COMPLEX_DOUBLE, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			az_complexd_add (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			az_complexd_sub (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->subtype == ARITHMETIC_STAR) {
			az_complexd_mul (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			az_complexd_div (&c.cdouble_v, &a.cdouble_v, &b.cdouble_v);
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for complex double\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_COMPLEX_DOUBLE;
	} else if ((lhs->subtype == AZ_TYPE_COMPLEX_FLOAT) || (rhs->subtype == AZ_TYPE_COMPLEX_FLOAT)) {
		/* Complex float */
		az_convert_arithmetic_type (AZ_TYPE_COMPLEX_FLOAT, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_COMPLEX_FLOAT, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			az_complexf_add (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			az_complexf_sub (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->subtype == ARITHMETIC_STAR) {
			az_complexf_mul (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			az_complexf_div (&c.cfloat_v, &a.cfloat_v, &b.cfloat_v);
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for complex float\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_COMPLEX_FLOAT;
	} else if ((lhs->subtype == AZ_TYPE_DOUBLE) || (rhs->subtype == AZ_TYPE_DOUBLE) ||
		((lhs->subtype == AZ_TYPE_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs->subtype == AZ_TYPE_FLOAT))) {
		/* Double */
		az_convert_arithmetic_type (AZ_TYPE_DOUBLE, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_DOUBLE, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.double_v = a.double_v + b.double_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.double_v = a.double_v - b.double_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.double_v = a.double_v * b.double_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.double_v = a.double_v / b.double_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.double_v = fmod (a.double_v, b.double_v);
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for double\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_DOUBLE;
	} else if ((lhs->subtype == AZ_TYPE_FLOAT) || (rhs->subtype == AZ_TYPE_FLOAT)) {
		/* Float */
		az_convert_arithmetic_type (AZ_TYPE_FLOAT, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_FLOAT, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.float_v = a.float_v + b.float_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.float_v = a.float_v - b.float_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.float_v = a.float_v * b.float_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.float_v = a.float_v / b.float_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.float_v = fmodf (a.float_v, b.float_v);
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for float\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_DOUBLE;
	} else if ((lhs->subtype == AZ_TYPE_INT64) || (rhs->subtype == AZ_TYPE_INT64) ||
		(((lhs->subtype == AZ_TYPE_UINT64) || (lhs->subtype == AZ_TYPE_UINT32)) && AZ_TYPE_IS_SIGNED (rhs->subtype)) ||
		(AZ_TYPE_IS_SIGNED (lhs->subtype) && ((rhs->subtype == AZ_TYPE_UINT64) || (rhs->subtype == AZ_TYPE_UINT32)))) {
		/* fixme: Do not promote shifts by right operand */
		/* Int64 - int64 or uint64|uint32 and any signed type */
		az_convert_arithmetic_type (AZ_TYPE_INT64, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT64, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.int64_v = a.int64_v + b.int64_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.int64_v = a.int64_v - b.int64_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.int64_v = a.int64_v * b.int64_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.int64_v = a.int64_v / b.int64_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.int64_v = a.int64_v % b.int64_v;
		} else if (expr->subtype == ARITHMETIC_AND) {
			c.int64_v = a.int64_v & b.int64_v;
		} else if (expr->subtype == ARITHMETIC_OR) {
			c.int64_v = a.int64_v | b.int64_v;
		} else if (expr->subtype == ARITHMETIC_CARET) {
			c.int64_v = a.int64_v ^ b.int64_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_INT64;
	} else if ((lhs->subtype == AZ_TYPE_UINT64) || (rhs->subtype == AZ_TYPE_UINT64)) {
		/* Uint64 */
		az_convert_arithmetic_type (AZ_TYPE_UINT64, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_UINT64, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.uint64_v = a.uint64_v + b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.uint64_v = a.uint64_v - b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.uint64_v = a.uint64_v * b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.uint64_v = a.uint64_v / b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.uint64_v = a.uint64_v % b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_AND) {
			c.uint64_v = a.uint64_v & b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_OR) {
			c.uint64_v = a.uint64_v | b.uint64_v;
		} else if (expr->subtype == ARITHMETIC_CARET) {
			c.uint64_v = a.uint64_v ^ b.uint64_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for uint64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_UINT64;
	} else if (AZ_TYPE_IS_SIGNED (lhs->subtype) || AZ_TYPE_IS_SIGNED (rhs->subtype)) {
		/* Int32 - any signed type */
		az_convert_arithmetic_type (AZ_TYPE_INT32, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.int32_v = a.int32_v + b.int32_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.int32_v = a.int32_v - b.int32_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.int32_v = a.int32_v * b.int32_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.int32_v = a.int32_v / b.int32_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.int32_v = a.int32_v % b.int32_v;
		} else if (expr->subtype == ARITHMETIC_AND) {
			c.int32_v = a.int32_v & b.int32_v;
		} else if (expr->subtype == ARITHMETIC_OR) {
			c.int32_v = a.int32_v | b.int32_v;
		} else if (expr->subtype == ARITHMETIC_CARET) {
			c.int64_v = a.int32_v ^ b.int32_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_INT32;
	} else {
		/* Uint32 - small unsigned types */
		az_convert_arithmetic_type (AZ_TYPE_UINT32, &a, lhs->subtype, &lhs->value.v);
		az_convert_arithmetic_type (AZ_TYPE_UINT32, &b, rhs->subtype, &rhs->value.v);
		if (expr->subtype == ARITHMETIC_PLUS) {
			c.uint32_v = a.uint32_v + b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_MINUS) {
			c.uint32_v = a.uint32_v - b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_STAR) {
			c.uint32_v = a.uint32_v * b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_SLASH) {
			c.uint32_v = a.uint32_v / b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_PERCENT) {
			c.uint32_v = a.uint32_v % b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_AND) {
			c.uint32_v = a.uint32_v & b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_OR) {
			c.uint32_v = a.uint32_v | b.uint32_v;
		} else if (expr->subtype == ARITHMETIC_CARET) {
			c.uint32_v = a.uint32_v ^ b.uint32_v;
		} else {
			fprintf (stderr, "calculate: Invalid arithmetic operation for int64\n");
			expr->type = AZO_TERM_INVALID;
			return expr;
		}
		type = AZ_TYPE_UINT32;
	}
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = type;
	az_packed_value_set_from_type_value (&expr->value, type, &c);
	expr->children = NULL;
	azo_expression_free (lhs);
	azo_expression_free (rhs);
#ifdef DEBUG_CALCULATE_ARITHMETIC
	fprintf (stderr, "calculate_arithmetic: Calculated value\n");
#endif
	return expr;
}

AZOExpression *
azo_compiler_resolve_array_literal (AZOExpression *expr)
{
	AZOExpression *child;
	unsigned int size = 0, i;
	for (child = expr->children; child; child = child->next) {
		if (child->type != EXPRESSION_CONSTANT) return expr;
		size += 1;
	}
	AZValueArrayRef *va = az_value_array_ref_new (size);
	i = 0;
	for (child = expr->children; child; child = child->next) {
		az_value_array_ref_set_element (va, i, child->value.impl, &child->value.v);
		i += 1;
	}
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = AZ_TYPE_VALUE_ARRAY_REF;
	az_packed_value_transfer_reference (&expr->value, AZ_TYPE_VALUE_ARRAY_REF, &va->reference);
	while (expr->children) {
		child = expr->children;
		expr->children = child->next;
		azo_expression_free_tree (child);
	}
	return expr;
}
