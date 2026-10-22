#define __AZO_CALCULATE_C__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016-2026
 */

#include <math.h>

#include <az/az.h>
#include <az/complex.h>
#include <az/convert.h>
#include <az/value.h>

#include <azo/calculate.h>

/* Get needed precision if type is converted to floating point */

static unsigned int
get_float_promotion (unsigned int type)
{
	if ((type == AZ_TYPE_INT64) || (type == AZ_TYPE_UINT64) || (type == AZ_TYPE_DOUBLE) || (type == AZ_TYPE_COMPLEX_DOUBLE)) return AZ_TYPE_DOUBLE;
	return AZ_TYPE_FLOAT;
}

/* Integer add/sub/mul with saturating or wrapping overflow handling.
 * Writes the result (clamped under saturate, wrapped under wrap) and returns 1 on overflow. */

static int
calc_add_i32 (unsigned int mode, int32_t x, int32_t y, int32_t *out)
{
	if (__builtin_add_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = (y < 0) ? INT32_MIN : INT32_MAX;
		return 1;
	}
	return 0;
}
static int
calc_sub_i32 (unsigned int mode, int32_t x, int32_t y, int32_t *out)
{
	if (__builtin_sub_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = (y < 0) ? INT32_MAX : INT32_MIN;
		return 1;
	}
	return 0;
}
static int
calc_mul_i32 (unsigned int mode, int32_t x, int32_t y, int32_t *out)
{
	if (__builtin_mul_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = ((x < 0) ^ (y < 0)) ? INT32_MIN : INT32_MAX;
		return 1;
	}
	return 0;
}
static int
calc_add_i64 (unsigned int mode, int64_t x, int64_t y, int64_t *out)
{
	if (__builtin_add_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = (y < 0) ? INT64_MIN : INT64_MAX;
		return 1;
	}
	return 0;
}
static int
calc_sub_i64 (unsigned int mode, int64_t x, int64_t y, int64_t *out)
{
	if (__builtin_sub_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = (y < 0) ? INT64_MAX : INT64_MIN;
		return 1;
	}
	return 0;
}
static int
calc_mul_i64 (unsigned int mode, int64_t x, int64_t y, int64_t *out)
{
	if (__builtin_mul_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = ((x < 0) ^ (y < 0)) ? INT64_MIN : INT64_MAX;
		return 1;
	}
	return 0;
}
static int
calc_add_u32 (unsigned int mode, uint32_t x, uint32_t y, uint32_t *out)
{
	if (__builtin_add_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = UINT32_MAX;
		return 1;
	}
	return 0;
}
static int
calc_sub_u32 (unsigned int mode, uint32_t x, uint32_t y, uint32_t *out)
{
	if (__builtin_sub_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = 0;
		return 1;
	}
	return 0;
}
static int
calc_mul_u32 (unsigned int mode, uint32_t x, uint32_t y, uint32_t *out)
{
	if (__builtin_mul_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = UINT32_MAX;
		return 1;
	}
	return 0;
}
static int
calc_add_u64 (unsigned int mode, uint64_t x, uint64_t y, uint64_t *out)
{
	if (__builtin_add_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = UINT64_MAX;
		return 1;
	}
	return 0;
}
static int
calc_sub_u64 (unsigned int mode, uint64_t x, uint64_t y, uint64_t *out)
{
	if (__builtin_sub_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = 0;
		return 1;
	}
	return 0;
}
static int
calc_mul_u64 (unsigned int mode, uint64_t x, uint64_t y, uint64_t *out)
{
	if (__builtin_mul_overflow (x, y, out)) {
		if (mode == AZO_CALCULATE_MODE_SATURATE) *out = UINT64_MAX;
		return 1;
	}
	return 0;
}

/* Return 1 if the operation on finite operands produced a non-finite result */

static int
fp_result_non_finite (unsigned int type, const AZValue *a, const AZValue *b, const AZValue *c)
{
	int a_finite, b_finite, c_finite;
	switch (type) {
	case AZ_TYPE_FLOAT:
		a_finite = isfinite (a->float_v); b_finite = isfinite (b->float_v);
		c_finite = isfinite (c->float_v);
		break;
	case AZ_TYPE_DOUBLE:
		a_finite = isfinite (a->double_v); b_finite = isfinite (b->double_v);
		c_finite = isfinite (c->double_v);
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		a_finite = az_complexf_isfinite(&a->cfloat_v);
		b_finite = az_complexf_isfinite(&b->cfloat_v);
		c_finite = az_complexf_isfinite(&c->cfloat_v);
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		a_finite = az_complexd_isfinite(&a->cdouble_v);
		b_finite = az_complexd_isfinite(&b->cdouble_v);
		c_finite = az_complexd_isfinite(&c->cdouble_v);
		break;
	default:
		return 0;
	}
	return a_finite && b_finite && !c_finite;
}

/* Convert an operand to the common type according to the calculation mode.
 * Narrowing of any smaller integral or any floating-point value never wraps (it
 * rounds or clamps), so saturation and wrap differ only when a uint64 is
 * narrowed to int64: wrap reinterprets the low bits, saturate clamps. */
static unsigned int
convert_operand (unsigned int mode, unsigned int to_type, AZValue *to_val, unsigned int from_type, const AZValue *from_val)
{
	if ((mode == AZO_CALCULATE_MODE_WRAP) && (from_type == AZ_TYPE_UINT64) && (to_type == AZ_TYPE_INT64)) {
		to_val->int64_v = (int64_t) from_val->uint64_v;
		return (from_val->uint64_v > (uint64_t) INT64_MAX) ? AZ_CONVERSION_CLAMPED : AZ_CONVERSION_EXACT;
	}
	return az_convert_arithmetic_type (to_type, to_val, from_type, from_val);
}

static void
accumulate_conversion_flags (unsigned int conv_a, unsigned int conv_b, unsigned int *flags)
{
	if ((conv_a == AZ_CONVERSION_ROUNDED) || (conv_b == AZ_CONVERSION_ROUNDED)) *flags |= AZO_CALCULATE_FLAG_ROUNDED;
	if ((conv_a == AZ_CONVERSION_CLAMPED) || (conv_b == AZ_CONVERSION_CLAMPED)) *flags |= AZO_CALCULATE_FLAG_MAGNITUDE;
}

unsigned int
azo_calculate_typed (unsigned int mode, unsigned int type, const AZValue *lhs, const AZValue *rhs, unsigned int operation, AZValue *result, unsigned int *flags)
{
	if (!AZ_TYPE_IS_ARITHMETIC (type)) return AZO_CALCULATE_INVALID_TYPE;
	switch (type) {
	case AZ_TYPE_COMPLEX_DOUBLE:
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: az_complexd_add (&result->cdouble_v, &lhs->cdouble_v, &rhs->cdouble_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: az_complexd_sub (&result->cdouble_v, &lhs->cdouble_v, &rhs->cdouble_v); break;
		case AZO_TERM_ARITHMETIC_STAR: az_complexd_mul (&result->cdouble_v, &lhs->cdouble_v, &rhs->cdouble_v); break;
		case AZO_TERM_ARITHMETIC_SLASH: az_complexd_div (&result->cdouble_v, &lhs->cdouble_v, &rhs->cdouble_v); break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (fp_result_non_finite (type, lhs, rhs, result)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
		return AZO_CALCULATE_OK;
	case AZ_TYPE_COMPLEX_FLOAT:
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: az_complexf_add (&result->cfloat_v, &lhs->cfloat_v, &rhs->cfloat_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: az_complexf_sub (&result->cfloat_v, &lhs->cfloat_v, &rhs->cfloat_v); break;
		case AZO_TERM_ARITHMETIC_STAR: az_complexf_mul (&result->cfloat_v, &lhs->cfloat_v, &rhs->cfloat_v); break;
		case AZO_TERM_ARITHMETIC_SLASH: az_complexf_div (&result->cfloat_v, &lhs->cfloat_v, &rhs->cfloat_v); break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (fp_result_non_finite (type, lhs, rhs, result)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
		return AZO_CALCULATE_OK;
	case AZ_TYPE_DOUBLE:
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: result->double_v = lhs->double_v + rhs->double_v; break;
		case AZO_TERM_ARITHMETIC_MINUS: result->double_v = lhs->double_v - rhs->double_v; break;
		case AZO_TERM_ARITHMETIC_STAR: result->double_v = lhs->double_v * rhs->double_v; break;
		case AZO_TERM_ARITHMETIC_SLASH: result->double_v = lhs->double_v / rhs->double_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT: result->double_v = fmod (lhs->double_v, rhs->double_v); break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (fp_result_non_finite (type, lhs, rhs, result)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
		return AZO_CALCULATE_OK;
	case AZ_TYPE_FLOAT:
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: result->float_v = lhs->float_v + rhs->float_v; break;
		case AZO_TERM_ARITHMETIC_MINUS: result->float_v = lhs->float_v - rhs->float_v; break;
		case AZO_TERM_ARITHMETIC_STAR: result->float_v = lhs->float_v * rhs->float_v; break;
		case AZO_TERM_ARITHMETIC_SLASH: result->float_v = lhs->float_v / rhs->float_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT: result->float_v = fmodf (lhs->float_v, rhs->float_v); break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (fp_result_non_finite (type, lhs, rhs, result)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
		return AZO_CALCULATE_OK;
	case AZ_TYPE_INT64: {
		int overflow = 0;
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: overflow = calc_add_i64 (mode, lhs->int64_v, rhs->int64_v, &result->int64_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: overflow = calc_sub_i64 (mode, lhs->int64_v, rhs->int64_v, &result->int64_v); break;
		case AZO_TERM_ARITHMETIC_STAR: overflow = calc_mul_i64 (mode, lhs->int64_v, rhs->int64_v, &result->int64_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (rhs->int64_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->int64_v = lhs->int64_v / rhs->int64_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (rhs->int64_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->int64_v = lhs->int64_v % rhs->int64_v; break;
		case AZO_TERM_ARITHMETIC_AND: result->int64_v = lhs->int64_v & rhs->int64_v; break;
		case AZO_TERM_ARITHMETIC_OR: result->int64_v = lhs->int64_v | rhs->int64_v; break;
		case AZO_TERM_ARITHMETIC_CARET: result->int64_v = lhs->int64_v ^ rhs->int64_v; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (overflow) *flags |= AZO_CALCULATE_FLAG_OVERFLOW;
		return AZO_CALCULATE_OK;
	}
	case AZ_TYPE_UINT64: {
		int overflow = 0;
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: overflow = calc_add_u64 (mode, lhs->uint64_v, rhs->uint64_v, &result->uint64_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: overflow = calc_sub_u64 (mode, lhs->uint64_v, rhs->uint64_v, &result->uint64_v); break;
		case AZO_TERM_ARITHMETIC_STAR: overflow = calc_mul_u64 (mode, lhs->uint64_v, rhs->uint64_v, &result->uint64_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (rhs->uint64_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->uint64_v = lhs->uint64_v / rhs->uint64_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (rhs->uint64_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->uint64_v = lhs->uint64_v % rhs->uint64_v; break;
		case AZO_TERM_ARITHMETIC_AND: result->uint64_v = lhs->uint64_v & rhs->uint64_v; break;
		case AZO_TERM_ARITHMETIC_OR: result->uint64_v = lhs->uint64_v | rhs->uint64_v; break;
		case AZO_TERM_ARITHMETIC_CARET: result->uint64_v = lhs->uint64_v ^ rhs->uint64_v; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (overflow) *flags |= AZO_CALCULATE_FLAG_OVERFLOW;
		return AZO_CALCULATE_OK;
	}
	case AZ_TYPE_INT32: {
		int overflow = 0;
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: overflow = calc_add_i32 (mode, lhs->int32_v, rhs->int32_v, &result->int32_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: overflow = calc_sub_i32 (mode, lhs->int32_v, rhs->int32_v, &result->int32_v); break;
		case AZO_TERM_ARITHMETIC_STAR: overflow = calc_mul_i32 (mode, lhs->int32_v, rhs->int32_v, &result->int32_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (rhs->int32_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->int32_v = lhs->int32_v / rhs->int32_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (rhs->int32_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->int32_v = lhs->int32_v % rhs->int32_v; break;
		case AZO_TERM_ARITHMETIC_AND: result->int32_v = lhs->int32_v & rhs->int32_v; break;
		case AZO_TERM_ARITHMETIC_OR: result->int32_v = lhs->int32_v | rhs->int32_v; break;
		case AZO_TERM_ARITHMETIC_CARET: result->int32_v = lhs->int32_v ^ rhs->int32_v; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (overflow) *flags |= AZO_CALCULATE_FLAG_OVERFLOW;
		return AZO_CALCULATE_OK;
	}
	case AZ_TYPE_UINT32: {
		int overflow = 0;
		switch (operation) {
		case AZO_TERM_ARITHMETIC_PLUS: overflow = calc_add_u32 (mode, lhs->uint32_v, rhs->uint32_v, &result->uint32_v); break;
		case AZO_TERM_ARITHMETIC_MINUS: overflow = calc_sub_u32 (mode, lhs->uint32_v, rhs->uint32_v, &result->uint32_v); break;
		case AZO_TERM_ARITHMETIC_STAR: overflow = calc_mul_u32 (mode, lhs->uint32_v, rhs->uint32_v, &result->uint32_v); break;
		case AZO_TERM_ARITHMETIC_SLASH:
			if (rhs->uint32_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->uint32_v = lhs->uint32_v / rhs->uint32_v; break;
		case AZO_TERM_ARITHMETIC_PERCENT:
			if (rhs->uint32_v == 0) return AZO_CALCULATE_DIVIDE_BY_ZERO;
			result->uint32_v = lhs->uint32_v % rhs->uint32_v; break;
		case AZO_TERM_ARITHMETIC_AND: result->uint32_v = lhs->uint32_v & rhs->uint32_v; break;
		case AZO_TERM_ARITHMETIC_OR: result->uint32_v = lhs->uint32_v | rhs->uint32_v; break;
		case AZO_TERM_ARITHMETIC_CARET: result->uint32_v = lhs->uint32_v ^ rhs->uint32_v; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		if (overflow) *flags |= AZO_CALCULATE_FLAG_OVERFLOW;
		return AZO_CALCULATE_OK;
	}
	default:
		return AZO_CALCULATE_INVALID_TYPE;
	}
}

unsigned int
azo_calculate_negate (unsigned int mode, unsigned int type, const AZValue *val, AZValue *result, unsigned int *flags)
{
	int overflow = 0;
	switch (type) {
	case AZ_TYPE_INT8: {
		int32_t r = -(int32_t) val->int8_v;
		if (r > INT8_MAX) {
			overflow = 1;
			result->int8_v = (mode == AZO_CALCULATE_MODE_SATURATE) ? INT8_MAX : (int8_t) r;
		} else {
			result->int8_v = (int8_t) r;
		}
		break;
	}
	case AZ_TYPE_INT16: {
		int32_t r = -(int32_t) val->int16_v;
		if (r > INT16_MAX) {
			overflow = 1;
			result->int16_v = (mode == AZO_CALCULATE_MODE_SATURATE) ? INT16_MAX : (int16_t) r;
		} else {
			result->int16_v = (int16_t) r;
		}
		break;
	}
	case AZ_TYPE_INT32:
		overflow = calc_sub_i32 (mode, 0, val->int32_v, &result->int32_v);
		break;
	case AZ_TYPE_INT64:
		overflow = calc_sub_i64 (mode, 0, val->int64_v, &result->int64_v);
		break;
	case AZ_TYPE_UINT8:
		if (val->uint8_v) {
			overflow = 1;
			result->uint8_v = (mode == AZO_CALCULATE_MODE_SATURATE) ? 0 : (uint8_t) (0u - (uint32_t) val->uint8_v);
		} else {
			result->uint8_v = 0;
		}
		break;
	case AZ_TYPE_UINT16:
		if (val->uint16_v) {
			overflow = 1;
			result->uint16_v = (mode == AZO_CALCULATE_MODE_SATURATE) ? 0 : (uint16_t) (0u - (uint32_t) val->uint16_v);
		} else {
			result->uint16_v = 0;
		}
		break;
	case AZ_TYPE_UINT32:
		overflow = calc_sub_u32 (mode, 0, val->uint32_v, &result->uint32_v);
		break;
	case AZ_TYPE_UINT64:
		overflow = calc_sub_u64 (mode, 0, val->uint64_v, &result->uint64_v);
		break;
	case AZ_TYPE_FLOAT:
		result->float_v = -val->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		result->double_v = -val->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		result->cfloat_v.r = -val->cfloat_v.r;
		result->cfloat_v.i = -val->cfloat_v.i;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		result->cdouble_v.r = -val->cdouble_v.r;
		result->cdouble_v.i = -val->cdouble_v.i;
		break;
	default:
		return AZO_CALCULATE_INVALID_TYPE;
	}
	if (overflow) *flags |= AZO_CALCULATE_FLAG_OVERFLOW;
	return AZO_CALCULATE_OK;
}

unsigned int
azo_calculate_convert (unsigned int mode, unsigned int to_type, unsigned int from_type, const AZValue *from_val, AZValue *to_val, unsigned int *flags)
{
	unsigned int result;
	if (!AZ_TYPE_IS_ARITHMETIC (to_type) || !AZ_TYPE_IS_ARITHMETIC (from_type)) {
		return AZO_CALCULATE_INVALID_TYPE;
	}
	result = az_convert_arithmetic_type (to_type, to_val, from_type, from_val);
	if ((mode == AZO_CALCULATE_MODE_WRAP) && (result == AZ_CONVERSION_CLAMPED) && AZ_TYPE_IS_INTEGRAL (to_type) && AZ_TYPE_IS_INTEGRAL (from_type)) {
		/* Narrowing integer conversion under wrap: truncate the bit pattern */
		uint64_t u;
		switch (from_type) {
		case AZ_TYPE_INT8: u = (uint64_t) from_val->int8_v; break;
		case AZ_TYPE_UINT8: u = from_val->uint8_v; break;
		case AZ_TYPE_INT16: u = (uint64_t) from_val->int16_v; break;
		case AZ_TYPE_UINT16: u = from_val->uint16_v; break;
		case AZ_TYPE_INT32: u = (uint64_t) from_val->int32_v; break;
		case AZ_TYPE_UINT32: u = from_val->uint32_v; break;
		case AZ_TYPE_INT64: u = (uint64_t) from_val->int64_v; break;
		case AZ_TYPE_UINT64: u = from_val->uint64_v; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		switch (to_type) {
		case AZ_TYPE_INT8: to_val->int8_v = (int8_t) u; break;
		case AZ_TYPE_UINT8: to_val->uint8_v = (uint8_t) u; break;
		case AZ_TYPE_INT16: to_val->int16_v = (int16_t) u; break;
		case AZ_TYPE_UINT16: to_val->uint16_v = (uint16_t) u; break;
		case AZ_TYPE_INT32: to_val->int32_v = (int32_t) u; break;
		case AZ_TYPE_UINT32: to_val->uint32_v = (uint32_t) u; break;
		case AZ_TYPE_INT64: to_val->int64_v = (int64_t) u; break;
		case AZ_TYPE_UINT64: to_val->uint64_v = u; break;
		default: return AZO_CALCULATE_INVALID_TYPE;
		}
		result = AZ_CONVERSION_CLAMPED;
	}
	if (result == AZ_CONVERSION_CLAMPED) {
		*flags |= AZO_CALCULATE_FLAG_MAGNITUDE;
	} else if (result == AZ_CONVERSION_ROUNDED) {
		*flags |= AZO_CALCULATE_FLAG_ROUNDED;
	} else if (result == AZ_CONVERSION_FAILED) {
		return AZO_CALCULATE_INVALID_TYPE;
	}
	/* Detect non-finite results (e.g. double -> float overflow) */
	if ((to_type == AZ_TYPE_FLOAT) && !isfinite (to_val->float_v)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
	else if ((to_type == AZ_TYPE_DOUBLE) && !isfinite (to_val->double_v)) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
	else if ((to_type == AZ_TYPE_COMPLEX_FLOAT) && (!isfinite (to_val->cfloat_v.r) || !isfinite (to_val->cfloat_v.i))) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
	else if ((to_type == AZ_TYPE_COMPLEX_DOUBLE) && (!isfinite (to_val->cdouble_v.r) || !isfinite (to_val->cdouble_v.i))) *flags |= AZO_CALCULATE_FLAG_NON_FINITE;
	return AZO_CALCULATE_OK;
}

unsigned int
azo_calculate_binary (unsigned int mode, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, unsigned int *result_type, AZValue *result, unsigned int *result_flags)
{
	AZValue a, b;
	unsigned int flags = 0;

	unsigned int type = azo_calculate_result_type (lhs_type, rhs_type, operation);
	if (type == AZ_TYPE_NONE) return AZO_CALCULATE_INVALID_TYPE;

	unsigned int conv_a = convert_operand (mode, type, &a, lhs_type, lhs);
	unsigned int conv_b = convert_operand (mode, type, &b, rhs_type, rhs);
	if ((conv_a == AZ_CONVERSION_FAILED) || (conv_b == AZ_CONVERSION_FAILED)) return AZO_CALCULATE_INVALID_TYPE;
	accumulate_conversion_flags (conv_a, conv_b, &flags);

	unsigned int code = azo_calculate_typed (mode, type, &a, &b, operation, result, &flags);
	if (code != AZO_CALCULATE_OK) return code;

	*result_type = type;
	*result_flags = flags;
	return AZO_CALCULATE_OK;
}

unsigned int
azo_calculate_logic (unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, AZValue *result)
{
	if ((lhs_type != AZ_TYPE_BOOLEAN) || (rhs_type != AZ_TYPE_BOOLEAN)) {
		return AZO_CALCULATE_INVALID_TYPE;
	}
	if (operation == AZO_TERM_ARITHMETIC_ANDAND) {
		result->boolean_v = lhs->boolean_v && rhs->boolean_v;
	} else if (operation == AZO_TERM_ARITHMETIC_OROR) {
		result->boolean_v = lhs->boolean_v || rhs->boolean_v;
	} else {
		return AZO_CALCULATE_INVALID_TYPE;
	}
	return AZO_CALCULATE_OK;
}

unsigned int
azo_calculate_shift (unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, unsigned int *result_type, AZValue *result)
{
	AZValue a, b, c;
	unsigned int type;

	if (!AZ_TYPE_IS_INTEGRAL (lhs_type) || !AZ_TYPE_IS_INTEGRAL (rhs_type)) {
		return AZO_CALCULATE_INVALID_TYPE;
	}
	/* The shift count is always read as int32 (it is independent of the value width) */
	az_convert_arithmetic_type (AZ_TYPE_INT32, &b, rhs_type, rhs);
	if (lhs_type == AZ_TYPE_INT64) {
		az_convert_arithmetic_type (AZ_TYPE_INT64, &a, lhs_type, lhs);
		if (operation == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.int64_v = a.int64_v << b.int32_v;
		} else if (operation == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.int64_v = a.int64_v >> b.int32_v;
		} else {
			return AZO_CALCULATE_INVALID_TYPE;
		}
		type = AZ_TYPE_INT64;
	} else if (lhs_type == AZ_TYPE_UINT64) {
		az_convert_arithmetic_type (AZ_TYPE_UINT64, &a, lhs_type, lhs);
		if (operation == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.uint64_v = a.uint64_v << b.int32_v;
		} else if (operation == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.uint64_v = a.uint64_v >> b.int32_v;
		} else {
			return AZO_CALCULATE_INVALID_TYPE;
		}
		type = AZ_TYPE_UINT64;
	} else if (AZ_TYPE_IS_SIGNED (lhs_type)) {
		az_convert_arithmetic_type (AZ_TYPE_INT32, &a, lhs_type, lhs);
		if (operation == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.int32_v = a.int32_v << b.int32_v;
		} else if (operation == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.int32_v = a.int32_v >> b.int32_v;
		} else {
			return AZO_CALCULATE_INVALID_TYPE;
		}
		type = AZ_TYPE_INT32;
	} else {
		az_convert_arithmetic_type (AZ_TYPE_UINT32, &a, lhs_type, lhs);
		if (operation == AZO_TERM_ARITHMETIC_SHIFT_LEFT) {
			c.uint32_v = a.uint32_v << b.int32_v;
		} else if (operation == AZO_TERM_ARITHMETIC_SHIFT_RIGHT) {
			c.uint32_v = a.uint32_v >> b.int32_v;
		} else {
			return AZO_CALCULATE_INVALID_TYPE;
		}
		type = AZ_TYPE_UINT32;
	}
	*result_type = type;
	*result = c;
	return AZO_CALCULATE_OK;
}

unsigned int
azo_calculate_result_type (unsigned int lhs_type, unsigned int rhs_type, unsigned int operation)
{
	unsigned int lhs_fp, rhs_fp;

	if ((operation == AZO_TERM_ARITHMETIC_SHIFT_LEFT) || (operation == AZO_TERM_ARITHMETIC_SHIFT_RIGHT)) {
		/* Shifts require integral operands and yield the (promoted) lhs type */
		if (!AZ_TYPE_IS_INTEGRAL (lhs_type) || !AZ_TYPE_IS_INTEGRAL (rhs_type)) return AZ_TYPE_NONE;
		if (lhs_type == AZ_TYPE_INT64) return AZ_TYPE_INT64;
		if (lhs_type == AZ_TYPE_UINT64) return AZ_TYPE_UINT64;
		if (AZ_TYPE_IS_SIGNED (lhs_type)) return AZ_TYPE_INT32;
		return AZ_TYPE_UINT32;
	}
	if ((operation == AZO_TERM_ARITHMETIC_AND) || (operation == AZO_TERM_ARITHMETIC_OR) || (operation == AZO_TERM_ARITHMETIC_CARET)) {
		/* Bitwise operations require integral operands */
		if (!AZ_TYPE_IS_INTEGRAL (lhs_type) || !AZ_TYPE_IS_INTEGRAL (rhs_type)) return AZ_TYPE_NONE;
	} else {
		if (!AZ_TYPE_IS_ARITHMETIC (lhs_type) || !AZ_TYPE_IS_ARITHMETIC (rhs_type)) return AZ_TYPE_NONE;
	}

	lhs_fp = get_float_promotion (lhs_type);
	rhs_fp = get_float_promotion (rhs_type);
	if ((lhs_type == AZ_TYPE_COMPLEX_DOUBLE) || (rhs_type == AZ_TYPE_COMPLEX_DOUBLE) ||
		((lhs_type == AZ_TYPE_COMPLEX_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs_type == AZ_TYPE_COMPLEX_FLOAT))) {
		return AZ_TYPE_COMPLEX_DOUBLE;
	} else if ((lhs_type == AZ_TYPE_COMPLEX_FLOAT) || (rhs_type == AZ_TYPE_COMPLEX_FLOAT)) {
		return AZ_TYPE_COMPLEX_FLOAT;
	} else if ((lhs_type == AZ_TYPE_DOUBLE) || (rhs_type == AZ_TYPE_DOUBLE) ||
		((lhs_type == AZ_TYPE_FLOAT) && (rhs_fp == AZ_TYPE_DOUBLE)) ||
		((lhs_fp == AZ_TYPE_DOUBLE) && (rhs_type == AZ_TYPE_FLOAT))) {
		return AZ_TYPE_DOUBLE;
	} else if ((lhs_type == AZ_TYPE_FLOAT) || (rhs_type == AZ_TYPE_FLOAT)) {
		return AZ_TYPE_FLOAT;
	} else if ((lhs_type == AZ_TYPE_INT64) || (rhs_type == AZ_TYPE_INT64) ||
		(((lhs_type == AZ_TYPE_UINT64) || (lhs_type == AZ_TYPE_UINT32)) && AZ_TYPE_IS_SIGNED(rhs_type)) ||
		(AZ_TYPE_IS_SIGNED(lhs_type) && ((rhs_type == AZ_TYPE_UINT64) || (rhs_type == AZ_TYPE_UINT32)))) {
		return AZ_TYPE_INT64;
	} else if ((lhs_type == AZ_TYPE_UINT64) || (rhs_type == AZ_TYPE_UINT64)) {
		return AZ_TYPE_UINT64;
	} else if (AZ_TYPE_IS_SIGNED (lhs_type) || AZ_TYPE_IS_SIGNED (rhs_type)) {
		return AZ_TYPE_INT32;
	}
	return AZ_TYPE_UINT32;
}
