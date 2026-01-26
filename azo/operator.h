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
	AZO_OPERATOR_COMMA,

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

	AZO_OPERATOR_NE,
	AZO_OPERATOR_NOT,

	AZO_OPERATOR_TILDE,

	AZO_OPERATOR_ANDAND,
	AZO_OPERATOR_AND_ASSIGN,
	AZO_OPERATOR_AND,

	AZO_OPERATOR_CARET_ASSIGN,
	AZO_OPERATOR_CARET,

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
	unsigned short precedence_prefix;
	unsigned short precedence_suffix;
};

#ifndef __AZO_OPERATOR_C__
extern AZOOperator azo_operators[];
#endif

#define AZO_PRECEDENCE_ARRAY 1
#define AZO_PRECEDENCE_FUNCTION 1
#define AZO_PRECEDENCE_CAST 2
#define AZO_PRECEDENCE_UNARY 2
/* is and instanceof */
#define AZO_PRECEDENCE_TYPE 10
#define AZO_PRECEDENCE_ASSIGN 14
#define AZO_PRECEDENCE_COMMA 15
#define AZO_PRECEDENCE_MINIMUM 255

unsigned int azo_operator_get_precedence (unsigned int subtype, unsigned int postfix);

unsigned int azo_operator_is_binary (unsigned int type);
unsigned int azo_operator_is_assignment (unsigned int type);

#ifdef __cplusplus
}
#endif

#endif
