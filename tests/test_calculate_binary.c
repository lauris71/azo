#define __AZO_TEST_CALCULATE_BINARY_C__

/*
 * Unit tests for azo_calculate_binary - the low-level arithmetic engine.
 *
 * Unlike tests/test_calculate.c (which drives the full language pipeline), these
 * exercise azo_calculate_binary directly, checking the result value, the promoted
 * result type and the overflow/rounded/non-finite flags for corner cases.
 */

#include <stdint.h>
#include <stdio.h>

#include <az/az.h>
#include <azo/calculate.h>

#include "unity/unity.h"
#include "test.h"

#define ASSERT_NO_FLAGS(f) TEST_ASSERT_EQUAL_UINT32(0u, (f))
#define ASSERT_HAS_OVERFLOW(f) TEST_ASSERT_TRUE(((f) & AZO_CALCULATE_FLAG_OVERFLOW) != 0)
#define ASSERT_HAS_ROUNDED(f) TEST_ASSERT_TRUE(((f) & AZO_CALCULATE_FLAG_ROUNDED) != 0)
#define ASSERT_HAS_NON_FINITE(f) TEST_ASSERT_TRUE(((f) & AZO_CALCULATE_FLAG_NON_FINITE) != 0)

/* Run azo_calculate_binary and return the result code */
static unsigned int
calc(unsigned int mode, unsigned int lt, const AZValue *lhs, unsigned int rt, const AZValue *rhs,
     unsigned int op, unsigned int *rtype, AZValue *result, unsigned int *flags)
{
	return azo_calculate_binary(mode, lt, lhs, rt, rhs, op, rtype, result, flags);
}

void
test_calculate_binary(void)
{
	az_init();

	AZValue lhs, rhs, result;
	unsigned int rtype, flags, code;

	/* Exact int32 arithmetic */
	{
		lhs.int32_v = 10; rhs.int32_v = 5;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_INT32, rtype);
		TEST_ASSERT_EQUAL_INT(15, result.int32_v);
		ASSERT_NO_FLAGS(flags);

		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_MINUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(5, result.int32_v);
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_STAR, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(50, result.int32_v);
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_SLASH, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(2, result.int32_v);

		/* Truncation toward zero: 10 % -3 == 1, -10 % 3 == -1 */
		lhs.int32_v = 10; rhs.int32_v = -3;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PERCENT, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(1, result.int32_v);
		lhs.int32_v = -10; rhs.int32_v = 3;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PERCENT, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(-1, result.int32_v);
	}

	/* Bitwise operations on int32 */
	{
		lhs.int32_v = 12; rhs.int32_v = 10;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_AND, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(8, result.int32_v);
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_OR, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(14, result.int32_v);
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_CARET, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(6, result.int32_v);
	}

	/* Bitwise operations on float are rejected */
	{
		lhs.float_v = 2.0f; rhs.float_v = 1.0f;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_FLOAT, &lhs, AZ_TYPE_FLOAT, &rhs, AZO_TERM_ARITHMETIC_AND, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_INVALID_TYPE, code);
	}

	/* Promotion rules */
	{
		/* int8 + int8 -> int32 */
		lhs.int8_v = 1; rhs.int8_v = 2;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT8, &lhs, AZ_TYPE_INT8, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_INT32, rtype);
		TEST_ASSERT_EQUAL_INT(3, result.int32_v);

		/* uint8 + uint8 -> uint32 */
		lhs.uint8_v = 200; rhs.uint8_v = 200;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_UINT8, &lhs, AZ_TYPE_UINT8, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_UINT32, rtype);
		TEST_ASSERT_EQUAL_UINT32(400, result.uint32_v);

		/* int32 + int64 -> int64 */
		lhs.int32_v = 1; rhs.int64_v = 5000000000LL;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT64, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_INT64, rtype);
		TEST_ASSERT_EQUAL_INT64(5000000001LL, result.int64_v);

		/* int32 + float -> float */
		lhs.int32_v = 2; rhs.float_v = 0.5f;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_FLOAT, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_FLOAT, rtype);
		TEST_ASSERT_EQUAL_FLOAT(2.5f, result.float_v);

		/* float + double -> double */
		lhs.float_v = 0.5f; rhs.double_v = 1.0;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_FLOAT, &lhs, AZ_TYPE_DOUBLE, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_DOUBLE, rtype);
		TEST_ASSERT_EQUAL_DOUBLE(1.5, result.double_v);
	}

	/* int32 overflow: saturate */
	{
		lhs.int32_v = INT32_MAX; rhs.int32_v = 1;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_INT(INT32_MAX, result.int32_v);
		ASSERT_HAS_OVERFLOW(flags);

		/* INT32_MIN - 1 clamps to INT32_MIN */
		lhs.int32_v = INT32_MIN; rhs.int32_v = 1;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_MINUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(INT32_MIN, result.int32_v);
		ASSERT_HAS_OVERFLOW(flags);
	}

	/* int32 overflow: wrap */
	{
		lhs.int32_v = INT32_MAX; rhs.int32_v = 1;
		code = calc(AZO_CALCULATE_MODE_WRAP, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_INT(INT32_MIN, result.int32_v);
		ASSERT_HAS_OVERFLOW(flags);

		/* INT32_MIN - 1 wraps to INT32_MAX */
		lhs.int32_v = INT32_MIN; rhs.int32_v = 1;
		calc(AZO_CALCULATE_MODE_WRAP, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_MINUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT(INT32_MAX, result.int32_v);
		ASSERT_HAS_OVERFLOW(flags);
	}

	/* int64 overflow: saturate vs wrap */
	{
		lhs.int64_v = INT64_MAX; rhs.int64_v = 1;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT64, &lhs, AZ_TYPE_INT64, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT64(INT64_MAX, result.int64_v);
		ASSERT_HAS_OVERFLOW(flags);

		calc(AZO_CALCULATE_MODE_WRAP, AZ_TYPE_INT64, &lhs, AZ_TYPE_INT64, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_INT64(INT64_MIN, result.int64_v);
		ASSERT_HAS_OVERFLOW(flags);
	}

	/* uint64 -> int64 narrowing: saturate clamps, wrap reinterprets low bits */
	{
		lhs.uint64_v = UINT64_MAX; rhs.int64_v = 0;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_UINT64, &lhs, AZ_TYPE_INT64, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_INT64, rtype);
		TEST_ASSERT_EQUAL_INT64(INT64_MAX, result.int64_v);
		ASSERT_HAS_OVERFLOW(flags);

		code = calc(AZO_CALCULATE_MODE_WRAP, AZ_TYPE_UINT64, &lhs, AZ_TYPE_INT64, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_INT64(-1, result.int64_v);
		ASSERT_HAS_OVERFLOW(flags);
	}

	/* Division by zero reports DIVIDE_BY_ZERO (integer) */
	{
		lhs.int32_v = 10; rhs.int32_v = 0;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_SLASH, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_DIVIDE_BY_ZERO, code);

		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_INT32, &rhs, AZO_TERM_ARITHMETIC_PERCENT, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_DIVIDE_BY_ZERO, code);
	}

	/* Floating-point non-finite results are flagged, not thrown */
	{
		lhs.double_v = 1.0; rhs.double_v = 0.0;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_DOUBLE, &lhs, AZ_TYPE_DOUBLE, &rhs, AZO_TERM_ARITHMETIC_SLASH, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		ASSERT_HAS_NON_FINITE(flags);

		/* 0.0 / 0.0 -> NaN */
		lhs.double_v = 0.0; rhs.double_v = 0.0;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_DOUBLE, &lhs, AZ_TYPE_DOUBLE, &rhs, AZO_TERM_ARITHMETIC_SLASH, &rtype, &result, &flags);
		TEST_ASSERT_TRUE(result.double_v != result.double_v); /* NaN */
		ASSERT_HAS_NON_FINITE(flags);
	}

	/* int -> float conversion that rounds is flagged ROUNDED */
	{
		lhs.int32_v = 16777217; /* 2^24 + 1, not representable in float32 */
		rhs.float_v = 0.0f;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_INT32, &lhs, AZ_TYPE_FLOAT, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_FLOAT, rtype);
		TEST_ASSERT_EQUAL_FLOAT(16777216.0f, result.float_v);
		ASSERT_HAS_ROUNDED(flags);
	}

	/* Complex double arithmetic */
	{
		lhs.cdouble_v.r = 1.0; lhs.cdouble_v.i = 2.0;
		rhs.cdouble_v.r = 3.0; rhs.cdouble_v.i = 4.0;
		code = calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_COMPLEX_DOUBLE, &lhs, AZ_TYPE_COMPLEX_DOUBLE, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZO_CALCULATE_OK, code);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_COMPLEX_DOUBLE, rtype);
		TEST_ASSERT_EQUAL_DOUBLE(4.0, result.cdouble_v.r);
		TEST_ASSERT_EQUAL_DOUBLE(6.0, result.cdouble_v.i);
		ASSERT_NO_FLAGS(flags);

		/* complex + double promotes to complex double */
		lhs.cdouble_v.r = 1.0; lhs.cdouble_v.i = 2.0; rhs.double_v = 1.0;
		calc(AZO_CALCULATE_MODE_SATURATE, AZ_TYPE_COMPLEX_DOUBLE, &lhs, AZ_TYPE_DOUBLE, &rhs, AZO_TERM_ARITHMETIC_PLUS, &rtype, &result, &flags);
		TEST_ASSERT_EQUAL_UINT32(AZ_TYPE_COMPLEX_DOUBLE, rtype);
		TEST_ASSERT_EQUAL_DOUBLE(2.0, result.cdouble_v.r);
		TEST_ASSERT_EQUAL_DOUBLE(2.0, result.cdouble_v.i);
	}
}