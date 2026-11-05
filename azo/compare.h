#ifndef __AZO_COMPARE_H__
#define __AZO_COMPARE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016-2018
 */

#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Comparison result codes */
enum {
	/* The operands were comparable, the boolean result is stored in result */
	AZO_COMPARE_OK = 0,
	/* The operands are of the same family but different types (e.g. two different
	 * value types, or different arithmetic types under identity comparison):
	 * the result (false) is stored, callers may treat it as an error */
	AZO_COMPARE_DIFFERENT_TYPE,
	/* The operands are from incomparable families (e.g. boolean vs. number):
	 * the result (false) is stored, callers may treat it as an error */
	AZO_COMPARE_INVALID_TYPE
};

/**
 * @brief Compare two values for equality (==)
 *
 * The comparison rules:
 * - null == null: all nulls (untyped, NULL pointer, NULL block) form one equality class
 * - null == non-null: false (a valid comparison)
 * - booleans, pointers: trivial value equality
 * - arithmetic: exact mathematical value equality across the family (no precision
 *   loss for int64/float mixes, complex with zero imaginary part equals its real part,
 *   NaN == NaN is true for now)
 * - block types (interfaces included - an interface is a block living inside another
 *   instance): pointer equality (strings are hash-collated so this is value equality)
 * - boxed values and boxed interfaces (storage containers, never typed NULL themselves)
 *   are unboxed transparently: boxed X == X compares the content/instance pointer
 * - non-primitive value types: byte equality, requires the same type
 * - cross-family comparisons (boolean/number, pointer/int etc.): forbidden, result
 *   is false and AZO_COMPARE_INVALID_TYPE is returned
 *
 * The boolean result is always stored to result (as boolean_v), also when the
 * returned code is not AZO_COMPARE_OK.
 *
 * @param lhs_impl the implementation of the left operand (NULL for untyped null)
 * @param lhs the left operand value
 * @param rhs_impl the implementation of the right operand (NULL for untyped null)
 * @param rhs the right operand value
 * @param result the location to store the boolean result
 *
 * @return the result code (AZO_COMPARE_*)
 */
unsigned int azo_compare_equal (const AZImplementation *lhs_impl, const AZValue *lhs, const AZImplementation *rhs_impl, const AZValue *rhs, AZValue *result);

/**
 * @brief Compare two values for identity (===)
 *
 * Strict implementation + value equality:
 * - operands with different implementations are never identical: for non-interfaces
 *   these are different classes, for interfaces implementations of different classes
 *   (AZO_COMPARE_DIFFERENT_TYPE is returned, result is false)
 * - null === null requires matching implementations (untyped === untyped, String null
 *   === String null)
 * - null === non-null: false (a valid comparison, AZO_COMPARE_OK)
 * - arithmetic values are compared bitwise: -0.0 !== +0.0, NaN === NaN (same bit pattern)
 * - boxed values and boxed interfaces are unboxed transparently before comparing
 * - booleans, pointers, blocks, value types: as azo_compare_equal
 *
 * The boolean result is always stored to result (as boolean_v), also when the
 * returned code is not AZO_COMPARE_OK.
 *
 * @param lhs_impl the implementation of the left operand (NULL for untyped null)
 * @param lhs the left operand value
 * @param rhs_impl the implementation of the right operand (NULL for untyped null)
 * @param rhs the right operand value
 * @param result the location to store the boolean result
 *
 * @return the result code (AZO_COMPARE_*)
 */
unsigned int azo_compare_identical (const AZImplementation *lhs_impl, const AZValue *lhs, const AZImplementation *rhs_impl, const AZValue *rhs, AZValue *result);

unsigned int azo_compiler_compile_comparison (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *lhs, const AZONode *rhs, const AZONode *expr, AZOSource *src, unsigned int reg);

#ifdef __cplusplus
}
#endif

#endif
