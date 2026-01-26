#ifndef __AZO_EXPRESSION_H__
#define __AZO_EXPRESSION_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOExpression AZOExpression;
typedef struct _AZOTerm AZOTerm;

typedef struct _AZOFrame AZOFrame;

#include <stdio.h>

#include <az/packed-value.h>
#include <azo/source.h>
#include <azo/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_EXPRESSION_IS(e,t,st) (((e)->term.type == (t)) && ((e)->term.subtype == (st)))

/* Expression types */
enum {
	/* Special */
	AZO_TERM_INVALID,
	AZO_TERM_EMPTY,

	/* Program */
	AZO_EXPRESSION_PROGRAM,
	/* Block */
	AZO_EXPRESSION_BLOCK,
	/* Keywords */
	EXPRESSION_KEYWORD,
	/* TYPE DECLARATION[...]*/
	EXPRESSION_DECLARATION_LIST,
	/* NAME [= VALUE] */
	EXPRESSION_DECLARATION,
	/* TYPE NAME */
	EXPRESSION_ARGUMENT_DECLARATION,
	/* [REFERENCE.]function [REFERENCE] (LIST) STATEMENT */
	EXPRESSION_FUNCTION,
	/* REFERENCE, LIST */
	EXPRESSION_FUNCTION_CALL,
	EXPRESSION_ARRAY_ELEMENT,
	/* List of expressions (for example function arguments) */
	EXPRESSION_LIST,
	/* Variable reference */
	EXPRESSION_REFERENCE,
	/* Literal array */
	EXPRESSION_LITERAL_ARRAY,

	/* Operators */
	EXPRESSION_SUFFIX,
	EXPRESSION_PREFIX,
	EXPRESSION_BINARY,
	EXPRESSION_COMPARISON,
	EXPRESSION_ASSIGN,
	EXPRESSION_COMMA,
	AZO_EXPRESSION_TEST,

	/* Resolved expressions */
	/* Subtype is value type (or 0), value is set */
	EXPRESSION_CONSTANT,
	EXPRESSION_VARIABLE,
	/* Subtype is variable type */
	EXPRESSION_TYPE,

	NUM_EXPRESSIONS
};

/* Unspecified subtype */
#define EXPRESSION_GENERIC 0

/* Reference subtypes */
enum {
	/* Simple variable name */
	REFERENCE_VARIABLE,
	/* Member object (dot operator) */
	REFERENCE_MEMBER,
	/* Second component of member */
	REFERENCE_PROPERTY
};

/* Variable subtypes */
enum {
	VARIABLE_PARENT,
	VARIABLE_LOCAL
};

/* Function subtypes */
enum {
	FUNCTION_STATIC,
	FUNCTION_MEMBER
};

/* Suffix subtypes */
enum {
	SUFFIX_INCREMENT,
	SUFFIX_DECREMENT
};

/* Prefix subtypes */
enum {
	PREFIX_INCREMENT,
	PREFIX_DECREMENT,
	PREFIX_PLUS,
	PREFIX_MINUS,
	PREFIX_NOT,
	PREFIX_TILDE
};

/* Arithmetic subtypes */
enum {
	ARITHMETIC_PLUS,
	ARITHMETIC_MINUS,
	ARITHMETIC_SLASH,
	ARITHMETIC_STAR,
	ARITHMETIC_PERCENT,
	ARITHMETIC_SHIFT_LEFT,
	ARITHMETIC_SHIFT_RIGHT,
	ARITHMETIC_AND,
	ARITHMETIC_ANDAND,
	ARITHMETIC_OR,
	ARITHMETIC_OROR,
	ARITHMETIC_CARET
};

/* Comparison subtypes */
enum {
	COMPARISON_E,
	COMPARISON_NE,
	COMPARISON_LT,
	COMPARISON_LE,
	COMPARISON_GT,
	COMPARISON_GE
};

/* Assign subtypes (priority 16) */
enum {
	ASSIGN,
	ASSIGN_PLUS,
	ASSIGN_MINUS,
	ASSIGN_STAR,
	ASSIGN_SLASH,
	ASSIGN_PERCENT,
	ASSIGN_SHIFT_LEFT,
	ASSIGN_SHIFT_RIGHT,
	ASSIGN_AND,
	ASSIGN_OR,
	ASSIGN_XOR
};

enum {
	AZO_TYPE_IS,
	AZO_TYPE_IMPLEMENTS
};

struct _AZOTerm {
	/**
	 * @brief Term main type
	 * 
	 */
	uint32_t type;
	/**
	 * @brief Term subtype
	 * 
	 */
	uint32_t subtype;

	/**
	 * @brief Term start in source code
	 * 
	 */
	unsigned int start;
	/**
	 * @brief Term end in source code
	 * 
	 */
	unsigned int end;
};

struct _AZOExpression {
	/* Tree implementation */
	AZOExpression *parent;
	AZOExpression *next;
	AZOExpression *children;

	AZOTerm term;
#if 0
	unsigned int type;
	unsigned int subtype;

	unsigned int start;
	unsigned int end;
#endif

	/* Need to align 16 bytes anyways */
	union {
		/* Function frame */
		AZOFrame *frame;
		/* Variable location */
		unsigned int var_pos;
		/* Size of scope */
		unsigned int scope_size;
	};

	/* Optimizer */
	AZPackedValue value;
};

AZOExpression *azo_expression_new (unsigned int type, unsigned int subtype, unsigned int start, unsigned int end);
void azo_expression_free (AZOExpression *expr);
void azo_expression_free_tree (AZOExpression *expr);
AZOExpression *azo_expression_clone_tree (AZOExpression *expr);

AZOExpression *azo_expression_new_number (const AZOSource *src, const AZOToken *token);
AZOExpression *azo_expression_new_integer (const AZOSource *src, const AZOToken *token);
AZOExpression *azo_expression_new_real (const AZOSource *src, const AZOToken *token);
AZOExpression *azo_expression_new_complex (const AZOSource *src, const AZOToken *token);
AZOExpression *azo_expression_new_text (const AZOSource *src, const AZOToken *token);
AZOExpression *azo_expression_new_reference (unsigned int subtype, const AZOSource *src, const AZOToken *token);

void azo_print_expression_list (AZOExpression *expr, FILE *ofs, const char *separator);

#ifdef __cplusplus
}
#endif

#endif
