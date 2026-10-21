#ifndef __AZO_CALCULATE_H__
#define __AZO_CALCULATE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016-2026
 */

#include <az/value.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Arithmetic operator codes - also used as AZO_TERM_ARITHMETIC_* node subtypes */
enum {
	AZO_TERM_ARITHMETIC_PLUS,
	AZO_TERM_ARITHMETIC_MINUS,
	AZO_TERM_ARITHMETIC_SLASH,
	AZO_TERM_ARITHMETIC_STAR,
	AZO_TERM_ARITHMETIC_PERCENT,
	AZO_TERM_ARITHMETIC_SHIFT_LEFT,
	AZO_TERM_ARITHMETIC_SHIFT_RIGHT,
	AZO_TERM_ARITHMETIC_AND,
	AZO_TERM_ARITHMETIC_ANDAND,
	AZO_TERM_ARITHMETIC_OR,
	AZO_TERM_ARITHMETIC_OROR,
	AZO_TERM_ARITHMETIC_CARET
};

/* Arithmetic calculation modes */
enum {
	AZO_CALCULATE_MODE_SATURATE = 0,
	AZO_CALCULATE_MODE_WRAP = 1
};

/* Binary arithmetic result codes */
enum {
	AZO_CALCULATE_OK = 0,
	AZO_CALCULATE_INVALID_TYPE,
	AZO_CALCULATE_DIVIDE_BY_ZERO
};

/* Binary arithmetic result flags */
enum {
	AZO_CALCULATE_FLAG_ROUNDED = 1,
	AZO_CALCULATE_FLAG_OVERFLOW = 2,
	AZO_CALCULATE_FLAG_NON_FINITE = 4
};

/**
 * @brief Calculate the value of a binary arithmetic operation
 * 
 * @param mode the calculation mode (AZO_CALCULATE_MODE_*): saturating clamps
 *   integer overflow to the nearest representable value, wrapping truncates to
 *   the low bits. On overflow the result is still computed (clamped or wrapped)
 *   and the OVERFLOW flag is set in @c result_flags; the caller decides whether
 *   to throw.
 * @param lhs_type the type of the left operand
 * @param lhs the left operand value
 * @param rhs_type the type of the right operand
 * @param rhs the right operand value
 * @param operation the operation code (AZO_TERM_ARITHMETIC_*)
 * @param result_type the location to store the result type
 * @param result the location to store the result value
 * @param result_flags the location to store the result flags (AZO_CALCULATE_FLAG_*)
 * 
 * @return the result code (AZO_CALCULATE_*)
 */
unsigned int azo_calculate_binary (unsigned int mode, unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, unsigned int *result_type, AZValue *result, unsigned int *result_flags);

/**
 * @brief Calculate a binary arithmetic operation on already-promoted operands
 * 
 * Assumes @c lhs and @c rhs are both of type @c type and performs @c operation
 * directly on them, skipping numeric promotion. Used by azo_calculate_binary
 * after promotion and by the typed opcodes (AZO_TC_ADD_TYPED etc.).
 * 
 * @param mode the calculation mode (AZO_CALCULATE_MODE_*)
 * @param type the common type of both operands and the result
 * @param lhs the left operand value
 * @param rhs the right operand value
 * @param operation the operation code (AZO_TERM_ARITHMETIC_*)
 * @param result the location to store the result value
 * @param flags the location to store result flags (AZO_CALCULATE_FLAG_*); only
 *   OVERFLOW and NON_FINITE are set here, existing flags are preserved
 * 
 * @return the result code (AZO_CALCULATE_*)
 */
unsigned int azo_calculate_typed (unsigned int mode, unsigned int type, const AZValue *lhs, const AZValue *rhs, unsigned int operation, AZValue *result, unsigned int *flags);

/**
 * @brief Calculate the value of a binary logical operation
 * 
 * The result type is always AZ_TYPE_BOOLEAN.
 * 
 * @param lhs_type the type of the left operand
 * @param lhs the left operand value
 * @param rhs_type the type of the right operand
 * @param rhs the right operand value
 * @param operation the operation code (AZO_TERM_ARITHMETIC_ANDAND or AZO_TERM_ARITHMETIC_OROR)
 * @param result the location to store the result value
 * 
 * @return the result code (AZO_CALCULATE_*)
 */
unsigned int azo_calculate_logic (unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, AZValue *result);

/**
 * @brief Calculate the value of a binary shift operation
 * 
 * @param lhs_type the type of the left operand
 * @param lhs the left operand value
 * @param rhs_type the type of the right operand
 * @param rhs the right operand value
 * @param operation the operation code (AZO_TERM_ARITHMETIC_SHIFT_LEFT or AZO_TERM_ARITHMETIC_SHIFT_RIGHT)
 * @param result_type the location to store the result type
 * @param result the location to store the result value
 * 
 * @return the result code (AZO_CALCULATE_*)
 */
unsigned int azo_calculate_shift (unsigned int lhs_type, const AZValue *lhs, unsigned int rhs_type, const AZValue *rhs, unsigned int operation, unsigned int *result_type, AZValue *result);

/**
 * @brief Determine the promoted result type of a binary arithmetic operation
 * 
 * Does not compute the value; returns the result type of the operation given
 * the operand types, or AZ_TYPE_NONE if the operation is invalid for the
 * operand types.
 * 
 * @param lhs_type the type of the left operand
 * @param rhs_type the type of the right operand
 * @param operation the operation code (AZO_TERM_ARITHMETIC_*)
 * 
 * @return the result type (AZ_TYPE_*), or AZ_TYPE_NONE
 */
unsigned int azo_calculate_result_type (unsigned int lhs_type, unsigned int rhs_type, unsigned int operation);

#ifdef __cplusplus
}
#endif

#endif
