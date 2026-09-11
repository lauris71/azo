#ifndef __AZO_OPERATOR_H__
#define __AZO_OPERATOR_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOOperator AZOOperator;

enum {
	AZO_OPERATOR_DOT,
	AZO_OPERATOR_ARROW,
	AZO_OPERATOR_LAMBDA,
	AZO_OPERATOR_COMMA,

	AZO_OPERATOR_IDENTICAL,
	AZO_OPERATOR_EQUAL,
	AZO_OPERATOR_ASSIGN,

	AZO_OPERATOR_PLUSPLUS,
	AZO_OPERATOR_PLUSASSIGN,
	AZO_OPERATOR_PLUS,

	AZO_OPERATOR_MINUSMINUS,
	AZO_OPERATOR_MINUSASSIGN,
	AZO_OPERATOR_MINUS,

	AZO_OPERATOR_SLASHASSIGN,
	AZO_OPERATOR_SLASH,

	AZO_OPERATOR_STARASSIGN,
	AZO_OPERATOR_STAR,

	AZO_OPERATOR_PERCENT_ASSIGN,
	AZO_OPERATOR_PERCENT,

	AZO_OPERATOR_SHIFT_LEFT_ASSIGN,
	AZO_OPERATOR_SHIFT_LEFT,
	AZO_OPERATOR_SHIFT_RIGHT_ASSIGN,
	AZO_OPERATOR_SHIFT_RIGHT,

	AZO_OPERATOR_GE,
	AZO_OPERATOR_GT,
	AZO_OPERATOR_LE,
	AZO_OPERATOR_LT,

	AZO_OPERATOR_NOT_IDENTICAL,
	AZO_OPERATOR_NE,
	AZO_OPERATOR_NOT,

	AZO_OPERATOR_TILDE,

	AZO_OPERATOR_ANDAND_ASSIGN,
	AZO_OPERATOR_ANDAND,
	AZO_OPERATOR_AND_ASSIGN,
	AZO_OPERATOR_AND,

	AZO_OPERATOR_CARET_ASSIGN,
	AZO_OPERATOR_CARET,

	AZO_OPERATOR_OROR_ASSIGN,
	AZO_OPERATOR_OROR,
	AZO_OPERATOR_OR_ASSIGN,
	AZO_OPERATOR_OR,

	AZO_OPERATOR_QUESTION,
	AZO_OPERATOR_COLON,

	AZO_NUM_OPERATORS
};

struct _AZOOperator {
	unsigned short type;
	const char *text;
	unsigned short valence;
	/* Binary/postfix binding powers (higher value binds tighter) */
	struct {
		unsigned short left;   /* Continue-gate value: the operator binds if left > context floor */
		unsigned short right;  /* RHS parse floor (left - 1 for right-associative operators) */
	} precedence;
	/* Unary/prefix binding power (0 if the operator is not prefixable) */
	unsigned short precedence_prefix;
};

#ifndef __AZO_OPERATOR_C__
extern AZOOperator azo_operators[];
#endif

/* Precedence groups (higher value binds tighter)
 *
 * The parser continues consuming an operator while operator.left > floor (strict).
 * Thus parsing at a floor equal to a separator's own precedence stops at that
 * separator but lets everything tighter bind - e.g. parsing a list item at
 * AZO_PRECEDENCE_COMMA stops at the comma without any offset. */
#define AZO_PRECEDENCE_MINIMUM 0		/* the lowest floor - parses a full expression (every operator binds) */
#define AZO_PRECEDENCE_COMMA 0			/* comma is a separator, not an operator; coincides with the statement boundary */
#define AZO_PRECEDENCE_TERNARY 15		/* ? : (right-associative) */
#define AZO_PRECEDENCE_ASSIGN 20		/* = += -= ... (right-associative) */
#define AZO_PRECEDENCE_TYPE 30			/* is / implements / as */
#define AZO_PRECEDENCE_LOGICAL_OR 40	/* || */
#define AZO_PRECEDENCE_LOGICAL_AND 50	/* && */
#define AZO_PRECEDENCE_OR 60			/* | */
#define AZO_PRECEDENCE_XOR 70			/* ^ */
#define AZO_PRECEDENCE_AND 80			/* & */
#define AZO_PRECEDENCE_EQUALITY 90		/* == != === !== */
#define AZO_PRECEDENCE_COMPARISON 100	/* < <= > >= */
#define AZO_PRECEDENCE_SHIFT 110		/* << >> */
#define AZO_PRECEDENCE_ADDITIVE 120		/* + - */
#define AZO_PRECEDENCE_MULTIPLICATIVE 130	/* * / % */
#define AZO_PRECEDENCE_CAST 135			/* (type) - between multiplicative and unary */
#define AZO_PRECEDENCE_UNARY 140		/* prefix + - ! ~ ++ -- */
#define AZO_PRECEDENCE_POSTFIX 150		/* postfix ++ -- */
#define AZO_PRECEDENCE_FUNCTION 160		/* ( call */
#define AZO_PRECEDENCE_ARRAY 160		/* [ subscript */
#define AZO_PRECEDENCE_MEMBER 180		/* . member reference */

unsigned int azo_operator_get_left_precedence (unsigned int type);
unsigned int azo_operator_get_right_precedence (unsigned int type);

unsigned int azo_operator_is_binary (unsigned int type);
unsigned int azo_operator_is_assignment (unsigned int type);
/* Comparison operators (== != < <= > >=) - map to AZO_TERM_COMPARISON_* */
unsigned int azo_operator_is_comparison (unsigned int type);
/* Arithmetic, bitwise and logical binary operators - map to AZO_TERM_BINARY/ARITHMETIC_* */
unsigned int azo_operator_is_arithmetic (unsigned int type);

#ifdef __cplusplus
}
#endif

#endif
