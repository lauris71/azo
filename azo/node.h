#ifndef __AZO_NODE_H__
#define __AZO_NODE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZONode AZONode;
typedef struct _AZOTerm AZOTerm;

typedef struct _AZOFrame AZOFrame;

#include <stdio.h>

#include <az/packed-value.h>
#include <azo/source.h>
#include <azo/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_NODE_IS(e,t,st) (((e)->term.type == (t)) && ((e)->term.subtype == (st)))

/* Term types */
enum {
	/* Special */
	AZO_TERM_INVALID,
	AZO_TERM_EMPTY,

	/* Program */
	AZO_TERM_PROGRAM,
	/* Block */
	AZO_TERM_BLOCK,
	/* Keywords */
	AZO_TERM_KEYWORD,
	/* TYPE DECLARATION[...]*/
	AZO_TERM_DECLARATION_LIST,
	/* NAME [= VALUE] */
	AZO_TERM_DECLARATION,
	/* TYPE NAME */
	AZO_TERM_ARGUMENT_DECLARATION,
	/* [REFERENCE.]function [REFERENCE] (LIST) STATEMENT */
	AZO_TERM_FUNCTION,
	/* REFERENCE, LIST */
	AZO_TERM_FUNCTION_CALL,
	AZO_TERM_ARRAY_ELEMENT,
	/* List of expressions (for example function arguments) */
	AZO_TERM_LIST,
	/* Variable reference */
	AZO_TERM_REFERENCE,
	/* Literal array */
	AZO_TERM_LITERAL_ARRAY,
	/* Cast */
	AZO_TERM_CAST,

	/* Operators */
	AZO_TERM_SUFFIX,
	AZO_TERM_PREFIX,
	AZO_TERM_BINARY,
	AZO_TERM_COMPARISON,
	AZO_TERM_ASSIGN,
	/* Type_operator */
	AZO_TERM_TEST,

	/* Resolved expressions */
	/* Subtype is value type (or 0), value is set */
	AZO_TERM_CONSTANT,
	AZO_TERM_VARIABLE,
	/* Subtype is variable type */
	AZO_TERM_TYPE,

	AZO_NUM_TERM_TYPES
};

/* Unspecified subtype */
#define AZO_TERM_GENERIC 0

/* Reference subtypes */
enum {
	/* Simple variable name */
	AZO_TERM_REFERENCE_VARIABLE,
	/* Member object (dot operator) */
	AZO_TERM_REFERENCE_MEMBER,
	/**
	 * @brief Second component (after the dot) of member reference
	 * 
	 */
	AZO_TERM_REFERENCE_PROPERTY
};

/* Variable subtypes */
enum {
	AZO_TERM_VARIABLE_PARENT,
	AZO_TERM_VARIABLE_LOCAL
};

/* Function subtypes */
enum {
	AZO_TERM_FUNCTION_STATIC,
	AZO_TERM_FUNCTION_MEMBER
};

/* Suffix subtypes */
enum {
	AZO_TERM_SUFFIX_INCREMENT,
	AZO_TERM_SUFFIX_DECREMENT
};

/* Prefix subtypes */
enum {
	AZO_TERM_PREFIX_INCREMENT,
	AZO_TERM_PREFIX_DECREMENT,
	AZO_TERM_PREFIX_PLUS,
	AZO_TERM_PREFIX_MINUS,
	AZO_TERM_PREFIX_NOT,
	AZO_TERM_PREFIX_TILDE
};

/* Arithmetic subtypes */
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

/* Comparison subtypes */
enum {
	AZO_TERM_COMPARISON_E,
	AZO_TERM_COMPARISON_NE,
	AZO_TERM_COMPARISON_LT,
	AZO_TERM_COMPARISON_LE,
	AZO_TERM_COMPARISON_GT,
	AZO_TERM_COMPARISON_GE
};

/* Assign subtypes (priority 16) */
enum {
	AZO_TERM_ASSIGN_PLAIN,
	AZO_TERM_ASSIGN_PLUS,
	AZO_TERM_ASSIGN_MINUS,
	AZO_TERM_ASSIGN_STAR,
	AZO_TERM_ASSIGN_SLASH,
	AZO_TERM_ASSIGN_PERCENT,
	AZO_TERM_ASSIGN_SHIFT_LEFT,
	AZO_TERM_ASSIGN_SHIFT_RIGHT,
	AZO_TERM_ASSIGN_AND,
	AZO_TERM_ASSIGN_OR,
	AZO_TERM_ASSIGN_XOR
};

enum {
	AZO_TERM_TEST_IS,
	AZO_TERM_TEST_IMPLEMENTS
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

struct _AZONode {
	/* Tree implementation */
	AZONode *parent;
	AZONode *next;
	AZONode *children;

	AZOTerm term;

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

AZONode *azo_node_new (unsigned int type, unsigned int subtype, unsigned int start, unsigned int end);
void azo_node_free (AZONode *expr);
void azo_node_free_tree (AZONode *expr);
void azo_node_clear_children (AZONode *expr);

AZONode *azo_node_new_number (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_integer (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_floating_point (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_text (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_reference (unsigned int subtype, const AZOSource *src, const AZOToken *token);

void azo_node_print (AZONode *expr, FILE *ofs);
void azo_node_print_list (AZONode *expr, FILE *ofs, const char *separator);

void azo_node_print_info(AZONode *expr, FILE *ofs, AZOSource *src, unsigned int indent);

#ifdef __cplusplus
}
#endif

#endif
