#define __AZO_OPERATOR_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <azo/operator.h>

/* The tokenizer matches operators greedily in table order (first match wins), so
 * longer operators have to be listed before their prefixes (e.g. "<<=" before "<<") */
/* Precedence: higher value binds tighter. The parser continues consuming operators
 * while operator.precedence.left > context floor. Left-associative operators have
 * left == right; right-associative ones (assignment, ternary) have right = left - 1,
 * so the recursive RHS parse does not stop at the same operator.
 * Member reference (.) binds tighter than function call/array access, so a parse at
 * AZO_PRECEDENCE_FUNCTION consumes a member reference path and stops at the first ( */
AZOOperator azo_operators[] = {
	{ AZO_OPERATOR_DOT, ".", 2, { AZO_PRECEDENCE_MEMBER, AZO_PRECEDENCE_MEMBER }, 0 },
	/* Not used currently, keep for C reference */
	{ AZO_OPERATOR_ARROW, "->", 2, { AZO_PRECEDENCE_MEMBER, AZO_PRECEDENCE_MEMBER }, 0 },
	/* Structural (lambda), not an expression operator */
	{ AZO_OPERATOR_LAMBDA, "=>", 0, { 0, 0 }, 0 },
	/* In current version comma is not an operator at all (is separator) but we keep it here for C reference */
	{ AZO_OPERATOR_COMMA, ",", 0, { AZO_PRECEDENCE_COMMA, AZO_PRECEDENCE_COMMA }, 0 },

	{ AZO_OPERATOR_IDENTICAL, "===", 2, { AZO_PRECEDENCE_EQUALITY, AZO_PRECEDENCE_EQUALITY }, 0 },
	{ AZO_OPERATOR_EQUAL, "==", 2, { AZO_PRECEDENCE_EQUALITY, AZO_PRECEDENCE_EQUALITY }, 0 },
	/* Assignment is right-associative (right < left) */
	{ AZO_OPERATOR_ASSIGN, "=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },

	{ AZO_OPERATOR_PLUSPLUS, "++", 1, { AZO_PRECEDENCE_POSTFIX, AZO_PRECEDENCE_POSTFIX }, AZO_PRECEDENCE_UNARY },
	{ AZO_OPERATOR_PLUSASSIGN, "+=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_PLUS, "+", 2, { AZO_PRECEDENCE_ADDITIVE, AZO_PRECEDENCE_ADDITIVE }, AZO_PRECEDENCE_UNARY },

	{ AZO_OPERATOR_MINUSMINUS, "--", 1, { AZO_PRECEDENCE_POSTFIX, AZO_PRECEDENCE_POSTFIX }, AZO_PRECEDENCE_UNARY },
	{ AZO_OPERATOR_MINUSASSIGN, "-=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_MINUS, "-", 2, { AZO_PRECEDENCE_ADDITIVE, AZO_PRECEDENCE_ADDITIVE }, AZO_PRECEDENCE_UNARY },

	{ AZO_OPERATOR_SLASHASSIGN, "/=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_SLASH, "/", 2, { AZO_PRECEDENCE_MULTIPLICATIVE, AZO_PRECEDENCE_MULTIPLICATIVE }, 0 },

	{ AZO_OPERATOR_STARASSIGN, "*=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	/* Indirection/Multiply */
	{ AZO_OPERATOR_STAR, "*", 2, { AZO_PRECEDENCE_MULTIPLICATIVE, AZO_PRECEDENCE_MULTIPLICATIVE }, AZO_PRECEDENCE_UNARY },

	{ AZO_OPERATOR_PERCENT_ASSIGN, "%=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_PERCENT, "%", 2, { AZO_PRECEDENCE_MULTIPLICATIVE, AZO_PRECEDENCE_MULTIPLICATIVE }, 0 },

	{ AZO_OPERATOR_SHIFT_LEFT_ASSIGN, "<<=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_SHIFT_LEFT, "<<", 2, { AZO_PRECEDENCE_SHIFT, AZO_PRECEDENCE_SHIFT }, 0 },
	{ AZO_OPERATOR_SHIFT_RIGHT_ASSIGN, ">>=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_SHIFT_RIGHT, ">>", 2, { AZO_PRECEDENCE_SHIFT, AZO_PRECEDENCE_SHIFT }, 0 },

	{ AZO_OPERATOR_GE, ">=", 2, { AZO_PRECEDENCE_COMPARISON, AZO_PRECEDENCE_COMPARISON }, 0 },
	{ AZO_OPERATOR_GT, ">", 2, { AZO_PRECEDENCE_COMPARISON, AZO_PRECEDENCE_COMPARISON }, 0 },
	{ AZO_OPERATOR_LE, "<=", 2, { AZO_PRECEDENCE_COMPARISON, AZO_PRECEDENCE_COMPARISON }, 0 },
	{ AZO_OPERATOR_LT, "<", 2, { AZO_PRECEDENCE_COMPARISON, AZO_PRECEDENCE_COMPARISON }, 0 },

	{ AZO_OPERATOR_NOT_IDENTICAL, "!==", 2, { AZO_PRECEDENCE_EQUALITY, AZO_PRECEDENCE_EQUALITY }, 0 },
	{ AZO_OPERATOR_NE, "!=", 2, { AZO_PRECEDENCE_EQUALITY, AZO_PRECEDENCE_EQUALITY }, 0 },
	{ AZO_OPERATOR_NOT, "!", 1, { 0, 0 }, AZO_PRECEDENCE_UNARY },
	{ AZO_OPERATOR_TILDE, "~", 1, { 0, 0 }, AZO_PRECEDENCE_UNARY },

	{ AZO_OPERATOR_ANDAND_ASSIGN, "&&=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_ANDAND, "&&", 2, { AZO_PRECEDENCE_LOGICAL_AND, AZO_PRECEDENCE_LOGICAL_AND }, 0 },
	{ AZO_OPERATOR_AND_ASSIGN, "&=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	/* AdressOf/and */
	{ AZO_OPERATOR_AND, "&", 2, { AZO_PRECEDENCE_AND, AZO_PRECEDENCE_AND }, AZO_PRECEDENCE_UNARY },

	{ AZO_OPERATOR_CARET_ASSIGN, "^=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_CARET, "^", 2, { AZO_PRECEDENCE_XOR, AZO_PRECEDENCE_XOR }, 0 },

	{ AZO_OPERATOR_OROR_ASSIGN, "||=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_OROR, "||", 2, { AZO_PRECEDENCE_LOGICAL_OR, AZO_PRECEDENCE_LOGICAL_OR }, 0 },
	{ AZO_OPERATOR_OR_ASSIGN, "|=", 2, { AZO_PRECEDENCE_ASSIGN, AZO_PRECEDENCE_ASSIGN - 1 }, 0 },
	{ AZO_OPERATOR_OR, "|", 2, { AZO_PRECEDENCE_OR, AZO_PRECEDENCE_OR }, 0 },

	/* Ternary is right-associative (right < left) */
	{ AZO_OPERATOR_QUESTION, "?", 3, { AZO_PRECEDENCE_TERNARY, AZO_PRECEDENCE_TERNARY - 1 }, 0 },
	{ AZO_OPERATOR_COLON, ":", 0, { 0, 0 }, 0 },
};

unsigned int
azo_operator_get_left_precedence (unsigned int type)
{
	return azo_operators[type].precedence.left;
}

unsigned int
azo_operator_get_right_precedence (unsigned int type)
{
	return azo_operators[type].precedence.right;
}

unsigned int
azo_operator_is_binary (unsigned int type)
{
	return azo_operators[type].valence == 2;
}

unsigned int
azo_operator_is_assignment (unsigned int type)
{
	return (type == AZO_OPERATOR_ASSIGN) ||
		(type == AZO_OPERATOR_PLUSASSIGN) ||
		(type == AZO_OPERATOR_MINUSASSIGN) ||
		(type == AZO_OPERATOR_STARASSIGN) ||
		(type == AZO_OPERATOR_SLASHASSIGN) ||
		(type == AZO_OPERATOR_PERCENT_ASSIGN) ||
		(type == AZO_OPERATOR_SHIFT_LEFT_ASSIGN) ||
		(type == AZO_OPERATOR_SHIFT_RIGHT_ASSIGN) ||
		(type == AZO_OPERATOR_AND_ASSIGN) ||
		(type == AZO_OPERATOR_CARET_ASSIGN) ||
		(type == AZO_OPERATOR_OR_ASSIGN);
}

unsigned int
azo_operator_is_comparison (unsigned int type)
{
	return (type == AZO_OPERATOR_EQUAL) || (type == AZO_OPERATOR_NE) ||
		(type == AZO_OPERATOR_GE) || (type == AZO_OPERATOR_GT) ||
		(type == AZO_OPERATOR_LE) || (type == AZO_OPERATOR_LT) ||
		(type == AZO_OPERATOR_IDENTICAL) || (type == AZO_OPERATOR_NOT_IDENTICAL);
}

unsigned int
azo_operator_is_arithmetic (unsigned int type)
{
	switch (type) {
	case AZO_OPERATOR_PLUS:
	case AZO_OPERATOR_MINUS:
	case AZO_OPERATOR_SLASH:
	case AZO_OPERATOR_STAR:
	case AZO_OPERATOR_PERCENT:
	case AZO_OPERATOR_SHIFT_LEFT:
	case AZO_OPERATOR_SHIFT_RIGHT:
	case AZO_OPERATOR_ANDAND:
	case AZO_OPERATOR_AND:
	case AZO_OPERATOR_OROR:
	case AZO_OPERATOR_OR:
	case AZO_OPERATOR_CARET:
		return 1;
	}
	return 0;
}
