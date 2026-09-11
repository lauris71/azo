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
	/* Statement group - behaves like a block but does not create a new scope */
	AZO_TERM_STATEMENT_GROUP,
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
	/* Selection (ternary) - CONDITION, TRUE_EXPRESSION, FALSE_EXPRESSION */
	AZO_TERM_SELECT,

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
	AZO_TERM_COMPARISON_GE,
	/* Identity (reference/value identity, no coercion) */
	AZO_TERM_COMPARISON_IDENTICAL,
	AZO_TERM_COMPARISON_NOT_IDENTICAL
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

/* CAST subtypes */
enum {
	/* Primitive value conversion - (type) expression, flags EXACT/ROUNDED */
	AZO_TERM_CAST_CONVERT,
	/* Checked class/interface conversion - expression as Type */
	AZO_TERM_CAST_AS
};

struct _AZOTerm {
	/**
	 * @brief Term main type
	 *
	 */
	uint16_t type;
	/**
	 * @brief Term flags (AZO_TERM_FLAG_*)
	 *
	 */
	uint16_t flags;
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

/* Term flags */
enum {
	/* Cast qualifiers ((int16 rounded) x - allow rounding, (int16 exact) x - throw unless exact) */
	AZO_TERM_FLAG_EXACT = 1,
	AZO_TERM_FLAG_ROUNDED = 2,
	/* Declaration qualifiers */
	AZO_TERM_FLAG_STATIC = 4,
	AZO_TERM_FLAG_CONST = 8,
	AZO_TERM_FLAG_FINAL = 16
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

AZONode *azo_node_new(unsigned int type, unsigned int subtype, unsigned int start, unsigned int end);
/**
 * @brief Create a new node with children
 * 
 * Creates a new node and links the given children into its child list in
 * order. The next pointer of every child is written, so the list is always
 * properly terminated.
 * 
 * The last (tail) child may be NULL, in which case the list is terminated
 * at the previous child. Passing NULL for any child except the last is not
 * allowed (it would break the chain).
 * 
 * @param type term type of the new node
 * @param subtype term subtype of the new node
 * @param start term start in source code
 * @param end term end in source code
 * @param n_children number of child arguments
 * @param ... the children, in order
 * @return the new node
 */
AZONode *azo_node_new_with_children(unsigned int type, unsigned int subtype, unsigned int start, unsigned int end, unsigned int n_children, ...);

void azo_node_free (AZONode *expr);
void azo_node_free_tree (AZONode *expr);
void azo_node_clear_children (AZONode *expr);

/**
 * @brief Flatten a node tree into an array
 * 
 * Writes the nodes of the tree into the given array in pre-order (node first,
 * then children recursively, then next sibling). The array contains pointers
 * to the original nodes, so attached values (e.g. constants) are preserved.
 * 
 * At most max_nodes nodes are written. The return value is the total number
 * of nodes in the tree, so if it exceeds max_nodes the array was truncated.
 * 
 * @param node the root node (its siblings are flattened too)
 * @param nodes output array
 * @param max_nodes capacity of the output array
 * @return the total number of nodes
 */
unsigned int azo_node_flatten (AZONode *node, AZONode **nodes, unsigned int max_nodes);

AZONode *azo_node_new_number (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_integer (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_floating_point (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_text (const AZOSource *src, const AZOToken *token);
AZONode *azo_node_new_reference (unsigned int subtype, const AZOSource *src, const AZOToken *token);

/**
 * @brief Get prefix term subtype from token
 * 
 * @param token the token to check
 * @return the subtype, or -1 if not a prefix operator
 */
int azo_token_get_prefix_term(const AZOToken *token);

/**
 * @brief Get assignment term subtype from token
 * 
 * @param token the token to check
 * @return the subtype, or -1 if not an assignment operator
 */
int azo_token_get_assignment_term(const AZOToken *token);

/**
 * @brief Get comparison term subtype from token
 * 
 * @param token the token to check
 * @return the subtype, or -1 if not a comparison operator
 */
int azo_token_get_comparison_term(const AZOToken *token);

/**
 * @brief Get binary (arithmetic/logical/bitwise) term subtype from token
 * 
 * @param token the token to check
 * @return the subtype, or -1 if not a binary operator
 */
int azo_token_get_binary_term(const AZOToken *token);

void azo_node_print (AZONode *expr, FILE *ofs);
void azo_node_print_list (AZONode *expr, FILE *ofs, const char *separator);

void azo_node_print_info(AZONode *expr, FILE *ofs, AZOSource *src, unsigned int indent);

#ifdef __cplusplus
}
#endif

#endif
