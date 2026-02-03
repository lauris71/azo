#ifndef __AZO_BYTECODE_H__
#define __AZO_BYTECODE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_TC_CHECK_ARGS 128

enum {
	NOP = 0,

	/* EXCEPTION MASK(U32) */
	AZO_TC_EXCEPTION,
	AZO_TC_EXCEPTION_IF,
	AZO_TC_EXCEPTION_IF_NOT,
	/*
	 * AZO_TC_EXCEPTION_IF_TYPE_IS_NOT pos type
	 * Throw INVALID_TYPE if element at pos is not type
	 */
	AZO_TC_EXCEPTION_IF_TYPE_IS_NOT,

	/* DEBUG OP(U32) [STRING] */
	AZO_TC_DEBUG,

	/* Stack management */

	/* PUSH_FRAME POS(U32) */
	/* Push [stack_end - pos] as new frame pointer */
	AZO_TC_PUSH_FRAME,
	/* Restores previous frame pointer */
	AZO_TC_POP_FRAME,

	/* POP N(U32) */
	/* Pop and discard last N values from stack */
	AZO_TC_POP,
	/* REMOVE FIRST(U32) N(U32) */
	/* Remove and discard N elements downwards, starting from FIRST from the top of stack */
	AZO_TC_REMOVE,
	/* PUSH_EMPTY TYPE(U32) */
	/* Push default value into stack */
	/* Type has to be >= Boolean */
	AZO_TC_PUSH_EMPTY,
	/* PUSH TYPE(U8) VALUE */
	/* Push immediate primitive value into stack */
	/* Size of value is determined by klass->value_size */
	PUSH_IMMEDIATE,
	/* PUSH_VALUE LOCATION(U32) */
	AZO_TC_PUSH_VALUE,
	/* DUPLICATE POS(U32) */
	/* Pushes a duplicate of an element into stack */
	DUPLICATE,
	/* DUPLICATE_FRAME POS(U32) */
	/* Pushes a duplicate of a frame-relative element into stack */
	DUPLICATE_FRAME,
	/* Exchange POS(U32) */
	/* Exchanges element with topmost */
	EXCHANGE,
	/**
	 * @brief Exchanges frame-relative element with top of stack
	 * 
	 * EXCHANGE_FRAME U32:POS
	 * [..., val1, ..., val2]
	 * [..., val2, ..., val1]
	 */
	AZO_TC_EXCHANGE_FRAME,

	/* Type tests */
	/* TEST POS(U8) */
	/* Test whether the element is exactly of certain type */
	AZO_TC_TYPE_EQUALS,
	/* Test whether the element is of certain type */
	AZO_TC_TYPE_IS,
	/* Test whether the element is supertype of certain type */
	AZO_TC_TYPE_IS_SUPER,
	/* Test whether the element implements certain type */
	AZO_TC_TYPE_IMPLEMENTS,
	/* TEST_IMMEDIATE POS(U8) TYPE(U32) */
	/* Test whether the element is exactly of certain type */
	AZO_TC_TYPE_EQUALS_IMMEDIATE,
	/* Test whether the element is of certain type */
	AZO_TC_TYPE_IS_IMMEDIATE,
	/* Test whether the element is supertype of certain type */
	AZO_TC_TYPE_IS_SUPER_IMMEDIATE,
	/* Test whether the element implements certain type */
	AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE,
	/* TYPE_OF POS(U8) */
	/* Get type of stack element as UINT32 */
	TYPE_OF,
	TYPE_OF_CLASS,

	/* Jumps */

	/* JMP_32... RADDR(I32) */
	/* Jump IP + 5 + RADDR */
	JMP_32,
	JMP_32_IF,
	JMP_32_IF_NOT,
	/* JMP depending on U32 value */
	JMP_32_IF_ZERO,
	JMP_32_IF_POSITIVE,
	JMP_32_IF_NEGATIVE,

	/* Conversions */
	/* POINTER_TO_U64 */
	/* Replaces topmost stack pointer with equivalent U64 */
	POINTER_TO_U64,
	U64_TO_POINTER,
	/* PROMOTE POS(U8) */
	/* Promote given element in-place ty tupe specified by stack(0) */
	/* Only arithmetic types are allowed */
	PROMOTE,

	/* Comparisons */
	/* EQUAL TYPE(U8) */
	/* Allowed types - primitives and block (block subtypes are tested as block) */
	/* Do not remove compared elements */
	EQUAL_TYPED,
	/* EQUAL */
	/* Allowed types - primitives and block (block subtypes are tested as block) */
	/* Do not remove compared elements */
	EQUAL,
	/* COMPARE TYPE(U8) */
	/* Allowed types - integers and reals */
	/* Result is negative if stack(1) < stack(0) */
	COMPARE_TYPED,
	/* COMPARE */
	/* Allowed types - integers and reals, type is determined from stack(0) */
	/* Result is negative if stack(1) < stack(0) */
	COMPARE,

	/* Arithmetic and logic */

	/* Unary */
	/* boolean -> boolean */
	AZO_TC_LOGICAL_NOT,
	AZO_TC_NEGATE,
	AZO_TC_CONJUGATE,
	AZO_TC_BITWISE_NOT,

	/* Binary */
	/* boolean, boolean -> boolean */
	AZO_TC_LOGICAL_AND,
	AZO_TC_LOGICAL_OR,

	/* Allowed types Int32 ... Complex Double */
	/* OPERATION TYPE(U8) */
	AZO_TC_ADD_TYPED,
	AZO_TC_SUBTRACT_TYPED,
	AZO_TC_MULTIPLY_TYPED,
	AZO_TC_DIVIDE_TYPED,
	/* Allowed types Int32 ... Double */
	AZO_TC_MODULO_TYPED,
	/* OPERATION */
	AZO_TC_ADD,
	AZO_TC_SUBTRACT,
	AZO_TC_MULTIPLY,
	AZO_TC_DIVIDE,
	AZO_TC_MODULO,

	/* MIN TYPE(U8) */
	/* Allowed types integers and reals */
	MIN_TYPED,
	MAX_TYPED,

	/* Interface */
	/* GET_INTERFACE_IMMEDIATE TYPE(U32) */
	/* Get interface of topmost element */
	/* Do not pop element to guarantee that interface is alive */
	AZO_TC_GET_INTERFACE_IMMEDIATE,

	/* Invoke function */
	AZO_TC_INVOKE,
	/* Return */
	/* Topmost stack element is return value (if applicable) */
	AZO_TC_RETURN,
	/* Bind function */
	/* FUNCTION -> FUNCTION */
	AZO_TC_BIND,

	/* Arrays */

	/* NEW_ARRAY */
	/* Create new array */
	/* Size of array is the topmost element of stack, replaced by array on completion */
	NEW_ARRAY,
	/* LOAD_ARRAY_ELEMENT */
	/* Array, index - index is popped from stack */
	LOAD_ARRAY_ELEMENT, /* LOAD_ARRAY_ELEMENT, RA, RB, RD */
	/* WRITE_ARRAY_ELEMENT */
	/* Array, index, value - the last two are popped from stack */
	WRITE_ARRAY_ELEMENT,

	/* Key */
	AZO_TC_GET_GLOBAL,
	/* Instance, key -> value|null */
	AZO_TC_GET_PROPERTY,
	/**
	 * @brief Get function by name and argument types
	 * 
	 * GET_FUNCTION U8:N_ARGS
	 * [inst, name, arg1...]
	 * [inst, name, arg1..., func | null]
	 */
	AZO_TC_GET_FUNCTION,
	/* Instance, key, value -> boolean */
	AZO_TC_SET_PROPERTY,
	/* Class, key */
	AZO_TC_GET_STATIC_PROPERTY,
	/**
	 * @brief Get static function by name and argument types
	 * 
	 * GET_STATIC_FUNCTION U8:N_ARGS
	 * [class, name, arg1...]
	 * [class, name, arg1..., func | null]
	 */
	/* Class, String, Arguments -> Class, String, Arguments, Function | null */
	AZO_TC_GET_STATIC_FUNCTION,
	/* Instance, key */
	/* Success: original instance, property instance, index, field */
	/* Failure: original instance, null */
	AZO_TC_LOOKUP_PROPERTY,
	/* fixme: remove this */
	GET_ATTRIBUTE,
	/**
	 * @brief Set value in dictionary
	 * 
	 * SET_ATTRIBUTE
	 * [instance, key, value]
	 * []
	 * 
	 * Throws INVALID_TYPE if instance is not dictionary
	 * Throws INVALID_VALUE if attribute cannot be set
	 */
	AZO_TC_SET_ATTRIBUTE,
};

/* Debug */
typedef struct _AZOProgram AZOProgram;
void print_bytecode (AZOProgram *program);

#ifdef __cplusplus
}
#endif

#endif
