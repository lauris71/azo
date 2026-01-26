#define __AZO_PARSER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arikkei/arikkei-utils.h>

#include <az/string.h>
#include <az/extend.h>

#include <azo/keyword.h>
#include <azo/operator.h>
#include <azo/parser.h>

enum AZOParserError {
	ERROR_NONE,
	ERROR_UNEXPECTED_EOF,
	ERROR_SYNTAX,
	ERROR_END_OF_BLOCK_MISSING,
	ERROR_SEMICOLON_MISSING,
	ERROR_CLOSING_PARENTHESIS_MISSING,
	ERROR_INVALID_START_OF_EXPRESSION
};

const char *parser_errors[] = {
	"No error",
	"Unexpected EOF",
	"Syntax error",
	"End of block missing",
	"Missing semicolon",
	"Closing brace missing",
	"Invalid start of expression"
};

static unsigned int parse_program (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_sentences (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_sentence (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_block (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_line (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_return (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_step_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_declaration_expression (AZOParser *parser, AZOToken *token, unsigned int qual_static, unsigned int qual_final);
static unsigned int azo_parser_parse_single_declaration (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_silent_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_parenthesed_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_naked_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_continue_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_prefix_expression (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_assignment_expression (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_function_definition (AZOParser *parser, AZOToken *token, unsigned int has_return_type, unsigned int is_member);

static void parser_report_error (AZOParser *parser, unsigned int errval);

static unsigned int azo_parser_type = 0;
static AZOParserClass *azo_parser_class = NULL;

unsigned int
azo_parser_get_type (void)
{
	if (!azo_parser_type) {
		azo_parser_class = (AZOParserClass *) az_register_type (&azo_parser_type, (const unsigned char *) "AZOParser", AZ_TYPE_BLOCK, sizeof (AZOParserClass), sizeof (AZOParser), AZ_FLAG_ZERO_MEMORY,
		NULL, NULL, NULL);
	}
	return azo_parser_type;
}

void
azo_parser_setup (AZOParser *parser, AZOSource *src)
{
	parser->src = src;
	azo_source_ref(src);
	azo_tokenizer_setup (&parser->tokenizer, src->cdata, src->csize);
	parser->parent = NULL;
}

void
azo_parser_release (AZOParser *parser)
{
	azo_tokenizer_release (&parser->tokenizer);
	azo_source_unref(parser->src);
}

AZOExpression *
azo_parser_parse (AZOParser *parser)
{
	AZOToken token;
	unsigned int result;

	if (azo_tokenizer_get_next_token (&parser->tokenizer, &token)) {
		result = parse_program (parser, &token);
		if (result) {
			parser_report_error (parser, result);
		}
		if (azo_tokenizer_has_next_token (&parser->tokenizer)) {
			/* Either parsing error or unexpected end of block */
		}
	}

	return parser->parent;
}

static void
parser_report_error (AZOParser *parser, unsigned int errval)
{
	fprintf (stderr, "%s at line ", parser_errors[errval]);
	if (!azo_tokenizer_is_eof (&parser->tokenizer)) {
		unsigned int first, last;
		if (azo_source_find_line_range (parser->tokenizer.src, parser->tokenizer.tokens[parser->tokenizer.c_token].start, parser->tokenizer.tokens[parser->tokenizer.c_token].end, &first, &last)) {
			fprintf (stderr, "%d near ", first);
			azo_tokenizer_print_token (&parser->tokenizer, &parser->tokenizer.tokens[parser->tokenizer.c_token], stderr);
			fprintf (stderr, "\n");
			/* fixme: Keep track of the last processed token */
			azo_source_print_lines (parser->tokenizer.src, first, last + 1);
		}
	} else {
		fprintf (stderr, "near EOF\n");
		azo_source_ensure_lines(parser->tokenizer.src);
		azo_source_print_lines (parser->tokenizer.src, parser->tokenizer.src->n_lines - 1, parser->tokenizer.src->n_lines);
	}
}

AZOExpression *
parser_get_last (AZOParser *parser)
{
	AZOExpression *last;
	arikkei_return_val_if_fail (parser->parent != NULL, NULL);
	arikkei_return_val_if_fail (parser->parent->children != NULL, NULL);
	last = parser->parent->children;
	while (last->next) last = last->next;
	return last;
}

/* Append expression to the rightmost position at the current level */

static void
parser_append (AZOParser *parser, AZOExpression *expr)
{
	expr->parent = parser->parent;
	if (!parser->parent->children) {
		parser->parent->children = expr;
	} else {
		AZOExpression *last;
		last = parser_get_last (parser);
		last->next = expr;
	}
}

/* Detach and return the rightmost expression at the current level */
AZOExpression *
parser_detach_last (AZOParser *parser)
{
	AZOExpression *prev, *last;
	arikkei_return_val_if_fail (parser->parent != NULL, NULL);
	arikkei_return_val_if_fail (parser->parent->children != NULL, NULL);
	prev = NULL;
	last = parser->parent->children;
	while (last->next) {
		prev = last;
		last = last->next;
	}
	if (prev) {
		prev->next = NULL;
	} else {
		parser->parent->children = NULL;
	}
	return last;
}

/* Push current as the new parent */

static void
parser_push (AZOParser *parser)
{
	AZOExpression *last;
	last = parser_get_last (parser);
	arikkei_return_if_fail (last != NULL);
	parser->parent = last;
}

/* Pop current and go to previous parent */

static void
parser_pop (AZOParser *parser)
{
	arikkei_return_if_fail (parser->parent->parent != NULL);
	parser->parent = parser->parent->parent;
}

static unsigned int
expression_is_silent_statement (AZOExpression *expr)
{
	if (expr->type == AZO_TERM_EMPTY) return 1;
	if (expr->type == EXPRESSION_ASSIGN) return 1;
	if (expr->type == EXPRESSION_FUNCTION_CALL) return 1;
	if (expr->type == EXPRESSION_PREFIX) {
		if (expr->subtype == PREFIX_INCREMENT) return 1;
		if (expr->subtype == PREFIX_DECREMENT) return 1;
	} else if (expr->type == EXPRESSION_SUFFIX) {
		if (expr->subtype == SUFFIX_INCREMENT) return 1;
		if (expr->subtype == SUFFIX_DECREMENT) return 1;
	}
	return 0;
}

static unsigned int parse_operator (AZOParser *parser, AZOToken *token);
static unsigned int parse_type_operator (AZOParser *parser, AZOToken *token);
static unsigned int parse_for (AZOParser *parser, AZOToken *token);
static unsigned int parse_while (AZOParser *parser, AZOToken *token);
static unsigned int parse_if (AZOParser *parser, AZOToken *token);
static unsigned int parse_function_call (AZOParser *parser, AZOToken *token);
static unsigned int parse_new (AZOParser *parser, AZOToken *token);
static unsigned int parse_array_literal (AZOParser *parser, AZOToken *token);
static unsigned int parse_array_element (AZOParser *parser, AZOToken *token);

/*
 * Program:
 *   Sentences
 */

static unsigned int
parse_program (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr = azo_expression_new (AZO_EXPRESSION_PROGRAM, EXPRESSION_GENERIC, token->start, 0);
	parser->parent = expr;
	unsigned int result = azo_parser_parse_sentences (parser, token);
	if (result != ERROR_NONE) return result;
	if (token->type != AZO_TOKEN_EOF) {
		return ERROR_UNEXPECTED_EOF;
	}
	if (parser->parent != expr) {
		fprintf(stderr, "parse_program: Internal error\n");
	}
	expr->end = token->start;
	return result;
}

/*
 * Sentences:
 *   [Sentence ...]
 */

static unsigned int
azo_parser_parse_sentences (AZOParser *parser, AZOToken *token)
{
	unsigned int result = ERROR_NONE;
	while (!result && !azo_tokenizer_is_eof (&parser->tokenizer)) {
		/* Sentence list is terminated either by EOF or closing BRACE */
		if (token->type == AZO_TOKEN_RIGHT_BRACE) return ERROR_NONE;
		result = azo_parser_parse_sentence (parser, token);
	}
	return result;
}

/*
 * Sentence:
 *   Block
 *   Line
 *   for
 *   while
 *   if
 */

static unsigned int
azo_parser_parse_sentence (AZOParser *parser, AZOToken *token)
{
	if (token->type == AZO_TOKEN_LEFT_BRACE) {
		return azo_parser_parse_block (parser, token);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FOR)) {
		return parse_for (parser, token);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_WHILE)) {
		return parse_while (parser, token);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_IF)) {
		return parse_if (parser, token);
	}
	/* Line */
	return azo_parser_parse_line (parser, token);
}

/*
 * Block:
 *   { Sentences }
 */

static unsigned int
azo_parser_parse_block (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr;
	unsigned int result;
	/* Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	expr = azo_expression_new (AZO_EXPRESSION_BLOCK, EXPRESSION_GENERIC, token->start, token->end);
	parser_append (parser, expr);
	parser_push (parser);
	result = azo_parser_parse_sentences (parser, token);
	if (result) return result;
	/* Block has to end with "}" */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_BRACE) return ERROR_END_OF_BLOCK_MISSING;
	expr->end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return result;
}

/*
 * Line:
 *   Statement;
 */

static unsigned int
azo_parser_parse_line (AZOParser *parser, AZOToken *token)
{
	unsigned int result;
	result = azo_parser_parse_statement (parser, token);
	if (result) return result;
	/* Parse expression keeps token at closing token */
	if (token->type != AZO_TOKEN_SEMICOLON) return ERROR_SEMICOLON_MISSING;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	return result;
}

/*
 * Statement:
 *   Step_statement
 *   debug
 *   return
 */

static unsigned int
azo_parser_parse_statement (AZOParser *parser, AZOToken *token)
{
	unsigned int result;
	/* Return */
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_RETURN)) {
		return azo_parser_parse_return (parser, token);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_DEBUG)) {
		AZOExpression *expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_DEBUG, token->start, token->end);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		return ERROR_NONE;
	}
	result = azo_parser_parse_step_statement (parser, token);
	return result;
}

/*
* Return:
*   return Expression
*/

static unsigned int
azo_parser_parse_return (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *val = NULL;
	unsigned int start, end, result = ERROR_NONE;
	start = token->start;
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) {
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (result) return result;
		val = parser_detach_last (parser);
		if (!val) {
			fprintf (stderr, "azo_parser_parse_return: Expression resulted in no value\n");
			return ERROR_SYNTAX;
		}
		end = val->end;
	}
	expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_RETURN, start, end);
	expr->children = val;
	parser_append (parser, expr);
	return result;
}

/*
 * Step_statement:
 *   Declaration
 *   Silent_statement
 * 
 * Normally it is terminated by semicolon, except when part of for signature
 * As both callers (::parse_line and ::parse_for) check for proper ending we can be generous here
 */

static unsigned int
azo_parser_parse_step_statement (AZOParser *parser, AZOToken *token)
{
	unsigned int qual_static = 0, qual_const = 0, qual_final = 0;
	unsigned int result;
	if ((token->type == AZO_TOKEN_SEMICOLON) || (token->type == AZO_TOKEN_RIGHT_PARENTHESIS)) {
		/* EMPTY */
		AZOExpression *expr;
		if (qual_static || qual_const || qual_final) return ERROR_SYNTAX;
		expr = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, token->start, token->start);
		parser_append (parser, expr);
		return ERROR_NONE;
	}
	/* [static] [final] TYPE ... */
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_STATIC)) {
		/* If static, const or final is present the statement has to be declaration */
		qual_static = 1;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	}
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_CONST)) {
		qual_const = 1;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	}
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FINAL)) {
		qual_final = 1;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	}
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FUNCTION)) {
		/* function */
		/* We create reference as it should be interpreted as type */
		AZOExpression *expr = azo_expression_new_reference (REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		if (token->type != AZO_TOKEN_WORD) return ERROR_SYNTAX;
		return azo_parser_parse_declaration_expression (parser, token, qual_static, qual_final);
	} else {
		/* Either Declaration or Silent Statement */
		/* Both start with expression */
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (result) return result;
	}
	/* If a naked word follows it is declaration */
	if (!azo_tokenizer_is_eof (&parser->tokenizer) && (token->type == AZO_TOKEN_WORD)) {
		return azo_parser_parse_declaration_expression (parser, token, qual_static, qual_final);
	} else {
		AZOExpression *expr;
		if (qual_static || qual_const || qual_final) return ERROR_SYNTAX;
		expr = parser_get_last (parser);
		if (!expression_is_silent_statement (expr)) return ERROR_SYNTAX;
		return ERROR_NONE;
	}
}

/*
 * Declaration:
 *   Qualifiers Type Single_declaration [, Single_declaration ...]
 *
 * Type expression is already parsed
 */

static unsigned int
azo_parser_parse_declaration_expression (AZOParser *parser, AZOToken *token, unsigned int qual_static, unsigned int qual_final)
{
	unsigned int result = ERROR_NONE;
	AZOExpression *declr, *type, *last;
	/* Create topmost declaration list */
	type = parser_detach_last (parser);
	declr = azo_expression_new (EXPRESSION_DECLARATION_LIST, EXPRESSION_GENERIC, type->start, token->end);
	declr->children = type;
	/* fixme: Qualifiers */
	last = type;
	while (token->type == AZO_TOKEN_WORD) {
		AZOExpression *expr;
		result = azo_parser_parse_single_declaration (parser, token);
		if (result) {
			azo_expression_free_tree (declr);
			return result;
		}
		expr = parser_detach_last(parser);
		last->next = expr;
		last = expr;
		declr->end = expr->end;
		if (token->type != AZO_TOKEN_COMMA) break;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			azo_expression_free_tree (declr);
			return ERROR_SYNTAX;
		}
	}
	parser_append (parser, declr);
	return result;
}

/*
 * Single_declaration:
 *  Pure_declaration
 *  Declaration_initialization
 *
 * Pure_declaration:
 *  NAME
 *
 * Declaration_initialization:
 *  Pure_declaration = Expression
 */

static unsigned int
azo_parser_parse_single_declaration (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *right;
	unsigned int result, end;
	if (token->type != AZO_TOKEN_WORD) return ERROR_SYNTAX;
	expr = azo_expression_new_reference (REFERENCE_VARIABLE, parser->src, token);
	parser_append (parser, expr);
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_ASSIGN) {
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_SYNTAX;
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (result) return result;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		end = right->end;
	} else {
		right = NULL;
		left = parser_detach_last (parser);
		end = left->end;
	}
	expr = azo_expression_new (EXPRESSION_DECLARATION, EXPRESSION_GENERIC, left->start, end);
	expr->children = left;
	left->next = right;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
 * Silent_statement:
 *   Empty_statement
 *   Assignment
 *   Function_call
 *   Prefix_arithmetic
 *   Suffix_arithmetic
 */

static unsigned int
azo_parser_parse_silent_statement (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr;
	unsigned int result;
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (result) return result;
	expr = parser_get_last (parser);
	if (!expression_is_silent_statement (expr)) return ERROR_SYNTAX;
	return ERROR_NONE;
}

/*
 * Expression:
 *   Parenthesed_expression
 *   Naked_expression
 */

static unsigned int
azo_parser_parse_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		/* Parenthesed_expression */
		return azo_parser_parse_parenthesed_expression (parser, token, left_precedence);
	} else {
		/* Naked expression */
		return azo_parser_parse_naked_expression (parser, token, left_precedence);
	}
}

/*
* Parenthesed_expression:
*   (Expression)
*/

static unsigned int
azo_parser_parse_parenthesed_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	unsigned int result;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return ERROR_SYNTAX;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
 * Naked_expression:
 *   Literal
 *   new
 *   function
 *   Variable_reference
 *   Member_reference
 *   Array_reference
 *   Function_call
 *   Operation
 */

static unsigned int
azo_parser_parse_naked_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	AZOExpression *expr = NULL;
	unsigned int result;
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_NULL)) {
		/* null */
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_NONE, token->start, token->end);
		/* fixme: Allowed next - operator */
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_THIS)) {
		/* this */
		expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_THIS, token->start, token->end);
		/* fixme: Allowed next - operator/function/array */
	} else if (AZO_TOKEN_IS_NUMBER (token)) {
		expr = azo_expression_new_number (parser->src, token);
	} else if (token->type == AZO_TOKEN_TEXT) {
		/* String literal */
		expr = azo_expression_new_text (parser->src, token);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_TRUE)) {
		/* true */
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_BOOLEAN, token->start, token->end);
		az_packed_value_set_boolean (&expr->value, 1);
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FALSE)) {
		/* false */
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_BOOLEAN, token->start, token->end);
		az_packed_value_set_boolean (&expr->value, 0);
	} else if (token->type == AZO_TOKEN_LEFT_BRACE) {
		/* Array literal */
		result = parse_array_literal (parser, token);
		if (result) return result;
		/* fixme: Allowed next - array */
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_NEW)) {
		/* new */
		return parse_new (parser, token);
		/* fixme: Are operators allowed here? */
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FUNCTION)) {
		/* function */
		result = azo_parser_parse_function_definition (parser, token, 0, 0);
		if (result) return result;
		/* fixme: Are operators allowed here? */
	} else if (token->type == AZO_TOKEN_WORD) {
		/* Variable reference */
		expr = azo_expression_new_reference (REFERENCE_VARIABLE, parser->src, token);
		/* fixme: Allowed next - operator/function/array */
	} else if (AZO_TOKEN_IS_OPERATOR (token)) {
		result = azo_parser_parse_prefix_expression (parser, token);
		if (result) return result;
		/* fixme: Allowed next - operator */
	} else {
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		return ERROR_INVALID_START_OF_EXPRESSION;
	}
	if (expr) {
		parser_append (parser, expr);
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
	}
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
 * Member:
 *   function (declares member function with implicit this parameter)
 *   Variable_reference
 *   Member_reference
 *   Array_reference
 *   Function_call
 *   Operation
 */

static unsigned int
azo_parser_parse_member (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	AZOExpression *expr = NULL;
	unsigned int result;
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FUNCTION)) {
		/* function */
		result = azo_parser_parse_function_definition (parser, token, 0, 1);
		if (result) return result;
		/* fixme: Are operators allowed here? */
	} else if (token->type == AZO_TOKEN_WORD) {
		/* Variable reference */
		expr = azo_expression_new_reference (REFERENCE_PROPERTY, parser->src, token);
		/* fixme: Allowed next - operator/function/array */
	} else {
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		return ERROR_INVALID_START_OF_EXPRESSION;
	}
	if (expr) {
		parser_append (parser, expr);
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
	}
	return azo_parser_continue_expression (parser, token, left_precedence);
}

static unsigned int
azo_parser_continue_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	/* If operator or ( or [ is following analyze whether we have to proceed further */
	while (!azo_tokenizer_is_eof (&parser->tokenizer)) {
		unsigned int error;
		if (AZO_TOKEN_IS_OPERATOR (token)) {
			unsigned int op, rightprecedence;
			op = AZO_TOKEN_OPERATOR_CODE (token);
			rightprecedence = azo_operator_get_precedence (op, 1);
			if (left_precedence > rightprecedence) {
				error = parse_operator (parser, token);
				if (error) return error;
			} else {
				break;
			}
		} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_IS) || azo_token_is_keyword (parser->src, token, AZO_KEYWORD_IMPLEMENTS)) {
			if (left_precedence > AZO_PRECEDENCE_TYPE) {
				error = parse_type_operator (parser, token);
				if (error) return error;
			} else {
				break;
			}
		} else if ((token->type == AZO_TOKEN_LEFT_PARENTHESIS) && (left_precedence > AZO_PRECEDENCE_FUNCTION)) {
			/* Function call */
			error = parse_function_call (parser, token);
			if (error) return error;
		} else if ((token->type == AZO_TOKEN_LEFT_BRACKET) && (left_precedence > AZO_PRECEDENCE_ARRAY)) {
			/* Array */
			error = parse_array_element (parser, token);
			if (error) return error;
		} else {
			/* Everything else terminates expression */
			break;
		}
	}
	return ERROR_NONE;
}

static unsigned int
azo_parser_parse_prefix_expression (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *right;
	unsigned int start, op, subtype, error;
	/* Unary prefix operators are processed here */
	/* Parse next expression to stack */
	start = token->start;
	op = AZO_TOKEN_OPERATOR_CODE (token);
	switch (op) {
	case AZO_OPERATOR_PLUSPLUS:
		subtype = PREFIX_INCREMENT;
		break;
	case AZO_OPERATOR_MINUSMINUS:
		subtype = PREFIX_DECREMENT;
		break;
	case AZO_OPERATOR_PLUS:
		subtype = PREFIX_PLUS;
		break;
	case AZO_OPERATOR_MINUS:
		subtype = PREFIX_MINUS;
		break;
	case AZO_OPERATOR_NOT:
		subtype = PREFIX_NOT;
		break;
	case AZO_OPERATOR_TILDE:
		subtype = PREFIX_TILDE;
		break;
	default:
		/* Binary or tertiary operator at first position of expression */
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		return ERROR_SYNTAX;
	}
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_UNARY);
	if (error) return error;
	right = parser_detach_last (parser);
	expr = azo_expression_new (EXPRESSION_PREFIX, subtype, start, right->end);
	expr->children = right;
	parser_append (parser, expr);
	/* fixme: Allowed next - operator */
	return ERROR_NONE;
}

static unsigned int
azo_parser_parse_assignment_expression (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *right;
	unsigned int result, subtype;
	subtype = AZO_TOKEN_OPERATOR_CODE (token);
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_ASSIGN);
	if (result) return result;
	right = parser_detach_last (parser);
	left = parser_detach_last (parser);
	if (!left || !right) return ERROR_SYNTAX;
	expr = NULL;
	if (subtype == AZO_OPERATOR_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_PLUSASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_PLUS, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_MINUSASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_MINUS, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_SLASHASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SLASH, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_STARASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_STAR, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_PERCENT_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_PERCENT, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_SHIFT_LEFT_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SHIFT_LEFT, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_SHIFT_RIGHT_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SHIFT_RIGHT, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_AND_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_AND, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_OR_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_OR, left->start, right->end);
	} else if (subtype == AZO_OPERATOR_CARET_ASSIGN) {
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_XOR, left->start, right->end);
	}
	expr->children = left;
	left->next = right;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* Parse operator and append expression to parser
*
* Current token is operator
* After completing token points after the end of RHS expression
*/

static unsigned int
parse_operator (AZOParser *parser, AZOToken *token)
{
	AZOExpression *left, *right, *expr, *tmp;
	unsigned int subtype, precendence;
	unsigned int end, error;

	if (azo_operator_is_assignment (AZO_TOKEN_OPERATOR_CODE (token))) {
		return azo_parser_parse_assignment_expression (parser, token);
	}

	subtype = AZO_TOKEN_OPERATOR_CODE (token);
	precendence = azo_operator_get_precedence (subtype, 1);
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;

	switch (subtype) {
	case AZO_OPERATOR_DOT:
		/* Member reference */
		error = azo_parser_parse_member (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		if (right->type == EXPRESSION_FUNCTION) {
			/* value.function construct, left is already consumed */
			parser_append (parser, right);
		} else if (right->type == EXPRESSION_REFERENCE) {
			/* ref.ref construct */
			left = parser_detach_last (parser);
			if (!left || !right) return ERROR_SYNTAX;
			expr = azo_expression_new (EXPRESSION_REFERENCE, REFERENCE_MEMBER, left->start, right->end);
			expr->children = left;
			left->next = right;
			parser_append (parser, expr);
		} else {
			return ERROR_SYNTAX;
		}
		return ERROR_NONE;
	case AZO_OPERATOR_ARROW:
		/* No arrow at moment */
		return ERROR_SYNTAX;
	case AZO_OPERATOR_COMMA:
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		if (!left || !right) return ERROR_SYNTAX;
		expr = azo_expression_new (EXPRESSION_COMMA, EXPRESSION_GENERIC, left->start, right->end);
		expr->children = left;
		left->next = right;
		parser_append (parser, expr);
		return ERROR_NONE;
	case AZO_OPERATOR_ASSIGN:
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		if (!left || !right) return ERROR_SYNTAX;
		expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN, left->start, right->end);
		expr->children = left;
		left->next = right;
		parser_append (parser, expr);
		return ERROR_NONE;
	case AZO_OPERATOR_EQUAL:
	case AZO_OPERATOR_NE:
	case AZO_OPERATOR_GE:
	case AZO_OPERATOR_GT:
	case AZO_OPERATOR_LE:
	case AZO_OPERATOR_LT:
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		if (!left || !right) return ERROR_SYNTAX;
		expr = NULL;
		if (subtype == AZO_OPERATOR_EQUAL) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_E, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_NE) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_NE, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_GE) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_LE, left->start, right->end);
			tmp = left;
			left = right;
			right = tmp;
		} else if (subtype == AZO_OPERATOR_GT) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_LT, left->start, right->end);
			tmp = left;
			left = right;
			right = tmp;
		} else if (subtype == AZO_OPERATOR_LE) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_LE, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_LT) {
			expr = azo_expression_new (EXPRESSION_COMPARISON, COMPARISON_LT, left->start, right->end);
		}
		expr->children = left;
		left->next = right;
		parser_append (parser, expr);
		return ERROR_NONE;
		/* Arithmetic */
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
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		if (!left || !right) return ERROR_SYNTAX;
		expr = NULL;
		if (subtype == AZO_OPERATOR_PLUS) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_PLUS, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_MINUS) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_MINUS, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SLASH) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_SLASH, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_STAR) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_STAR, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_PERCENT) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_PERCENT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SHIFT_LEFT) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_SHIFT_LEFT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SHIFT_RIGHT) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_SHIFT_RIGHT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_ANDAND) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_ANDAND, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_AND) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_AND, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_OROR) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_OROR, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_OR) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_OR, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_CARET) {
			expr = azo_expression_new (EXPRESSION_BINARY, ARITHMETIC_CARET, left->start, right->end);
		}
		expr->children = left;
		left->next = right;
		parser_append (parser, expr);
		return ERROR_NONE;
		/* Suffix */
	case AZO_OPERATOR_PLUSPLUS:
		left = parser_detach_last (parser);
		if (!left) return ERROR_SYNTAX;
		expr = azo_expression_new (EXPRESSION_SUFFIX, SUFFIX_INCREMENT, left->start, end);
		expr->children = left;
		parser_append (parser, expr);
		return ERROR_NONE;
	case AZO_OPERATOR_MINUSMINUS:
		left = parser_detach_last (parser);
		if (!left) return ERROR_SYNTAX;
		expr = azo_expression_new (EXPRESSION_SUFFIX, SUFFIX_DECREMENT, left->start, end);
		expr->children = left;
		parser_append (parser, expr);
		return ERROR_NONE;
		/* Arithmetic assignment */
	case AZO_OPERATOR_PLUSASSIGN:
	case AZO_OPERATOR_MINUSASSIGN:
	case AZO_OPERATOR_SLASHASSIGN:
	case AZO_OPERATOR_STARASSIGN:
	case AZO_OPERATOR_PERCENT_ASSIGN:
	case AZO_OPERATOR_SHIFT_LEFT_ASSIGN:
	case AZO_OPERATOR_SHIFT_RIGHT_ASSIGN:
	case AZO_OPERATOR_AND_ASSIGN:
	case AZO_OPERATOR_OR_ASSIGN:
	case AZO_OPERATOR_CARET_ASSIGN:
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		if (!left || !right) return ERROR_SYNTAX;
		expr = NULL;
		if (subtype == AZO_OPERATOR_PLUSASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_PLUS, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_MINUSASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_MINUS, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SLASHASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SLASH, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_STARASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_STAR, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_PERCENT_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_PERCENT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SHIFT_LEFT_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SHIFT_LEFT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_SHIFT_RIGHT_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_SHIFT_RIGHT, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_AND_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_AND, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_OR_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_OR, left->start, right->end);
		} else if (subtype == AZO_OPERATOR_CARET_ASSIGN) {
			expr = azo_expression_new (EXPRESSION_ASSIGN, ASSIGN_XOR, left->start, right->end);
		}
		expr->children = left;
		left->next = right;
		parser_append (parser, expr);
		return ERROR_NONE;
		/* Question */
	case AZO_OPERATOR_QUESTION:
	case AZO_OPERATOR_COLON:
		return ERROR_SYNTAX;
	default:
		break;
	}
	return ERROR_SYNTAX;
}

/*
* Parse is|implements and append expression to parser
*
* Current token is operator
* After completing token points after the end of RHS expression
*/

static unsigned int
parse_type_operator (AZOParser *parser, AZOToken *token)
{
	AZOExpression *left, *right, *expr;
	unsigned int op_type, end, error;

	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_IS)) {
		op_type = 0;
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_IMPLEMENTS)) {
		op_type = 1;
	} else {
		return ERROR_SYNTAX;
	}
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_TYPE);
	if (error) return error;
	right = parser_detach_last (parser);
	left = parser_detach_last (parser);
	if (!left || !right) return ERROR_SYNTAX;
	if (op_type == 0) {
		expr = azo_expression_new (AZO_EXPRESSION_TEST, AZO_TYPE_IS, left->start, right->end);
	} else {
		expr = azo_expression_new (AZO_EXPRESSION_TEST, AZO_TYPE_IMPLEMENTS, left->start, right->end);
	}
	expr->children = left;
	left->next = right;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* Parse list of function arguments into LIST expression
*   (AGUMENT[,ARGUMENT])
*
* Current token points to the opening brace
*/

static unsigned int
parse_list (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr;
	unsigned int need_separator;
	unsigned int start, error;
	start = token->start;
	/* ( */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;

	expr = azo_expression_new (EXPRESSION_LIST, EXPRESSION_GENERIC, start, token->end);
	parser_append (parser, expr);
	parser_push (parser);
	need_separator = 0;
	while (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) {
		if (need_separator) {
			if (token->type != AZO_TOKEN_COMMA) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_expression_free_tree (expr);
				return ERROR_SYNTAX;
			}
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_expression_free_tree (expr);
				return ERROR_UNEXPECTED_EOF;
			}
		}
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_expression_free_tree (expr);
			return error;
		}
		need_separator = 1;
		if (azo_tokenizer_is_eof (&parser->tokenizer)) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_expression_free_tree (expr);
			return ERROR_UNEXPECTED_EOF;
		}
	}
	expr->end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return ERROR_NONE;
}

/*
* Parse function call
*   (ARGUMENTS)
*
* Current token points to the opening brace
*/

static unsigned int
parse_function_call (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *right;
	unsigned int error;

	error = parse_list (parser, token);
	if (error) {
		return error;
	}
	right = parser_detach_last (parser);
	left = parser_detach_last (parser);
	if (!left || !right) return ERROR_SYNTAX;
	expr = azo_expression_new (EXPRESSION_FUNCTION_CALL, EXPRESSION_GENERIC, left->start, right->start);
	expr->children = left;
	left->next = right;
	parser_append (parser, expr);
	return ERROR_NONE;
}

static unsigned int
parse_argument_definition (AZOParser *parser, AZOToken *token)
{
	AZOExpression *left, *right, *decl;
	unsigned int result, start;
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	start = token->start;
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
	if (result) return result;
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_WORD) {
		AZOExpression *name = azo_expression_new_reference (REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, name);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
	} else {
		left = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, start, start);
		right = parser_detach_last (parser);
	}
	decl = azo_expression_new (EXPRESSION_ARGUMENT_DECLARATION, EXPRESSION_GENERIC, start, token->start);
	decl->children = left;
	left->next = right;
	parser_append (parser, decl);
	return ERROR_NONE;
}

static unsigned int
parse_arguments_definition (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr;
	unsigned int need_separator;
	unsigned int start, result;
	start = token->start;
	/* ( */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;

	expr = azo_expression_new (EXPRESSION_LIST, EXPRESSION_GENERIC, start, token->end);
	parser_append (parser, expr);
	parser_push (parser);

	need_separator = 0;
	while (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) {
		if (need_separator) {
			if (token->type != AZO_TOKEN_COMMA) return ERROR_SYNTAX;
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		}
		result = parse_argument_definition (parser, token);
		if (result) return result;
		need_separator = 1;
	}
	expr->end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return ERROR_NONE;
}

static unsigned int
parse_signature_definition (AZOParser *parser, AZOToken *token, unsigned int *is_class)
{
	AZOExpression *expr;
	unsigned int error;
	/* Return type */
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_FUNCTION)) {
		/* function */
		/* We create reference as it should be interpreted as type */
		expr = azo_expression_new_reference (REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	} else if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_VOID)) {
		expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_VOID, token->start, token->end);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	} else if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		expr = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, token->start, token->end);
		parser_append (parser, expr);
	} else if (token->type == AZO_TOKEN_WORD) {
		/* fixme: We should exclude keywords here so function in 'function is any' construct is parsed as class */
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_FUNCTION);
		if (error) return error;
	} else if (token->type == AZO_TOKEN_RIGHT_PARENTHESIS) {
		*is_class = 1;
		return ERROR_NONE;
	} else if (token->type == AZO_TOKEN_RIGHT_BRACKET) {
		*is_class = 1;
		return ERROR_NONE;
	} else if (token->type == AZO_TOKEN_OPERATOR) {
		*is_class = 1;
		return ERROR_NONE;
	} else {
		return ERROR_SYNTAX;
	}
	/* Arguments */
	if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		error = parse_arguments_definition (parser, token);
		if (error) return error;
	} else {
		return ERROR_SYNTAX;
	}
	*is_class = 0;
	return ERROR_NONE;
}

/*
* Parse function definition
*   function [TYPE|VOID] (ARGUMENTS) STATEMENT
*
* Current token points to function keyword
* If member, object reference is already pushed into stack
*/

static unsigned int
azo_parser_parse_function_definition (AZOParser *parser, AZOToken *token, unsigned int has_return_type, unsigned int is_member)
{
	AZOExpression *expr;
	unsigned int start, end, error, is_class;
	start = token->start;
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = parse_signature_definition (parser, token, &is_class);
	if (error) return error;
	if (is_class) {
		/* function bareword */
		/* fixme: */
		expr = azo_expression_new (EXPRESSION_REFERENCE, REFERENCE_VARIABLE, start, end);
		AZString *f = az_string_new ((const unsigned char *) "function");
		az_packed_value_set_string (&expr->value, f);
		parser_append (parser, expr);
		//if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		return ERROR_NONE;
	}
	/* Statement/Block */
	if (token->type == AZO_TOKEN_LEFT_BRACE) {
		/* Definition */
		error = azo_parser_parse_sentence (parser, token);
		if (error) return error;
	} else {
		return ERROR_SYNTAX;
	}
	if (is_member) {
		AZOExpression *obj, *type, *args, *body;
		body = parser_detach_last (parser);
		args = parser_detach_last (parser);
		type = parser_detach_last (parser);
		obj = parser_detach_last (parser);
		expr = azo_expression_new (EXPRESSION_FUNCTION, FUNCTION_MEMBER, obj->start, body->end);
		expr->children = type;
		type->next = obj;
		obj->next = args;
		args->next = body;
	} else {
		AZOExpression *type, *args, *body;
		body = parser_detach_last (parser);
		args = parser_detach_last (parser);
		type = parser_detach_last (parser);
		expr = azo_expression_new (EXPRESSION_FUNCTION, FUNCTION_STATIC, type->start, body->end);
		expr->children = type;
		type->next = args;
		args->next = body;
	}
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* Parse array element
*   [EXPRESSION]
*
* Current token is at opening bracket
* Array reference is last element in stack
* Result - array element expression as last element in stack
*/

static unsigned int
parse_array_element (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *right;
	unsigned int start, end, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_RIGHT_BRACKET) {
		/* Empty element - i.e. declaration */
		right = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, token->start, token->start);
	} else {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (error) return error;
		right = parser_detach_last (parser);
	}
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_BRACKET) return ERROR_SYNTAX;
	end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	left = parser_detach_last (parser);
	if (!left || !right) return ERROR_SYNTAX;
	expr = azo_expression_new (EXPRESSION_ARRAY_ELEMENT, EXPRESSION_GENERIC, start, end);
	expr->children = left;
	left->next = right;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* Parse array literal
*   {[EXPRESSION][,EXPRESSION]...}
*
* Current token is at opening brace
*/

static unsigned int
parse_array_literal (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr;
	unsigned int start, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	expr = azo_expression_new (EXPRESSION_LITERAL_ARRAY, EXPRESSION_GENERIC, token->start, token->end);
	parser_append (parser, expr);
	parser_push (parser);
	while (token->type != AZO_TOKEN_RIGHT_BRACE) {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) return error;
		if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
		if (token->type == AZO_TOKEN_COMMA) {
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		} else if (token->type != AZO_TOKEN_RIGHT_BRACE) {
			return ERROR_SYNTAX;
		}
	}
	expr->end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return ERROR_NONE;
}

/*
* Parse constructor
*   new EXPRESSION(ARGUMENTS)
*
* Current token is at keyword "new"
*/

static unsigned int
parse_new (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *right;
	unsigned int start, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, 3);
	if (error) return error;
	right = parser_detach_last (parser);
	if (right->type != EXPRESSION_FUNCTION_CALL) return ERROR_SYNTAX;
	expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_NEW, start, right->end);
	expr->children = right->children;
	azo_expression_free (right);
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* while (CONDITION) STATEMENT
*
* Current token is at keyword while
*/

static unsigned int
parse_while (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *middle, *right, *block;
	unsigned int start, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return ERROR_SYNTAX;
	/* Expression */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	/* ) */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return ERROR_SYNTAX;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	block = parser_detach_last (parser);
	middle = parser_detach_last (parser);
	if (!middle || !block) return ERROR_SYNTAX;
	left = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, middle->start, middle->start);
	right = azo_expression_new (AZO_TERM_EMPTY, EXPRESSION_GENERIC, middle->start, middle->start);
	expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_FOR, start, block->end);
	expr->children = left;
	left->next = middle;
	middle->next = right;
	right->next = block;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* for (INITIALIZATION; CONDITION; STEP) STATEMENT
*
* Current token is at keyword
*/

static unsigned int
parse_for (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *left, *middle, *right, *block;
	unsigned int start, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return ERROR_SYNTAX;
	/* Initialization */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_step_statement (parser, token);
	if (error) return error;
	/* ; */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) return ERROR_SYNTAX;
	/* Condition */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	/* ; */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) return ERROR_SYNTAX;
	/* Step */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_silent_statement (parser, token);
	if (error) return error;
	/* ) */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return ERROR_SYNTAX;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	block = parser_detach_last (parser);
	right = parser_detach_last (parser);
	middle = parser_detach_last (parser);
	left = parser_detach_last (parser);
	if (!left || !middle || !right || !block) return ERROR_SYNTAX;
	expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_FOR, start, block->end);
	expr->children = left;
	left->next = middle;
	middle->next = right;
	right->next = block;
	parser_append (parser, expr);
	return ERROR_NONE;
}

/*
* if (EXPRESSION) STATEMENT [ ELSE STATEMENT ]
*
* Current token is at keyword
*/

static unsigned int
parse_if (AZOParser *parser, AZOToken *token)
{
	AZOExpression *expr, *cond, *if_true, *if_false;
	unsigned int start, end, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return ERROR_SYNTAX;
	/* Expression */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	cond = parser_detach_last (parser);
	/* ) */
	if (azo_tokenizer_is_eof (&parser->tokenizer)) return ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return ERROR_CLOSING_PARENTHESIS_MISSING;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	if_true = parser_detach_last (parser);
	end = if_true->end;
	/* Potential else */
	if_false = NULL;
	if (azo_token_is_keyword (parser->src, token, AZO_KEYWORD_ELSE)) {
		/* Statement/Block */
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return ERROR_UNEXPECTED_EOF;
		error = azo_parser_parse_sentence (parser, token);
		if (error) return error;
		if_false = parser_detach_last (parser);
		end = if_false->end;
	}
	if (!cond || !if_true) return ERROR_SYNTAX;
	expr = azo_expression_new (EXPRESSION_KEYWORD, AZO_KEYWORD_IF, start, end);
	expr->children = cond;
	cond->next = if_true;
	if_true->next = if_false;
	parser_append (parser, expr);
	return ERROR_NONE;
}

