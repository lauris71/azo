#define __AZO_OPERATOR_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <azo/operator.h>

AZOOperator azo_operators[] = {
	{ AZO_OPERATOR_DOT, ".", 2, 0, 1 },
	{ AZO_OPERATOR_ARROW, "->", 2, 0, 1 },
	/* fixme: While parsing function arguments and literal arrays we should not treat comma as operator */
	{ AZO_OPERATOR_COMMA, ",", 0, 0, 255 },

	{ AZO_OPERATOR_EQUAL, "==", 2, 0, 7 },
	{ AZO_OPERATOR_ASSIGN, "=", 2, 0, 14 },

	{ AZO_OPERATOR_PLUSPLUS, "++", 1, 2, 1 },
	{ AZO_OPERATOR_PLUSASSIGN, "+=", 2, 0, 14 },
	/* fixme: How to handle unary versions? */
	{ AZO_OPERATOR_PLUS, "+", 2, 2, 4 },

	{ AZO_OPERATOR_MINUSMINUS, "--", 1, 2, 1 },
	{ AZO_OPERATOR_MINUSASSIGN, "-=", 2, 0, 14 },
	/* fixme: How to handle unary versions? */
	{ AZO_OPERATOR_MINUS, "-", 2, 2, 4 },

	{ AZO_OPERATOR_SLASHASSIGN, "/=", 2, 0, 14 },
	{ AZO_OPERATOR_SLASH, "/", 2, 0, 3 },

	{ AZO_OPERATOR_STARASSIGN, "*=", 2, 0, 14 },
	/* Indirection/Multiply */
	{ AZO_OPERATOR_STAR, "*", 2, 2, 3 },

	{ AZO_OPERATOR_PERCENT_ASSIGN, "%=", 2, 0, 14 },
	{ AZO_OPERATOR_PERCENT, "%", 2, 0, 3 },

	{ AZO_OPERATOR_SHIFT_LEFT_ASSIGN, "<<=", 2, 0, 14 },
	{ AZO_OPERATOR_SHIFT_LEFT, "<<", 2, 0, 5 },
	{ AZO_OPERATOR_SHIFT_RIGHT_ASSIGN, ">>=", 2, 0, 14 },
	{ AZO_OPERATOR_SHIFT_RIGHT, ">>", 2, 0, 5 },

	{ AZO_OPERATOR_GE, ">=", 2, 0, 6 },
	{ AZO_OPERATOR_GT, ">", 2, 0, 6 },
	{ AZO_OPERATOR_LE, "<=", 2, 0, 6 },
	{ AZO_OPERATOR_LT, "<", 2, 0, 6 },

	{ AZO_OPERATOR_NE, "!=", 2, 0, 7 },
	{ AZO_OPERATOR_NOT, "!", 1, 2, 0 },

	{ AZO_OPERATOR_TILDE, "~", 1, 2, 0 },

	{ AZO_OPERATOR_ANDAND, "&&", 2, 0, 11 },
	{ AZO_OPERATOR_AND_ASSIGN, "&=", 2, 0, 14 },
	/* AdressOf/and */
	{ AZO_OPERATOR_AND, "&", 2, 2, 8 },

	{ AZO_OPERATOR_CARET_ASSIGN, "^=", 2, 0, 14 },
	{ AZO_OPERATOR_CARET, "^", 2, 0, 9 },

	{ AZO_OPERATOR_OROR, "||", 2, 0, 12 },
	{ AZO_OPERATOR_OR_ASSIGN, "|=", 2, 0, 14 },
	{ AZO_OPERATOR_OR, "|", 2, 0, 10 },

	{ AZO_OPERATOR_QUESTION, "?", 3, 0, 13 },
	{ AZO_OPERATOR_COLON, ":", 0, 0, 255 },
};

unsigned int
azo_operator_get_precedence (unsigned int type, unsigned int postfix)
{
	return (postfix) ? azo_operators[type].precedence_suffix : azo_operators[type].precedence_prefix;
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
