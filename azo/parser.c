#define __AZO_PARSER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arikkei/arikkei-utils.h>

#include <az/string.h>
#include <az/extend.h>

#include <azo/keyword.h>
#include <azo/operator.h>
#include <azo/parser.h>

static const char *parser_errors[] = {
	"No error",
	"Unexpected EOF",
	"Syntax error",
	"End of block missing",
	"Missing semicolon",
	"Closing brace missing",
	"Invalid start of expression",
	"Too many errors"
};

static unsigned int parse_program (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_sentences (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_sentence (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_block (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_line (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_return (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_step_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_declaration (AZOParser *parser, AZOToken *token, uint16_t flags);
static unsigned int azo_parser_parse_single_declaration (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_silent_statement (AZOParser *parser, AZOToken *token);
static unsigned int azo_parser_parse_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_parenthesized_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_naked_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_continue_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence);
static unsigned int azo_parser_parse_prefix_expression (AZOParser *parser, AZOToken *token);
static unsigned int parse_assignment_statement (AZOParser *parser, AZOToken *token);
#ifdef HAS_FUNCTION_KEYWORD
static unsigned int parse_function_definition (AZOParser *parser, AZOToken *token, unsigned int is_member);
#endif

static void parser_report_error (AZOParser *parser, const AZOToken *token, unsigned int errval);

static unsigned int azo_parser_type = 0;
static AZOParserClass *azo_parser_class = NULL;

unsigned int
azo_parser_get_type (void)
{
	if (!azo_parser_type) {
		azo_parser_class = (AZOParserClass *) az_register_type (&azo_parser_type, (const unsigned char *) "AZOParser", AZ_TYPE_BLOCK, sizeof (AZOParserClass), sizeof (AZOParser), AZ_FLAG_ZERO_MEMORY, 0, 0,
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
	parser->current = NULL;
	parser->n_errors = 0;
}

void
azo_parser_release (AZOParser *parser)
{
	azo_tokenizer_release (&parser->tokenizer);
	azo_source_unref(parser->src);
}

AZONode *
azo_parser_parse (AZOParser *parser)
{
	AZOToken token = {0};
	unsigned int result;

	if (azo_tokenizer_get_next_token (&parser->tokenizer, &token)) {
		result = parse_program (parser, &token);
		/* Report at top level only if the error was not logged during recovery */
		if (result && !parser->n_errors) {
			parser_report_error (parser, &token, result);
		}
		if (azo_tokenizer_has_next_token(&parser->tokenizer, &token)) {
			/* Either parsing error or unexpected end of block */
		}
	}

	return parser->current;
}

static void
parser_report_error (AZOParser *parser, const AZOToken *token, unsigned int errval)
{
	/* Log the error into the parser error list */
	if (parser->n_errors < AZO_PARSER_MAX_ERRORS) {
		AZOParserError *error = &parser->errors[parser->n_errors++];
		error->code = errval;
		error->token = *token;
		strncpy (error->message, parser_errors[errval], sizeof (error->message) - 1);
		error->message[sizeof (error->message) - 1] = 0;
	}
	fprintf (stderr, "%s at line ", parser_errors[errval]);
	if (!azo_tokenizer_is_eof (&parser->tokenizer, token)) {
		unsigned int first, last;
		if (azo_source_find_line_range (parser->src, token->start, token->end, &first, &last)) {
			fprintf (stderr, "%d near ", first);
			azo_tokenizer_print_token (&parser->tokenizer, token, stderr);
			fprintf (stderr, "\n");
			/* fixme: Keep track of the last processed token */
			azo_source_print_lines (parser->src, first, last + 1);
		}
	} else {
		fprintf (stderr, "near EOF\n");
		azo_source_ensure_lines(parser->src);
		azo_source_print_lines (parser->src, parser->src->n_lines - 1, parser->src->n_lines);
	}
}

const char *
azo_parser_error_get_message (unsigned int code)
{
	if (code >= AZO_PARSER_NUM_ERRORS) return "Unknown error";
	return parser_errors[code];
}

AZONode *
parser_peek_last (AZOParser *parser)
{
	AZONode *last;
	arikkei_return_val_if_fail (parser->current != NULL, NULL);
	arikkei_return_val_if_fail (parser->current->children != NULL, NULL);
	last = parser->current->children;
	while (last->next) last = last->next;
	return last;
}

/* Append expression to the rightmost position at the current level */

static void
parser_append (AZOParser *parser, AZONode *expr)
{
	expr->parent = parser->current;
	if (!parser->current->children) {
		parser->current->children = expr;
	} else {
		AZONode *last;
		last = parser_peek_last (parser);
		last->next = expr;
	}
}

/* Detach and return the rightmost expression at the current level */
AZONode *
parser_detach_last (AZOParser *parser)
{
	AZONode *prev, *last;
	arikkei_return_val_if_fail (parser->current != NULL, NULL);
	arikkei_return_val_if_fail (parser->current->children != NULL, NULL);
	prev = NULL;
	last = parser->current->children;
	while (last->next) {
		prev = last;
		last = last->next;
	}
	if (prev) {
		prev->next = NULL;
	} else {
		parser->current->children = NULL;
	}
	return last;
}

/* Append expression and push it as the new parent */

static void
parser_push (AZOParser *parser, AZONode *expr)
{
	parser_append (parser, expr);
	parser->current = expr;
}

/* Pop current and go to previous parent */

static void
parser_pop (AZOParser *parser)
{
	arikkei_return_if_fail (parser->current->parent != NULL);
	parser->current = parser->current->parent;
}

static unsigned int
term_is_silent_statement (AZOTerm *expr)
{
	if (expr->type == AZO_TERM_EMPTY) return 1;
	if (expr->type == AZO_TERM_ASSIGN) return 1;
	if (expr->type == AZO_TERM_FUNCTION_CALL) return 1;
	if (expr->type == AZO_TERM_PREFIX) {
		if (expr->subtype == AZO_TERM_PREFIX_INCREMENT) return 1;
		if (expr->subtype == AZO_TERM_PREFIX_DECREMENT) return 1;
	} else if (expr->type == AZO_TERM_SUFFIX) {
		if (expr->subtype == AZO_TERM_SUFFIX_INCREMENT) return 1;
		if (expr->subtype == AZO_TERM_SUFFIX_DECREMENT) return 1;
	}
	return 0;
}

/* Whether the term is a nonempty step statement (can be an item of Multi_statement) */

static unsigned int
term_is_nonempty_step_statement (AZOTerm *expr)
{
	if (expr->type == AZO_TERM_DECLARATION_LIST) return 1;
	if (expr->type == AZO_TERM_ASSIGN) return 1;
	if (expr->type == AZO_TERM_FUNCTION_CALL) return 1;
	if (expr->type == AZO_TERM_PREFIX) {
		if (expr->subtype == AZO_TERM_PREFIX_INCREMENT) return 1;
		if (expr->subtype == AZO_TERM_PREFIX_DECREMENT) return 1;
	} else if (expr->type == AZO_TERM_SUFFIX) {
		if (expr->subtype == AZO_TERM_SUFFIX_INCREMENT) return 1;
		if (expr->subtype == AZO_TERM_SUFFIX_DECREMENT) return 1;
	}
	return 0;
}

/* Whether the term is a nonempty silent statement (can be an item of Silent_multi_statement) */

static unsigned int
term_is_nonempty_silent_statement (AZOTerm *expr)
{
	if (expr->type == AZO_TERM_DECLARATION_LIST) return 0;
	return term_is_nonempty_step_statement (expr);
}

/* Create a reference node with an explicit name (for multi-word type names) */

static AZONode *
new_named_reference (unsigned int subtype, unsigned int start, unsigned int end, const char *name)
{
	AZONode *expr = azo_node_new (AZO_TERM_REFERENCE, subtype, start, end);
	AZString *str = az_string_new ((const uint8_t *) name);
	az_packed_value_transfer_string (&expr->value, str);
	return expr;
}

/* Whether the token can start an expression (used to disambiguate primitive casts from parenthesized class values) */

static unsigned int
token_can_start_expression (AZOParser *parser, const AZOToken *token)
{
	if (AZO_TOKEN_IS_WORD (token)) return 1;
	if (AZO_TOKEN_IS_NUMBER (token)) return 1;
	if (token->type == AZO_TOKEN_TEXT) return 1;
	if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) return 1;
	if (token->type == AZO_TOKEN_LEFT_BRACE) return 1;
	/* Prefix operators */
	if (AZO_TOKEN_IS_OPERATOR (token)) return azo_token_get_prefix_term (token) >= 0;
	return 0;
}

static unsigned int parse_operator (AZOParser *parser, AZOToken *token);
static unsigned int parse_type_operator (AZOParser *parser, AZOToken *token);
static unsigned int parse_for (AZOParser *parser, AZOToken *token);
static unsigned int parse_while (AZOParser *parser, AZOToken *token);
static unsigned int parse_do (AZOParser *parser, AZOToken *token);
static unsigned int parse_if (AZOParser *parser, AZOToken *token);
static unsigned int parse_function_call (AZOParser *parser, AZOToken *token);
static unsigned int parse_new (AZOParser *parser, AZOToken *token);
static unsigned int parse_array_literal (AZOParser *parser, AZOToken *token);
static unsigned int parse_array_element (AZOParser *parser, AZOToken *token);
static unsigned int parse_multi_statement (AZOParser *parser, AZOToken *token, unsigned int terminator, unsigned int silent_only);
static unsigned int parse_argument_definition (AZOParser *parser, AZOToken *token, unsigned int *has_type);
static unsigned int continue_arguments_definition (AZOParser *parser, AZOToken *token, unsigned int *any_typed, unsigned int *any_untyped);
static unsigned int parse_lambda (AZOParser *parser, AZOToken *token, unsigned int left_precedence, unsigned int start, unsigned int type_allowed);

/*
 * Program:
 *   Sentences
 */

static unsigned int
parse_program (AZOParser *parser, AZOToken *token)
{
	AZONode *expr = azo_node_new (AZO_TERM_PROGRAM, AZO_TERM_GENERIC, token->start, token->end);
	parser->current = expr;
	unsigned int result = azo_parser_parse_sentences (parser, token);
	expr->term.end = token->start;
	if (result) return result;
	if (token->type != AZO_TOKEN_EOF) {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	if (parser->current != expr) {
		fprintf(stderr, "parse_program: Internal error\n");
	}
	return result;
}

/*
 * Skip garbage after a syntax error until a trustworthy anchor:
 * ';' (consumed), '}' (kept) or EOF
 * Braces are balanced so a garbage block/literal does not eat the
 * closing brace of the enclosing block.
 * ';' anchors at any depth because it cannot legally appear inside
 * parentheses/brackets (the for header is the only exception and
 * re-synchronizing there is still better than eating the block end)
 */

static void
parser_synchronize (AZOParser *parser, AZOToken *token)
{
	unsigned int brace_depth = 0;
	while (token->type != AZO_TOKEN_EOF) {
		if (!brace_depth) {
			if (token->type == AZO_TOKEN_SEMICOLON) {
				/* Consume and stop */
				azo_tokenizer_get_next_token (&parser->tokenizer, token);
				return;
			}
			if (token->type == AZO_TOKEN_RIGHT_BRACE) {
				/* Keep and stop */
				return;
			}
		}
		if (token->type == AZO_TOKEN_LEFT_BRACE) {
			brace_depth += 1;
		} else if (token->type == AZO_TOKEN_RIGHT_BRACE) {
			brace_depth -= 1;
		}
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return;
	}
}

/*
 * Sentences:
 *   [Sentence ...]
 */

static unsigned int
azo_parser_parse_sentences (AZOParser *parser, AZOToken *token)
{
	unsigned int result = AZO_PARSER_ERROR_NONE;
	/* Sentence list is terminated either by EOF or closing BRACE */
	while ((token->type != AZO_TOKEN_EOF) && (token->type != AZO_TOKEN_RIGHT_BRACE)) {
		unsigned int n_errors_before, lresult;
		if (parser->n_errors >= AZO_PARSER_MAX_ERRORS) {
			/* Too many errors - abort parsing */
			parser_report_error (parser, token, AZO_PARSER_ERROR_TOO_MANY_ERRORS);
			if (!result) result = AZO_PARSER_ERROR_TOO_MANY_ERRORS;
			break;
		}
		n_errors_before = parser->n_errors;
		lresult = azo_parser_parse_sentence (parser, token);
		if (lresult) {
			if (parser->n_errors == n_errors_before) {
				/* Error was not handled inside - report and recover at this level */
				unsigned int err_start = token->start;
				AZONode *poison;
				parser_report_error (parser, token, lresult);
				parser_synchronize (parser, token);
				poison = azo_node_new (AZO_TERM_INVALID, AZO_TERM_GENERIC, err_start, token->start);
				parser_append (parser, poison);
			}
			/* Keep parsing the remaining sentences but return the first error */
			if (!result) result = lresult;
		}
	}
	return result;
}

/*
 * Sentence:
 *   Block
 *   Line
 *   for
 *   while
 *   do
 *   if
 */

static unsigned int
azo_parser_parse_sentence (AZOParser *parser, AZOToken *token)
{
	if (token->type == AZO_TOKEN_LEFT_BRACE) {
		return azo_parser_parse_block (parser, token);
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_FOR, parser->src)) {
		return parse_for (parser, token);
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_WHILE, parser->src)) {
		return parse_while (parser, token);
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_DO, parser->src)) {
		return parse_do (parser, token);
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_IF, parser->src)) {
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
	AZONode *expr;
	unsigned int result, start, end;
	/* Block */
	start = token->start;
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	expr = azo_node_new (AZO_TERM_BLOCK, AZO_TERM_GENERIC, start, end);
	parser_push (parser, expr);
	result = azo_parser_parse_sentences (parser, token);
	expr->term.end = token->end;
	parser_pop (parser);
	/* Block has to end with "}" (kept by synchronize after inner errors) */
	if (token->type == AZO_TOKEN_EOF) {
		if (!result) result = AZO_PARSER_ERROR_UNEXPECTED_EOF;
		return result;
	}
	if (token->type != AZO_TOKEN_RIGHT_BRACE) {
		if (!result) result = AZO_PARSER_ERROR_END_OF_BLOCK_MISSING;
		return result;
	}
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	return result;
}

/* Move the last statement under a new statement group node and push the group as the current node */

static void
parser_wrap_last_in_statement_group (AZOParser *parser)
{
	AZONode *last = parser_detach_last (parser);
	AZONode *group = azo_node_new (AZO_TERM_STATEMENT_GROUP, AZO_TERM_GENERIC, last->term.start, last->term.end);
	parser_push (parser, group);
	parser_append (parser, last);
}

/*
 * Parse the remaining items of a multi statement (token is at the first comma)
 * Items are appended to the current node (a statement group)
 */

static unsigned int
parse_remaining_multi_statement (AZOParser *parser, AZOToken *token, unsigned int silent_only)
{
	while (token->type == AZO_TOKEN_COMMA) {
		AZONode *last = parser_peek_last (parser);
		unsigned int result;
		/* Only nonempty statements of the appropriate kind can be comma-separated */
		if (!last || !(silent_only ? term_is_nonempty_silent_statement (&last->term) : term_is_nonempty_step_statement (&last->term))) return AZO_PARSER_ERROR_SYNTAX;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		/* Items cannot be empty */
		if ((token->type == AZO_TOKEN_COMMA) || (token->type == AZO_TOKEN_SEMICOLON) || (token->type == AZO_TOKEN_RIGHT_PARENTHESIS)) return AZO_PARSER_ERROR_SYNTAX;
		if (silent_only) {
			result = azo_parser_parse_silent_statement (parser, token);
		} else {
			result = azo_parser_parse_step_statement (parser, token);
		}
		if (result) return result;
	}
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Multi_statement / Silent_multi_statement:
 *   Nonempty_statement [, Nonempty_statement ...]
 *
 * Parses comma-separated statements into the current node
 * If more than one statement is present they are collected under a STATEMENT_GROUP node
 * After completion token points at the expected terminator (';' or ')'), which is not consumed
 */

static unsigned int
parse_multi_statement (AZOParser *parser, AZOToken *token, unsigned int terminator, unsigned int silent_only)
{
	unsigned int result;
	if (silent_only) {
		result = azo_parser_parse_silent_statement (parser, token);
	} else {
		result = azo_parser_parse_step_statement (parser, token);
	}
	if (result) return result;
	if (token->type == AZO_TOKEN_COMMA) {
		AZONode *group;
		parser_wrap_last_in_statement_group (parser);
		group = parser->current;
		result = parse_remaining_multi_statement (parser, token, silent_only);
		if (!result) group->term.end = parser_peek_last (parser)->term.end;
		parser_pop (parser);
		if (result) return result;
	}
	if (token->type != terminator) {
		return (terminator == AZO_TOKEN_SEMICOLON) ? AZO_PARSER_ERROR_SEMICOLON_MISSING : AZO_PARSER_ERROR_CLOSING_PARENTHESIS_MISSING;
	}
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Line:
 *   Statement ;
 *   Multi_statement ;
 */

/* Whether the token clearly starts a new sentence or ends the block
 * (used for missing semicolon repair) */

static unsigned int
token_is_sentence_boundary (AZOParser *parser, const AZOToken *token)
{
	if ((token->type == AZO_TOKEN_EOF) || (token->type == AZO_TOKEN_RIGHT_BRACE) || (token->type == AZO_TOKEN_LEFT_BRACE)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_FOR, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_WHILE, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_DO, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_IF, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_ELSE, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_RETURN, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_BREAK, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_CONTINUE, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_DEBUG, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_STATIC, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_CONST, parser->src)) return 1;
	if (azo_token_is_keyword (token, AZO_KEYWORD_FINAL, parser->src)) return 1;
	return 0;
}

static unsigned int
azo_parser_parse_line (AZOParser *parser, AZOToken *token)
{
	unsigned int result;
	result = azo_parser_parse_statement (parser, token);
	if (result) return result;
	/* Multi_statement: nonempty step statements separated by commas */
	if (token->type == AZO_TOKEN_COMMA) {
		AZONode *first = parser_peek_last (parser);
		AZONode *group;
		/* Only nonempty step statements can be comma-separated (i.e. not return, break, continue or empty) */
		if (!first || !term_is_nonempty_step_statement (&first->term)) return AZO_PARSER_ERROR_SYNTAX;
		parser_wrap_last_in_statement_group (parser);
		group = parser->current;
		result = parse_remaining_multi_statement (parser, token, 0);
		if (!result) group->term.end = parser_peek_last (parser)->term.end;
		parser_pop (parser);
		if (result) return result;
	}
	/* Parse expression keeps token at closing token */
	if (token->type != AZO_TOKEN_SEMICOLON) {
		/* Repair missing semicolon if the next token clearly starts a new sentence */
		if (token_is_sentence_boundary (parser, token)) {
			/* Log the error but continue as if the semicolon was there */
			parser_report_error (parser, token, AZO_PARSER_ERROR_SEMICOLON_MISSING);
			return AZO_PARSER_ERROR_NONE;
		}
		return AZO_PARSER_ERROR_SEMICOLON_MISSING;
	}
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	return result;
}

/*
 * Statement:
 *   Step_statement
 *   return
 *   break
 *   continue
 *   debug (only if HAS_DEBUG_KEYWORD is defined - not part of the language spec)
 */

static unsigned int
azo_parser_parse_statement (AZOParser *parser, AZOToken *token)
{
	unsigned int result;
	/* Return */
	if (azo_token_is_keyword(token, AZO_KEYWORD_RETURN, parser->src)) {
		return azo_parser_parse_return (parser, token);
	} else if (azo_token_is_keyword(token, AZO_KEYWORD_BREAK, parser->src) || azo_token_is_keyword(token, AZO_KEYWORD_CONTINUE, parser->src)) {
		AZONode *expr = azo_node_new (AZO_TERM_KEYWORD, azo_token_get_keyword (token, parser->src), token->start, token->end);
		parser_append (parser, expr);
		/* The following token may be EOF - the semicolon check (and repair) is done by parse_line */
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		return AZO_PARSER_ERROR_NONE;
	}
#ifdef HAS_DEBUG_KEYWORD
	else if (azo_token_is_keyword (token, AZO_KEYWORD_DEBUG, parser->src)) {
		AZONode *expr = azo_node_new (AZO_TERM_KEYWORD, AZO_KEYWORD_DEBUG, token->start, token->end);
		parser_append (parser, expr);
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		return AZO_PARSER_ERROR_NONE;
	}
#endif
	result = azo_parser_parse_step_statement (parser, token);
	return result;
}

/*
* Return:
*   return [Expression]
*/

static unsigned int
azo_parser_parse_return (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *val = NULL;
	unsigned int start, end, result;
	start = token->start;
	end = token->end;
	/* After return the token may be EOF */
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	/* The expression can be omitted before ';', '}' or EOF (nothing else can start a bare return) */
	if ((token->type != AZO_TOKEN_SEMICOLON) && (token->type != AZO_TOKEN_RIGHT_BRACE) && (token->type != AZO_TOKEN_EOF)) {
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (result) return result;
		val = parser_detach_last (parser);
		end = val->term.end;
	}
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_RETURN, start, end, 1, val);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
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
	uint16_t flags = 0;
	unsigned int result;
	/* Qualifiers can appear in any order but only once each */
	while (1) {
		unsigned int keyword = azo_token_get_keyword (token, parser->src);
		if (keyword == AZO_KEYWORD_STATIC) {
			if (flags & AZO_TERM_FLAG_STATIC) return AZO_PARSER_ERROR_SYNTAX;
			flags |= AZO_TERM_FLAG_STATIC;
		} else if (keyword == AZO_KEYWORD_CONST) {
			if (flags & AZO_TERM_FLAG_CONST) return AZO_PARSER_ERROR_SYNTAX;
			flags |= AZO_TERM_FLAG_CONST;
		} else if (keyword == AZO_KEYWORD_FINAL) {
			if (flags & AZO_TERM_FLAG_FINAL) return AZO_PARSER_ERROR_SYNTAX;
			flags |= AZO_TERM_FLAG_FINAL;
		} else {
			break;
		}
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	}
	if ((token->type == AZO_TOKEN_SEMICOLON) || (token->type == AZO_TOKEN_RIGHT_PARENTHESIS)) {
		/* EMPTY */
		AZONode *expr;
		if (flags) return AZO_PARSER_ERROR_SYNTAX;
		expr = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
#ifdef HAS_FUNCTION_KEYWORD
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* LEGACY - 'function' used as a type name in a declaration (function f = ...) */
		/* We create reference as it should be interpreted as type */
		AZONode *expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		if ((token->type != AZO_TOKEN_WORD) || (azo_token_get_keyword (token, parser->src) != AZO_KEYWORD_NONE)) return AZO_PARSER_ERROR_SYNTAX;
		return azo_parser_parse_declaration (parser, token, flags);
	}
#endif
	/* Either Declaration or Silent Statement - both start with expression */
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
	if (result) return result;
	/* If a naked word follows it is declaration (keywords cannot be variable names) */
	if ((token->type == AZO_TOKEN_WORD) && (azo_token_get_keyword (token, parser->src) == AZO_KEYWORD_NONE)) {
		return azo_parser_parse_declaration (parser, token, flags);
	} else {
		AZONode *expr;
		if (flags) return AZO_PARSER_ERROR_SYNTAX;
		if (AZO_TOKEN_IS_OPERATOR (token) && azo_operator_is_assignment (AZO_TOKEN_OPERATOR_CODE (token))) {
			/* Assignment */
			return parse_assignment_statement (parser, token);
		}
		expr = parser_peek_last (parser);
		if (!term_is_silent_statement (&expr->term)) return AZO_PARSER_ERROR_SYNTAX;
		return AZO_PARSER_ERROR_NONE;
	}
}

/*
 * Declaration:
 *   Qualifiers Type Single_declaration [, Single_declaration ...]
 *
 * Type expression is already parsed, token is the next after type (should be variable name)
 */

static unsigned int
azo_parser_parse_declaration (AZOParser *parser, AZOToken *token, uint16_t flags)
{
	unsigned int result = AZO_PARSER_ERROR_NONE;
	AZONode *declr, *type, *last;
	/* Create topmost declaration list */
	type = parser_detach_last (parser);
	declr = azo_node_new_with_children (AZO_TERM_DECLARATION_LIST, AZO_TERM_GENERIC, type->term.start, token->end, 1, type);
	declr->term.flags = flags;
	last = type;
	/* The first declaration is guaranteed by the caller (a word follows the type) */
	while (1) {
		AZONode *expr;
		result = azo_parser_parse_single_declaration (parser, token);
		if (result) {
			azo_node_free_tree (declr);
			return result;
		}
		expr = parser_detach_last(parser);
		last->next = expr;
		last = expr;
		declr->term.end = expr->term.end;
		if (token->type != AZO_TOKEN_COMMA) break;
		/* After comma another declaration is required (the following token may be EOF) */
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
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
 * 
 * Token is at variable name
 */

static unsigned int
azo_parser_parse_single_declaration (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *left, *right;
	unsigned int result, end;
	/* Keywords cannot be variable names */
	if ((token->type != AZO_TOKEN_WORD) || (azo_token_get_keyword (token, parser->src) != AZO_KEYWORD_NONE)) return AZO_PARSER_ERROR_SYNTAX;
	left = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
	/* The following token may be EOF */
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	if (token->type == AZO_TOKEN_ASSIGN) {
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			azo_node_free (left);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (result) {
			azo_node_free (left);
			return result;
		}
		right = parser_detach_last (parser);
		end = right->term.end;
	} else {
		right = NULL;
		end = left->term.end;
	}
	expr = azo_node_new_with_children(AZO_TERM_DECLARATION, AZO_TERM_GENERIC, left->term.start, end, 2, left, right);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
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
	AZONode *expr;
	unsigned int result;
	if ((token->type == AZO_TOKEN_SEMICOLON) || (token->type == AZO_TOKEN_RIGHT_PARENTHESIS)) {
		/* EMPTY */
		expr = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (result) return result;
	if (AZO_TOKEN_IS_OPERATOR (token) && azo_operator_is_assignment (AZO_TOKEN_OPERATOR_CODE (token))) {
		/* Assignment */
		return parse_assignment_statement (parser, token);
	}
	expr = parser_peek_last (parser);
	if (!term_is_silent_statement (&expr->term)) return AZO_PARSER_ERROR_SYNTAX;
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Expression:
 *   Parenthesized_expression
 *   Naked_expression
 *
 * Current token is the start of the expression
 * After completing successfully token points past the end of the expression
 * and exactly one node has been appended to the current node (checked by assert)
 */

#ifndef NDEBUG
static unsigned int
count_children (const AZONode *node)
{
	unsigned int n = 0;
	for (const AZONode *child = node->children; child; child = child->next) n += 1;
	return n;
}
#endif

static unsigned int
azo_parser_parse_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	unsigned int result;
#ifndef NDEBUG
	AZONode *current = parser->current;
	unsigned int n_children = count_children (current);
#endif
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		/* Parenthesized_expression */
		result = azo_parser_parse_parenthesized_expression (parser, token, left_precedence);
	} else {
		/* Naked expression */
		result = azo_parser_parse_naked_expression (parser, token, left_precedence);
	}
	/* Invariant: on success exactly one node has been appended to the current node */
	assert (result || ((parser->current == current) && (count_children (current) == n_children + 1)));
	return result;
}

/*
* Parenthesized group:
*   (Expression)                                     (parenthesized expression)
*   (Primitive_type) Expression                      (cast - primitive conversions only)
*   (Primitive_type exact) Expression                (cast, throws unless the result is exact)
*   (Primitive_type rounded) Expression              (cast, allows rounding, throws on clamping)
*   (Arguments_definition) [Type] => Lambda_body     (lambda)
*
* The content is parsed with the full expression parser and reinterpreted once
* the token after the expression is known:
*   ')' followed by '=>'    - lambda with a single untyped argument
*   ',' or NAME             - lambda argument list (continued as argument definitions)
*   ')' followed by else    - parenthesized expression or primitive cast
* An empty () is only legal as a lambda argument list
* The return type may only be given if the argument list is empty or all
* arguments are typed
*
* The exact cast rule: the parenthesized content is a single primitive type name
* (optionally followed by a cast qualifier) and it is followed by an expression.
* (Type) with a class type is not a cast - class/interface conversions use the 'as' operator
*
* Current token is the opening parenthesis
* After completing token points past the closing parenthesis (or the lambda body)
*/

static unsigned int
azo_parser_parse_parenthesized_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	unsigned int result;
	unsigned int start = token->start;
	unsigned int inner_keyword;
	unsigned int primitive;
	uint16_t flags = 0;
	AZONode *expr;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	/* () is only legal as an (empty) lambda argument list */
	if (token->type == AZO_TOKEN_RIGHT_PARENTHESIS) {
		AZONode *list = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
		parser_append (parser, list);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		return parse_lambda (parser, token, left_precedence, start, 1);
	}
	/* Remember whether the content starts with a primitive type name (cast check) */
	inner_keyword = azo_token_get_keyword (token, parser->src);
	/* The first item is parsed as an expression - whether it is a parenthesized
	 * expression or a lambda argument is decided by the following token */
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
	if (result) return result;
	if ((token->type == AZO_TOKEN_COMMA) || ((token->type == AZO_TOKEN_WORD) && (azo_token_get_keyword (token, parser->src) == AZO_KEYWORD_NONE))) {
		/* Lambda argument list */
		AZONode *first, *decl, *list;
		unsigned int any_typed = 0, any_untyped = 0;
		/* The first expression becomes the first argument */
		first = parser_detach_last (parser);
		if (token->type == AZO_TOKEN_COMMA) {
			/* Untyped argument */
			decl = azo_node_new_with_children (AZO_TERM_ARGUMENT_DECLARATION, AZO_TERM_GENERIC, first->term.start, first->term.end, 2,
				azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, first->term.start, first->term.start), first);
			any_untyped = 1;
		} else {
			/* Typed argument - the expression is the type, the token is the argument name */
			AZONode *name = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
			decl = azo_node_new_with_children (AZO_TERM_ARGUMENT_DECLARATION, AZO_TERM_GENERIC, first->term.start, token->end, 2, first, name);
			any_typed = 1;
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
				azo_node_free_tree (decl);
				return AZO_PARSER_ERROR_UNEXPECTED_EOF;
			}
		}
		list = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
		parser_push (parser, list);
		parser_append (parser, decl);
		result = continue_arguments_definition (parser, token, &any_typed, &any_untyped);
		/* If any argument is typed all arguments have to be typed */
		if (!result && (any_typed && any_untyped)) result = AZO_PARSER_ERROR_SYNTAX;
		if (!result && (token->type == AZO_TOKEN_EOF)) result = AZO_PARSER_ERROR_UNEXPECTED_EOF;
		if (!result && (token->type != AZO_TOKEN_RIGHT_PARENTHESIS)) result = AZO_PARSER_ERROR_SYNTAX;
		if (result) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (list);
			return result;
		}
		list->term.end = token->end;
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
		parser_pop (parser);
		/* A multi-item or typed list is not an expression - it can only be a lambda argument list */
		return parse_lambda (parser, token, left_precedence, start, any_typed);
	}
	expr = parser_peek_last (parser);
	/* The content is exactly the primitive type name (references are single words) */
	primitive = (expr->term.type == AZO_TERM_REFERENCE) && (expr->term.subtype == AZO_TERM_REFERENCE_VARIABLE) &&
		AZO_KEYWORD_IS_PRIMITIVE_TYPE (inner_keyword);
	/* Cast qualifiers */
	if (primitive) {
		if (azo_token_is_keyword (token, AZO_KEYWORD_EXACT, parser->src)) {
			flags |= AZO_TERM_FLAG_EXACT;
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		} else if (azo_token_is_keyword (token, AZO_KEYWORD_ROUNDED, parser->src)) {
			flags |= AZO_TERM_FLAG_ROUNDED;
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
	}
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	unsigned int list_end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	if (AZO_TOKEN_IS_OPERATOR (token) && (AZO_TOKEN_OPERATOR_CODE (token) == AZO_OPERATOR_LAMBDA)) {
		/* Lambda with a single untyped argument */
		AZONode *arg, *decl, *list;
		if (flags) return AZO_PARSER_ERROR_SYNTAX;
		arg = parser_detach_last (parser);
		decl = azo_node_new_with_children (AZO_TERM_ARGUMENT_DECLARATION, AZO_TERM_GENERIC, arg->term.start, arg->term.end, 2,
			azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, arg->term.start, arg->term.start), arg);
		list = azo_node_new_with_children (AZO_TERM_LIST, AZO_TERM_GENERIC, start, list_end, 1, decl);
		parser_append (parser, list);
		return parse_lambda (parser, token, left_precedence, start, 0);
	}
	if (primitive && token_can_start_expression (parser, token)) {
		/* Primitive cast - (primitive [qualifier]) expression */
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_CAST);
		if (result) return result;
		AZONode *right = parser_detach_last (parser);
		AZONode *left = parser_detach_last (parser);
		AZONode *cast_expr = azo_node_new_with_children(AZO_TERM_CAST, AZO_TERM_CAST_CONVERT, start, right->term.end, 2, left, right);
		cast_expr->term.flags = flags;
		parser_append (parser, cast_expr);
	} else if (flags) {
		/* Cast qualifier without cast operand */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
 * Keyword_expression:
 *   null
 *   this
 *   true
 *   false
 *   new
 *   function
 *   Primitive_type (evaluates to its class)
 *
 * Keywords that can start an expression
 *
 * Current token is the keyword
 */

static unsigned int
parse_keyword_expression (AZOParser *parser, AZOToken *token, unsigned int keyword, unsigned int left_precedence)
{
	AZONode *expr;
	unsigned int result;
	/* Keywords with their own (self-appending) parse functions */
	if (keyword == AZO_KEYWORD_NEW) {
		result = parse_new (parser, token);
		if (result) return result;
		return azo_parser_continue_expression (parser, token, left_precedence);
	}
#ifdef HAS_FUNCTION_KEYWORD
	if (keyword == AZO_KEYWORD_FUNCTION) {
		/* LEGACY - function [TYPE] (ARGUMENTS) BLOCK or the bareword evaluating to the function class */
		result = parse_function_definition (parser, token, 0);
		if (result) return result;
		return azo_parser_continue_expression (parser, token, left_precedence);
	}
#endif
	/* Keywords producing a value node */
	switch (keyword) {
		case AZO_KEYWORD_NULL:
			expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_NONE, token->start, token->end);
			break;
		case AZO_KEYWORD_THIS:
			expr = azo_node_new (AZO_TERM_KEYWORD, AZO_KEYWORD_THIS, token->start, token->end);
			break;
		case AZO_KEYWORD_TRUE:
		case AZO_KEYWORD_FALSE:
			expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_BOOLEAN, token->start, token->end);
			az_packed_value_set_boolean (&expr->value, keyword == AZO_KEYWORD_TRUE);
			break;
		case AZO_KEYWORD_COMPLEX: {
			/* complex float / complex double are two-word type names */
			unsigned int start = token->start;
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
			if (azo_token_is_keyword (token, AZO_KEYWORD_FLOAT, parser->src)) {
				expr = new_named_reference (AZO_TERM_REFERENCE_VARIABLE, start, token->end, "complex float");
			} else if (azo_token_is_keyword (token, AZO_KEYWORD_DOUBLE, parser->src)) {
				expr = new_named_reference (AZO_TERM_REFERENCE_VARIABLE, start, token->end, "complex double");
			} else {
				/* Keep the offending token as anchor for error recovery */
				return AZO_PARSER_ERROR_SYNTAX;
			}
			break;
		}
		default:
			if (AZO_KEYWORD_IS_PRIMITIVE_TYPE (keyword)) {
				/* Primitive type names evaluate to their class */
				expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
				break;
			}
			/* Keywords that cannot start an expression - keep the offending token as anchor */
			return AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION;
	}
	parser_append (parser, expr);
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
 * Naked_expression:
 *   Literal
 *   Keyword_expression
 *   Variable_reference
 *   Member_reference
 *   Array_reference
 *   Function_call
 *   Operation
 */

static unsigned int
azo_parser_parse_naked_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	AZONode *expr = NULL;
	unsigned int result;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (AZO_TOKEN_IS_WORD(token)) {
		unsigned int keyword = azo_token_get_keyword(token, parser->src);
		if (keyword != AZO_KEYWORD_NONE) return parse_keyword_expression (parser, token, keyword, left_precedence);
		/* Bareword */
		/* Variable reference */
		expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
	} else if (AZO_TOKEN_IS_NUMBER (token)) {
		expr = azo_node_new_number (parser->src, token);
	} else if (token->type == AZO_TOKEN_TEXT) {
		/* String literal */
		expr = azo_node_new_text (parser->src, token);
	} else if (token->type == AZO_TOKEN_LEFT_BRACE) {
		/* Array literal */
		result = parse_array_literal (parser, token);
		if (result) return result;
	} else if (AZO_TOKEN_IS_OPERATOR (token)) {
		result = azo_parser_parse_prefix_expression (parser, token);
		if (result) return result;
	} else {
		/* Everything else (separators, closing brackets, INVALID) cannot start an expression */
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION;
	}
	if (expr) {
		parser_append (parser, expr);
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
	}
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
 * Member:
 *   Variable_reference
 *   Member_reference
 *   Array_reference
 *   Function_call
 *   Operation
 *
 * If HAS_FUNCTION_KEYWORD is defined the legacy 'function' keyword is also
 * accepted (declares a member function with implicit this parameter)
 */

static unsigned int
azo_parser_parse_member (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	AZONode *expr = NULL;
	unsigned int result;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
#ifdef HAS_FUNCTION_KEYWORD
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* LEGACY - member function definition (the object reference is already on the stack) */
		result = parse_function_definition (parser, token, 1);
		if (result) return result;
		return azo_parser_continue_expression (parser, token, left_precedence);
	}
#endif
	if ((token->type == AZO_TOKEN_WORD) && (azo_token_get_keyword (token, parser->src) == AZO_KEYWORD_NONE)) {
		/* Variable reference (keywords cannot be references) */
		expr = azo_node_new_reference (AZO_TERM_REFERENCE_PROPERTY, parser->src, token);
		/* fixme: Allowed next - operator/function/array */
	} else {
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION;
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
	while (token->type != AZO_TOKEN_EOF) {
		unsigned int error;
		if (AZO_TOKEN_IS_OPERATOR (token)) {
			unsigned int op, rightprecedence;
			op = AZO_TOKEN_OPERATOR_CODE (token);
			/* Assignment operators are not expression operators - they terminate the expression (handled at statement level) */
			if (azo_operator_is_assignment (op)) break;
			rightprecedence = azo_operator_get_left_precedence (op);
			if (rightprecedence > left_precedence) {
				error = parse_operator (parser, token);
				if (error) return error;
			} else {
				break;
			}
		} else if (azo_token_is_keyword (token, AZO_KEYWORD_IS, parser->src) || azo_token_is_keyword (token, AZO_KEYWORD_IMPLEMENTS, parser->src) || azo_token_is_keyword (token, AZO_KEYWORD_AS, parser->src)) {
			if (left_precedence < AZO_PRECEDENCE_TYPE) {
				error = parse_type_operator (parser, token);
				if (error) return error;
			} else {
				break;
			}
		} else if ((token->type == AZO_TOKEN_LEFT_PARENTHESIS) && (left_precedence < AZO_PRECEDENCE_FUNCTION)) {
			/* Function call */
			error = parse_function_call (parser, token);
			if (error) return error;
		} else if ((token->type == AZO_TOKEN_LEFT_BRACKET) && (left_precedence < AZO_PRECEDENCE_ARRAY)) {
			/* Array */
			error = parse_array_element (parser, token);
			if (error) return error;
		} else {
			/* Everything else terminates expression */
			break;
		}
	}
	return AZO_PARSER_ERROR_NONE;
}

static unsigned int
azo_parser_parse_prefix_expression (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *right;
	unsigned int start, error;
	int subtype;
	/* Unary prefix operators are processed here */
	subtype = azo_token_get_prefix_term (token);
	if (subtype < 0) {
		/* Binary or tertiary operator at first position of expression */
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION;
	}
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_UNARY);
	if (error) return error;
	right = parser_detach_last (parser);
	/* Prefix ++/-- of a postfix ++/-- (++a++) is rejected: the two are different
	 * operations and the combination is ambiguous. Unary +/- of a postfix
	 * (-a++) is legal */
	if (((subtype == AZO_TERM_PREFIX_INCREMENT) || (subtype == AZO_TERM_PREFIX_DECREMENT)) &&
		(right->term.type == AZO_TERM_SUFFIX) &&
		((right->term.subtype == AZO_TERM_SUFFIX_INCREMENT) || (right->term.subtype == AZO_TERM_SUFFIX_DECREMENT))) {
		azo_node_free_tree (right);
		return AZO_PARSER_ERROR_SYNTAX;
	}
	expr = azo_node_new_with_children (AZO_TERM_PREFIX, subtype, start, right->term.end, 1, right);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/* Whether the term is an assignable LValue (variable/member reference or array element) */

static unsigned int
term_is_lvalue (AZOTerm *term)
{
	return (term->type == AZO_TERM_REFERENCE) || (term->type == AZO_TERM_ARRAY_ELEMENT);
}

/* Free a chain of nodes linked by the next pointer (each node's own subtree included) */

static void
free_node_chain (AZONode *node)
{
	while (node) {
		AZONode *next = node->next;
		azo_node_free_tree (node);
		node = next;
	}
}

/*
 * Assignment:
 *   LValue assignment_operator Expression
 *   LValue = LValue [= LValue ...] = Expression
 *
 * Only the plain = chains
 * The LHS is already parsed (last node in the parser)
 * Current token is the assignment operator
 */

static unsigned int
parse_assignment_statement (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *left, *right, *last;
	unsigned int start;
	int subtype;
	subtype = azo_token_get_assignment_term (token);
	left = parser_detach_last (parser);
	if (!term_is_lvalue (&left->term)) {
		azo_node_free_tree (left);
		return AZO_PARSER_ERROR_SYNTAX;
	}
	start = left->term.start;
	last = NULL;
	while (1) {
		unsigned int result;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			free_node_chain (left);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (result) {
			free_node_chain (left);
			return result;
		}
		right = parser_detach_last (parser);
		/* Chained assignment continues only when both the statement and the next operator are plain = */
		if ((subtype != AZO_TERM_ASSIGN_PLAIN) || !AZO_TOKEN_IS_OPERATOR (token) || (AZO_TOKEN_OPERATOR_CODE (token) != AZO_OPERATOR_ASSIGN)) break;
		/* In a chain the RHS becomes the next target, so it has to be an LValue */
		if (!term_is_lvalue (&right->term)) {
			free_node_chain (left);
			azo_node_free_tree (right);
			return AZO_PARSER_ERROR_SYNTAX;
		}
		if (last) {
			last->next = right;
		} else {
			left->next = right;
		}
		last = right;
	}
	/* The last parsed value terminates the target chain */
	if (last) {
		last->next = right;
	} else {
		left->next = right;
	}
	/* Children are target(s) followed by the value (the chain is built manually because of its variable size) */
	expr = azo_node_new (AZO_TERM_ASSIGN, subtype, start, right->term.end);
	expr->children = left;
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Parse operator and append expression to parser
 *
 * Assignment operators are not parsed here - they terminate the expression
 * and are handled at statement level (parse_assignment_statement)
 *
 * Current token is operator
 * After completing token points after the end of RHS expression
 */

static unsigned int
parse_operator (AZOParser *parser, AZOToken *token)
{
	AZONode *left, *right, *expr;
	int subtype, precendence;
	unsigned int end, error;

	subtype = AZO_TOKEN_OPERATOR_CODE (token);
	precendence = azo_operator_get_right_precedence (subtype);
	end = token->end;
	/* Save the operator token for term resolution (token will advance past the expression) */
	AZOToken optoken = *token;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	if (subtype == AZO_OPERATOR_DOT) {
		/* Member reference */
		error = azo_parser_parse_member (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
#ifdef HAS_FUNCTION_KEYWORD
		if (right->term.type == AZO_TERM_FUNCTION) {
			/* LEGACY - value.function construct (member function definition), left is already consumed */
			parser_append (parser, right);
		} else
#endif
		if (right->term.type == AZO_TERM_REFERENCE) {
			/* ref.ref construct */
			left = parser_detach_last (parser);
			expr = azo_node_new_with_children(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER, left->term.start, right->term.end, 2, left, right);
			parser_append (parser, expr);
		} else {
			return AZO_PARSER_ERROR_SYNTAX;
		}
		return AZO_PARSER_ERROR_NONE;
	}
	if (azo_operator_is_comparison (subtype)) {
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		subtype = azo_token_get_comparison_term(&optoken);
		expr = azo_node_new_with_children(AZO_TERM_COMPARISON, subtype, left->term.start, right->term.end, 2, left, right);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	if (azo_operator_is_arithmetic (subtype)) {
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		subtype = azo_token_get_binary_term(&optoken);
		expr = azo_node_new_with_children(AZO_TERM_BINARY, subtype, left->term.start, right->term.end, 2, left, right);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	if (subtype == AZO_OPERATOR_PLUSPLUS) {
		left = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_SUFFIX, AZO_TERM_SUFFIX_INCREMENT, left->term.start, end, 1, left);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	if (subtype == AZO_OPERATOR_MINUSMINUS) {
		left = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_SUFFIX, AZO_TERM_SUFFIX_DECREMENT, left->term.start, end, 1, left);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	if (subtype == AZO_OPERATOR_QUESTION) {
		/* Ternary selection - CONDITION ? TRUE_EXPRESSION : FALSE_EXPRESSION */
		AZONode *cond, *iftrue, *iffalse;
		unsigned int qend;
		cond = parser_detach_last (parser);
		/* True expression (stops at the colon - it is not an operator) */
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) {
			azo_node_free_tree (cond);
			return error;
		}
		iftrue = parser_detach_last (parser);
		if (!AZO_TOKEN_IS_OPERATOR (token) || (AZO_TOKEN_OPERATOR_CODE (token) != AZO_OPERATOR_COLON)) {
			azo_node_free_tree (iftrue);
			azo_node_free_tree (cond);
			return AZO_PARSER_ERROR_SYNTAX;
		}
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			azo_node_free_tree (iftrue);
			azo_node_free_tree (cond);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
		/* False expression (right-associative - a ? b : c ? d : e groups as a ? b : (c ? d : e)) */
		error = azo_parser_parse_expression (parser, token, precendence);
		if (error) {
			azo_node_free_tree (iftrue);
			azo_node_free_tree (cond);
			return error;
		}
		iffalse = parser_detach_last (parser);
		qend = iffalse->term.end;
		expr = azo_node_new_with_children (AZO_TERM_SELECT, AZO_TERM_GENERIC, cond->term.start, qend, 3, cond, iftrue, iffalse);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	}
	/* Everything else (arrow, comma, ...) is not a valid expression operator */
	return AZO_PARSER_ERROR_SYNTAX;
}

/*
* Parse a type operator and append the expression to parser
*   Expression is Expression         (type test - boolean result)
*   Expression implements Expression (interface test - boolean result)
*   Expression as Expression         (checked class/interface conversion)
*
* is/implements produce an AZO_TERM_TEST node, as produces AZO_TERM_CAST with
* subtype CAST_AS (primitive conversions are the (type) expression cast, CAST_CONVERT)
*
* Current token is the operator keyword
* After completing token points after the end of RHS expression
*/

static unsigned int
parse_type_operator (AZOParser *parser, AZOToken *token)
{
	unsigned int end, error;

	unsigned int type, subtype;
	if (azo_token_is_keyword (token, AZO_KEYWORD_IS, parser->src)) {
		type = AZO_TERM_TEST;
		subtype = AZO_TERM_TEST_IS;
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_IMPLEMENTS, parser->src)) {
		type = AZO_TERM_TEST;
		subtype = AZO_TERM_TEST_IMPLEMENTS;
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_AS, parser->src)) {
		type = AZO_TERM_CAST;
		subtype = AZO_TERM_CAST_AS;
	} else {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_TYPE);
	if (error) return error;
	AZONode *right = parser_detach_last (parser);
	AZONode *left = parser_detach_last (parser);
	AZONode *expr;
	if (type == AZO_TERM_TEST) {
		/* TEST children: [value, type] */
		expr = azo_node_new_with_children (type, subtype, left->term.start, right->term.end, 2, left, right);
	} else {
		/* CAST children: [type, value] - the same layout as the primitive (type) cast */
		expr = azo_node_new_with_children (type, subtype, left->term.start, right->term.end, 2, right, left);
	}
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Parse comma-separated expressions into the current node, until the terminator
 * token (')' or '}') is reached
 *
 * Current token is at the first expression or the terminator
 * After completing token is at the terminator (not consumed)
 */

static unsigned int
parse_expression_list (AZOParser *parser, AZOToken *token, unsigned int terminator)
{
	unsigned int error;
	if (token->type == terminator) return AZO_PARSER_ERROR_NONE;
	while (1) {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) return error;
		if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		if (token->type == terminator) return AZO_PARSER_ERROR_NONE;
		if (token->type != AZO_TOKEN_COMMA) return AZO_PARSER_ERROR_SYNTAX;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	}
}

/*
* Parse list of function arguments into LIST expression
*   ([ARGUMENT [, ARGUMENT ...]])
*
* Current token points to the opening parenthesis
* After completing token points past the closing parenthesis
*/

static unsigned int
parse_list (AZOParser *parser, AZOToken *token)
{
	unsigned int start, error;
	start = token->start;
	/* ( */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	AZONode *expr = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
	parser_push (parser, expr);
	error = parse_expression_list (parser, token, AZO_TOKEN_RIGHT_PARENTHESIS);
	if (error) {
		parser_pop (parser);
		parser_detach_last (parser);
		azo_node_free_tree (expr);
		return error;
	}
	expr->term.end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return AZO_PARSER_ERROR_NONE;
}

/*
* Parse function call
*   Expression(ARGUMENTS)
*
* The callee expression is already parsed (last node on the stack)
* Current token points to the opening parenthesis
* After completing token points past the closing parenthesis
*/

static unsigned int
parse_function_call (AZOParser *parser, AZOToken *token)
{
	unsigned int error = parse_list (parser, token);
	if (error) {
		return error;
	}
	AZONode *right = parser_detach_last (parser);
	AZONode *left = parser_detach_last (parser);
	AZONode *expr = azo_node_new_with_children (AZO_TERM_FUNCTION_CALL, AZO_TERM_GENERIC, left->term.start, right->term.end, 2, left, right);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Parse a single argument definition
 *   Type NAME    (typed)
 *   NAME         (untyped)
 *
 * has_type is set to 1 if the argument has an explicit type, 0 otherwise
 * (the typedness is needed by the lambda rule: arguments are either all
 * typed or all untyped, and the return type requires typed arguments)
 */

static unsigned int
parse_argument_definition (AZOParser *parser, AZOToken *token, unsigned int *has_type)
{
	AZONode *left, *right, *decl;
	unsigned int result, start;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	start = token->start;
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
	if (result) return result;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if ((token->type == AZO_TOKEN_WORD) && (azo_token_get_keyword (token, parser->src) == AZO_KEYWORD_NONE)) {
		AZONode *name = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, name);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		right = parser_detach_last (parser);
		left = parser_detach_last (parser);
		*has_type = 1;
	} else {
		left = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, start, start);
		right = parser_detach_last (parser);
		*has_type = 0;
	}
	decl = azo_node_new_with_children (AZO_TERM_ARGUMENT_DECLARATION, AZO_TERM_GENERIC, start, token->start, 2, left, right);
	parser_append (parser, decl);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * Parse the remaining argument definitions of an argument list - the first
 * argument is already parsed and appended, token is at the comma or closing
 * parenthesis following it
 *
 * The arguments are appended to the current node (the argument list)
 * any_typed / any_untyped accumulate the typedness of the arguments
 */

static unsigned int
continue_arguments_definition (AZOParser *parser, AZOToken *token, unsigned int *any_typed, unsigned int *any_untyped)
{
	while (token->type == AZO_TOKEN_COMMA) {
		unsigned int is_typed, result;
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		result = parse_argument_definition (parser, token, &is_typed);
		if (result) return result;
		*any_typed |= is_typed;
		*any_untyped |= !is_typed;
	}
	return AZO_PARSER_ERROR_NONE;
}

#ifdef HAS_FUNCTION_KEYWORD
/*
 * LEGACY - kept for old scripts, new code uses lambdas (=>)
 *
 * Parse a list of argument definitions into a LIST expression
 *   ([ARGUMENT_DEFINITION [, ARGUMENT_DEFINITION ...]])
 *
 * Current token is the opening parenthesis
 * After completing token points past the closing parenthesis
 */

static unsigned int
parse_arguments_definition (AZOParser *parser, AZOToken *token)
{
	unsigned int start, result;
	start = token->start;
	/* ( */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	AZONode *expr = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
	parser_push (parser, expr);

	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) {
		/* The typedness is not restricted by the legacy syntax */
		unsigned int any_typed = 0, any_untyped = 0, is_typed;
		result = parse_argument_definition (parser, token, &is_typed);
		if (!result) {
			any_typed |= is_typed;
			any_untyped |= !is_typed;
			result = continue_arguments_definition (parser, token, &any_typed, &any_untyped);
		}
		if (!result && (token->type == AZO_TOKEN_EOF)) result = AZO_PARSER_ERROR_UNEXPECTED_EOF;
		if (!result && (token->type != AZO_TOKEN_RIGHT_PARENTHESIS)) result = AZO_PARSER_ERROR_SYNTAX;
		if (result) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return result;
		}
	}
	expr->term.end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * LEGACY - kept for old scripts, new code uses lambdas (=>)
 *
 * Function_definition:
 *   function [TYPE | VOID] (ARGUMENTS) BLOCK    (function definition)
 *   function                                    (bareword - evaluates to the function class)
 *
 * Builds an AZO_TERM_FUNCTION node with children [return_type, arguments, body]
 * ([return_type, object, arguments, body] for member functions), the same shape
 * the resolver/compiler expect from a lambda
 *
 * Current token is the function keyword
 * If member, the object reference is already on the stack
 * After completing token points past the body
 */

static unsigned int
parse_function_definition (AZOParser *parser, AZOToken *token, unsigned int is_member)
{
	AZONode *expr, *type, *args, *body;
	unsigned int start, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	/* Return type */
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* function function (...) - i.e. a function returning a function */
		/* We create reference as it should be interpreted as type */
		type = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			azo_node_free (type);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_VOID, parser->src)) {
		/* function void (...) */
		type = azo_node_new (AZO_TERM_KEYWORD, AZO_KEYWORD_VOID, token->start, token->end);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
			azo_node_free (type);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
	} else if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		/* function (...) - no return type */
		type = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
	} else if (AZO_TOKEN_IS_WORD (token)) {
		/* function type (...) - the return type is given as an expression (may be a type keyword, e.g. int32) */
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_FUNCTION);
		if (error) return error;
		type = parser_detach_last (parser);
	} else if ((token->type == AZO_TOKEN_RIGHT_PARENTHESIS) || (token->type == AZO_TOKEN_RIGHT_BRACKET) ||
		(token->type == AZO_TOKEN_SEMICOLON) || (token->type == AZO_TOKEN_COMMA) || (token->type == AZO_TOKEN_RIGHT_BRACE) ||
		AZO_TOKEN_IS_OPERATOR (token)) {
		/* The bareword evaluates to the function class (e.g. function.name, a implements function, function f) */
		expr = new_named_reference (AZO_TERM_REFERENCE_VARIABLE, start, token->start, "function");
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	} else {
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	/* Arguments */
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) {
		azo_node_free (type);
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	error = parse_arguments_definition (parser, token);
	if (error) {
		azo_node_free (type);
		return error;
	}
	args = parser_detach_last (parser);
	/* Body - a function definition requires a block */
	if (token->type != AZO_TOKEN_LEFT_BRACE) {
		azo_node_free (args);
		azo_node_free (type);
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	error = azo_parser_parse_sentence (parser, token);
	if (error) {
		azo_node_free (args);
		azo_node_free (type);
		return error;
	}
	body = parser_detach_last (parser);
	if (is_member) {
		AZONode *obj = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER, obj->term.start, body->term.end, 4, type, obj, args, body);
	} else {
		expr = azo_node_new_with_children (AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_STATIC, start, body->term.end, 3, type, args, body);
	}
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}
#endif

/*
 * Lambda:
 *   (ARGUMENTS) => BODY
 *   (ARGUMENTS) RETURN_TYPE => BODY
 *
 * BODY is a block or a single expression (the value is returned implicitly)
 * The return type may only be given if the argument list is empty or all
 * arguments are typed (type_allowed) - for untyped arguments it would be
 * ambiguous with a call of the parenthesized argument
 *
 * Builds an AZO_TERM_FUNCTION node with children [return_type, arguments, body],
 * the same shape the resolver/compiler expect from a function definition
 *
 * The argument list is already parsed and is the last node on the stack
 * Current token is at => or at the return type (only words and void are legal)
 * After completing token points past the body
 */

static unsigned int
parse_lambda (AZOParser *parser, AZOToken *token, unsigned int left_precedence, unsigned int start, unsigned int type_allowed)
{
	AZONode *expr, *type, *args, *body;
	unsigned int error;
	/* Optional return type (stops at =>) */
	if (AZO_TOKEN_IS_OPERATOR (token) && (AZO_TOKEN_OPERATOR_CODE (token) == AZO_OPERATOR_LAMBDA)) {
		/* No return type */
		type = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
	} else if (!type_allowed) {
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_VOID, parser->src)) {
		/* void return type */
		type = azo_node_new (AZO_TERM_KEYWORD, AZO_KEYWORD_VOID, token->start, token->end);
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
	} else if (AZO_TOKEN_IS_WORD (token)) {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (error) return error;
		type = parser_detach_last (parser);
	} else {
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	args = parser_detach_last (parser);
	/* => */
	if (!AZO_TOKEN_IS_OPERATOR (token) || (AZO_TOKEN_OPERATOR_CODE (token) != AZO_OPERATOR_LAMBDA)) {
		azo_node_free (type);
		azo_node_free_tree (args);
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
		azo_node_free (type);
		azo_node_free_tree (args);
		return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	}
	/* Body: block or a single expression */
	if (token->type == AZO_TOKEN_LEFT_BRACE) {
		error = azo_parser_parse_sentence (parser, token);
	} else {
		/* Expression body - the value is returned implicitly */
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	}
	if (error) {
		azo_node_free (type);
		azo_node_free_tree (args);
		return error;
	}
	body = parser_detach_last (parser);
	expr = azo_node_new_with_children (AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_STATIC, start, body->term.end, 3, type, args, body);
	parser_append (parser, expr);
	return azo_parser_continue_expression (parser, token, left_precedence);
}

/*
* Parse array element
*   [EXPRESSION]
*
* The empty index ([]) is not supported - array type declarations and new-array
* expressions are a separate construct, not an empty subscript
*
* Current token is at opening bracket
* Array reference is last element in stack
* Result - array element expression as last element in stack
*/

static unsigned int
parse_array_element (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *left, *right;
	unsigned int start, end, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	right = parser_detach_last (parser);
	if (token->type == AZO_TOKEN_EOF) {
		azo_node_free_tree (right);
		return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	}
	if (token->type != AZO_TOKEN_RIGHT_BRACKET) {
		azo_node_free_tree (right);
		return AZO_PARSER_ERROR_SYNTAX;
	}
	end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	left = parser_detach_last (parser);
	expr = azo_node_new_with_children (AZO_TERM_ARRAY_ELEMENT, AZO_TERM_GENERIC, start, end, 2, left, right);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
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
	AZONode *expr;
	unsigned int start, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	expr = azo_node_new (AZO_TERM_LITERAL_ARRAY, AZO_TERM_GENERIC, token->start, token->end);
	parser_push (parser, expr);
	error = parse_expression_list (parser, token, AZO_TOKEN_RIGHT_BRACE);
	if (error) {
		parser_pop (parser);
		parser_detach_last (parser);
		azo_node_free_tree (expr);
		return error;
	}
	expr->term.end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return AZO_PARSER_ERROR_NONE;
}

/*
* Parse constructor
*   new EXPRESSION(ARGUMENTS)
*
* The class expression is parsed at function precedence: member references bind
* tighter (DOT), so the path is consumed and the parse stops at the constructor's
* opening parenthesis. The rest of the chain (e.g. .someMethod() in
* new MyObject().someMethod()) continues on the NEW node.
*
* Current token is at keyword "new"
* After completing token points past the closing parenthesis
*/

static unsigned int
parse_new (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *type, *args;
	unsigned int start, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	/* The class expression (member references bind tighter, calls are not consumed) */
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_FUNCTION);
	if (error) return error;
	/* The constructor arguments */
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	error = parse_list (parser, token);
	if (error) return error;
	args = parser_detach_last (parser);
	type = parser_detach_last (parser);
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_NEW, start, args->term.end, 2, type, args);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
* while (CONDITION) STATEMENT
*
* Current token is at keyword while
*/

static unsigned int
parse_while (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *left, *middle, *right, *block;
	unsigned int start, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	/* Expression */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	/* ) */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	block = parser_detach_last (parser);
	middle = parser_detach_last (parser);
	left = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, middle->term.start, middle->term.start);
	right = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, middle->term.start, middle->term.start);
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_FOR, start, block->term.end, 4, left, middle, right, block);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * for (Multi_statement ; [Expression] ; Silent_multi_statement) Sentence
 *
 * Current token is at keyword
 */

static unsigned int
parse_for (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *left, *middle, *right, *block;
	unsigned int start, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	/* Initialization */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = parse_multi_statement (parser, token, AZO_TOKEN_SEMICOLON, 0);
	if (error) return error;
	/* Condition (the ';' was validated by parse_multi_statement) */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (error) return error;
	} else {
		/* Empty condition */
		AZONode *empty = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
		parser_append (parser, empty);
	}
	/* ; */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) return AZO_PARSER_ERROR_SYNTAX;
	/* Step */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = parse_multi_statement (parser, token, AZO_TOKEN_RIGHT_PARENTHESIS, 1);
	if (error) return error;
	/* Statement/Block (the ')' was validated by parse_multi_statement) */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	block = parser_detach_last (parser);
	right = parser_detach_last (parser);
	middle = parser_detach_last (parser);
	left = parser_detach_last (parser);
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_FOR, start, block->term.end, 4, left, middle, right, block);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
* if (EXPRESSION) STATEMENT [ ELSE STATEMENT ]
*
* Current token is at keyword
*/

static unsigned int
parse_if (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *cond, *if_true, *if_false;
	unsigned int start, end, error;
	/* ( */
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	/* Expression */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	cond = parser_detach_last (parser);
	/* ) */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return AZO_PARSER_ERROR_CLOSING_PARENTHESIS_MISSING;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	if_true = parser_detach_last (parser);
	end = if_true->term.end;
	/* Potential else */
	if_false = NULL;
	if (azo_token_is_keyword (token, AZO_KEYWORD_ELSE, parser->src)) {
		/* Statement/Block */
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		error = azo_parser_parse_sentence (parser, token);
		if (error) return error;
		if_false = parser_detach_last (parser);
		end = if_false->term.end;
	}
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_IF, start, end, 3, cond, if_true, if_false);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * do STATEMENT while (EXPRESSION) ;
 *
 * Condition is checked after the statement, so unlike while this cannot
 * be translated into a for node
 *
 * Current token is at keyword do
 */

static unsigned int
parse_do (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *block, *cond;
	unsigned int start, end, error;
	start = token->start;
	/* Statement/Block */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_sentence (parser, token);
	if (error) return error;
	block = parser_detach_last (parser);
	/* while */
	if (!azo_token_is_keyword (token, AZO_KEYWORD_WHILE, parser->src)) return AZO_PARSER_ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	/* ( */
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	/* Expression */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (error) return error;
	cond = parser_detach_last (parser);
	/* ) */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) return AZO_PARSER_ERROR_CLOSING_PARENTHESIS_MISSING;
	end = token->end;
	/* ; */
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_SEMICOLON) {
		/* Repair missing semicolon if the next token clearly starts a new sentence */
		/* An identifier also qualifies - nothing can follow the closing parenthesis of do...while */
		if ((token->type != AZO_TOKEN_WORD) && !token_is_sentence_boundary (parser, token)) return AZO_PARSER_ERROR_SEMICOLON_MISSING;
		parser_report_error (parser, token, AZO_PARSER_ERROR_SEMICOLON_MISSING);
	} else {
		end = token->end;
		azo_tokenizer_get_next_token (&parser->tokenizer, token);
	}
	/* Unlike for the condition is evaluated after the block */
	expr = azo_node_new_with_children (AZO_TERM_KEYWORD, AZO_KEYWORD_DO, start, end, 2, block, cond);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
}

