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
static unsigned int azo_parser_parse_function_definition (AZOParser *parser, AZOToken *token, unsigned int has_return_type, unsigned int is_member);

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
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* 'function' is a keyword but in declaration it is treated like a class name (declares a function type variable) */
		AZONode *expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		if ((token->type != AZO_TOKEN_WORD) || (azo_token_get_keyword (token, parser->src) != AZO_KEYWORD_NONE)) return AZO_PARSER_ERROR_SYNTAX;
		return azo_parser_parse_declaration (parser, token, flags);
	} else {
		/* Either Declaration or Silent Statement */
		/* Both start with expression */
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (result) return result;
	}
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
* Parenthesized_expression:
*   (Primitive_type) Expression            (cast - primitive conversions only)
*   (Primitive_type exact) Expression      (cast, throws unless the result is exact)
*   (Primitive_type rounded) Expression    (cast, allows rounding, throws on clamping)
*   (Expression)
*
* The exact cast rule: the parenthesized content is a single primitive type name
* (optionally followed by a cast qualifier) and it is followed by an expression.
* (Type) with a class type is not a cast - class/interface conversions use the 'as' operator
*
* Current token is the opening parenthesis
* After completing token points past the closing parenthesis
*/

static unsigned int
azo_parser_parse_parenthesized_expression (AZOParser *parser, AZOToken *token, unsigned int left_precedence)
{
	unsigned int result;
	unsigned int start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	/* Remember whether the inner expression starts with a primitive type name */
	unsigned int inner_keyword = azo_token_get_keyword (token, parser->src);
	result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
	if (result) return result;
	AZONode *expr = parser_peek_last (parser);
	/* The inner node is exactly the primitive type name (references are single words) */
	unsigned int primitive = (expr->term.type == AZO_TERM_REFERENCE) && (expr->term.subtype == AZO_TERM_REFERENCE_VARIABLE) &&
		AZO_KEYWORD_IS_PRIMITIVE_TYPE (inner_keyword);
	/* Cast qualifiers */
	uint16_t flags = 0;
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
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	if (primitive && token_can_start_expression (parser, token)) {
		/* Primitive cast - (primitive [qualifier]) expression */
		result = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_CAST);
		if (result) return result;
		AZONode *right = parser_detach_last (parser);
		AZONode *left = parser_detach_last (parser);
		AZONode *cast_expr = azo_node_new_with_children(AZO_TERM_CAST, AZO_TERM_GENERIC, start, right->term.end, 2, left, right);
		cast_expr->term.flags = flags;
		parser_append (parser, cast_expr);
	} else if (flags) {
		/* Cast qualifier without cast operand */
		return AZO_PARSER_ERROR_SYNTAX;
	}
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
	AZONode *expr = NULL;
	unsigned int result;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (AZO_TOKEN_IS_WORD(token)) {
		unsigned int keyword = azo_token_get_keyword(token, parser->src);
		if (keyword != AZO_KEYWORD_NONE) {
			/* Known keyword */
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
				case AZO_KEYWORD_NEW:
					result = parse_new (parser, token);
					if (result) return result;
					break;
				case AZO_KEYWORD_FUNCTION:
					result = azo_parser_parse_function_definition (parser, token, 0, 0);
					if (result) return result;
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
					azo_tokenizer_get_next_token (&parser->tokenizer, token);
					return AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION;
			}
		} else {
			/* Bareword */
			/* Variable reference */
			expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
			/* fixme: Allowed next - operator/function/array */
		}
	} else if (AZO_TOKEN_IS_NUMBER (token)) {
		expr = azo_node_new_number (parser->src, token);
	} else if (token->type == AZO_TOKEN_TEXT) {
		/* String literal */
		expr = azo_node_new_text (parser->src, token);
	} else if (token->type == AZO_TOKEN_LEFT_BRACE) {
		/* Array literal */
		result = parse_array_literal (parser, token);
		if (result) return result;
		/* fixme: Allowed next - array */
	} else if (AZO_TOKEN_IS_OPERATOR (token)) {
		result = azo_parser_parse_prefix_expression (parser, token);
		if (result) return result;
		/* fixme: Allowed next - operator */
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
	AZONode *expr = NULL;
	unsigned int result;
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* function */
		result = azo_parser_parse_function_definition (parser, token, 0, 1);
		if (result) return result;
		/* fixme: Are operators allowed here? */
	} else if ((token->type == AZO_TOKEN_WORD) && (azo_token_get_keyword (token, parser->src) == AZO_KEYWORD_NONE)) {
		/* Variable reference (keywords except function cannot be references) */
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
			rightprecedence = azo_operator_get_precedence (op, 1);
			if (left_precedence > rightprecedence) {
				error = parse_operator (parser, token);
				if (error) return error;
			} else {
				break;
			}
		} else if (azo_token_is_keyword (token, AZO_KEYWORD_IS, parser->src) || azo_token_is_keyword (token, AZO_KEYWORD_IMPLEMENTS, parser->src)) {
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
	return AZO_PARSER_ERROR_NONE;
}

static unsigned int
azo_parser_parse_prefix_expression (AZOParser *parser, AZOToken *token)
{
	AZONode *expr, *right;
	unsigned int start, op, error;
	/* Unary prefix operators are processed here */
	/* Parse next expression to stack */
	int subtype = azo_token_get_prefix_term(token);
	if (subtype < 0) {
		/* Binary or tertiary operator at first position of expression */
		/* Keep the offending token as anchor for error recovery */
		return AZO_PARSER_ERROR_SYNTAX;
	}
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_UNARY);
	if (error) return error;
	right = parser_detach_last (parser);
	expr = azo_node_new_with_children (AZO_TERM_PREFIX, subtype, start, right->term.end, 1, right);
	parser_append (parser, expr);
	/* fixme: Allowed next - operator */
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
	AZONode *left, *right, *expr, *tmp;
	int subtype, precendence;
	unsigned int end, error;

	subtype = AZO_TOKEN_OPERATOR_CODE (token);
	precendence = azo_operator_get_precedence (subtype, 1);
	end = token->end;
	/* Save the operator token for term resolution (token will advance past the expression) */
	AZOToken optoken = *token;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	switch (subtype) {
	case AZO_OPERATOR_DOT:
		/* Member reference */
		error = azo_parser_parse_member (parser, token, precendence);
		if (error) return error;
		right = parser_detach_last (parser);
		if (right->term.type == AZO_TERM_FUNCTION) {
			/* value.function construct, left is already consumed */
			parser_append (parser, right);
		} else if (right->term.type == AZO_TERM_REFERENCE) {
			/* ref.ref construct */
			left = parser_detach_last (parser);
			expr = azo_node_new_with_children(AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_MEMBER, left->term.start, right->term.end, 2, left, right);
			parser_append (parser, expr);
		} else {
			return AZO_PARSER_ERROR_SYNTAX;
		}
		return AZO_PARSER_ERROR_NONE;
	case AZO_OPERATOR_ARROW:
		/* No arrow at moment */
		return AZO_PARSER_ERROR_SYNTAX;
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
		subtype = azo_token_get_comparison_term(&optoken);
		/* Normalize GE/GT to LE/LT by swapping operands */
		if ((subtype == AZO_TERM_COMPARISON_GE) || (subtype == AZO_TERM_COMPARISON_GT)) {
			subtype = (subtype == AZO_TERM_COMPARISON_GE) ? AZO_TERM_COMPARISON_LE : AZO_TERM_COMPARISON_LT;
			tmp = left;
			left = right;
			right = tmp;
		}
		expr = azo_node_new_with_children(AZO_TERM_COMPARISON, subtype, left->term.start, right->term.end, 2, left, right);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
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
		subtype = azo_token_get_binary_term(&optoken);
		expr = azo_node_new_with_children(AZO_TERM_BINARY, subtype, left->term.start, right->term.end, 2, left, right);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
		/* Suffix */
	case AZO_OPERATOR_PLUSPLUS:
		left = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_SUFFIX, AZO_TERM_SUFFIX_INCREMENT, left->term.start, end, 1, left);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	case AZO_OPERATOR_MINUSMINUS:
		left = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_SUFFIX, AZO_TERM_SUFFIX_DECREMENT, left->term.start, end, 1, left);
		parser_append (parser, expr);
		return AZO_PARSER_ERROR_NONE;
	/* Question */
	case AZO_OPERATOR_QUESTION:
	case AZO_OPERATOR_COLON:
		return AZO_PARSER_ERROR_SYNTAX;
	// fixme: Actually maybe we should support assignment-as-expression for a = b = c constructs?
	default:
		break;
	}
	return AZO_PARSER_ERROR_SYNTAX;
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
	unsigned int op_type, end, error;

	unsigned int subtype;
	if (azo_token_is_keyword (token, AZO_KEYWORD_IS, parser->src)) {
		subtype = AZO_TERM_TEST_IS;
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_IMPLEMENTS, parser->src)) {
		subtype = AZO_TERM_TEST_IMPLEMENTS;
	} else {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_TYPE);
	if (error) return error;
	AZONode *right = parser_detach_last (parser);
	AZONode *left = parser_detach_last (parser);
	AZONode *expr = azo_node_new_with_children (AZO_TERM_TEST, subtype, left->term.start, right->term.end, 2, left, right);
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
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
	unsigned int need_separator;
	unsigned int start, error;
	start = token->start;
	/* ( */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	AZONode *expr = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
	parser_push (parser, expr);
	need_separator = 0;
	while (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) {
		if (need_separator) {
			if (token->type != AZO_TOKEN_COMMA) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_node_free_tree (expr);
				return AZO_PARSER_ERROR_SYNTAX;
			}
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_node_free_tree (expr);
				return AZO_PARSER_ERROR_UNEXPECTED_EOF;
			}
		}
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return error;
		}
		need_separator = 1;
		if (token->type == AZO_TOKEN_EOF) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
	}
	expr->term.end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return AZO_PARSER_ERROR_NONE;
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

static unsigned int
parse_argument_definition (AZOParser *parser, AZOToken *token)
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
	} else {
		left = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, start, start);
		right = parser_detach_last (parser);
	}
	decl = azo_node_new_with_children (AZO_TERM_ARGUMENT_DECLARATION, AZO_TERM_GENERIC, start, token->start, 2, left, right);
	parser_append (parser, decl);
	return AZO_PARSER_ERROR_NONE;
}

static unsigned int
parse_arguments_definition (AZOParser *parser, AZOToken *token)
{
	unsigned int need_separator;
	unsigned int start, result;
	start = token->start;
	/* ( */
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_LEFT_PARENTHESIS) return AZO_PARSER_ERROR_SYNTAX;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;

	AZONode *expr = azo_node_new (AZO_TERM_LIST, AZO_TERM_GENERIC, start, token->end);
	parser_push (parser, expr);

	need_separator = 0;
	while (token->type != AZO_TOKEN_RIGHT_PARENTHESIS) {
		if (need_separator) {
			if (token->type != AZO_TOKEN_COMMA) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_node_free_tree (expr);
				return AZO_PARSER_ERROR_SYNTAX;
			}
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_node_free_tree (expr);
				return AZO_PARSER_ERROR_UNEXPECTED_EOF;
			}
		}
		result = parse_argument_definition (parser, token);
		if (result) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return result;
		}
		need_separator = 1;
	}
	expr->term.end = token->end;
	azo_tokenizer_get_next_token (&parser->tokenizer, token);
	parser_pop (parser);
	return AZO_PARSER_ERROR_NONE;
}

/*
 * RETURN_TYPE (ARGUMENTS)
 *
 * Current token points to return type
 */
static unsigned int
parse_signature_definition (AZOParser *parser, AZOToken *token, unsigned int *is_class)
{
	AZONode *expr;
	unsigned int error;
	/* Return type */
	if (azo_token_is_keyword (token, AZO_KEYWORD_FUNCTION, parser->src)) {
		/* function function (...) - i.e. function that returns a function */
		/* We create reference as it should be interpreted as type */
		expr = azo_node_new_reference (AZO_TERM_REFERENCE_VARIABLE, parser->src, token);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	} else if (azo_token_is_keyword (token, AZO_KEYWORD_VOID, parser->src)) {
		/* function void (...) */
		expr = azo_node_new (AZO_TERM_KEYWORD, AZO_KEYWORD_VOID, token->start, token->end);
		parser_append (parser, expr);
		if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	} else if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		/* function (...) - void return type */
		expr = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->end);
		parser_append (parser, expr);
	} else if (token->type == AZO_TOKEN_WORD) {
		/* function type (...) - type is given as an expression */
		/* fixme: We should exclude keywords here so function in 'function is any' construct is parsed as class */
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_FUNCTION);
		if (error) return error;
	} else if (token->type == AZO_TOKEN_RIGHT_PARENTHESIS) {
		/* ... function) - means function class */
		/* eg: if (a implements function) */
		*is_class = 1;
		return AZO_PARSER_ERROR_NONE;
	} else if (token->type == AZO_TOKEN_RIGHT_BRACKET) {
		/* ... function} - means function class */
		/* E.g. {1, "text, function"} */
		*is_class = 1;
		return AZO_PARSER_ERROR_NONE;
	} else if (token->type == AZO_TOKEN_OPERATOR) {
		/* function OP ... - means function class */
		/* E.g: function.name */
		*is_class = 1;
		return AZO_PARSER_ERROR_NONE;
	} else {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	/* Arguments */
	if (token->type == AZO_TOKEN_LEFT_PARENTHESIS) {
		error = parse_arguments_definition (parser, token);
		if (error) return error;
	} else {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	*is_class = 0;
	return AZO_PARSER_ERROR_NONE;
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
	AZONode *expr;
	unsigned int start, end, error, is_class;
	start = token->start;
	end = token->end;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	error = parse_signature_definition (parser, token, &is_class);
	if (error) return error;
	if (is_class) {
		/* function bareword */
		/* fixme: */
		expr = azo_node_new (AZO_TERM_REFERENCE, AZO_TERM_REFERENCE_VARIABLE, start, end);
		AZString *f = az_string_new ((const unsigned char *) "function");
		az_packed_value_set_string (&expr->value, f);
		parser_append (parser, expr);
		//if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		return AZO_PARSER_ERROR_NONE;
	}
	/* Statement/Block */
	if (token->type == AZO_TOKEN_LEFT_BRACE) {
		/* Definition */
		error = azo_parser_parse_sentence (parser, token);
		if (error) return error;
	} else {
		return AZO_PARSER_ERROR_SYNTAX;
	}
	AZONode *type, *args, *body;
	body = parser_detach_last (parser);
	args = parser_detach_last (parser);
	type = parser_detach_last (parser);
	if (is_member) {
		AZONode *obj = parser_detach_last (parser);
		expr = azo_node_new_with_children (AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_MEMBER, obj->term.start, body->term.end, 4, type, obj, args, body);
	} else {
		expr = azo_node_new_with_children (AZO_TERM_FUNCTION, AZO_TERM_FUNCTION_STATIC, type->term.start, body->term.end, 3, type, args, body);
	}
	parser_append (parser, expr);
	return AZO_PARSER_ERROR_NONE;
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
	AZONode *expr, *left, *right;
	unsigned int start, end, error;
	start = token->start;
	if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type == AZO_TOKEN_RIGHT_BRACKET) {
		/* Empty element - i.e. declaration */
		right = azo_node_new (AZO_TERM_EMPTY, AZO_TERM_GENERIC, token->start, token->start);
	} else {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_MINIMUM);
		if (error) return error;
		right = parser_detach_last (parser);
	}
	if (token->type == AZO_TOKEN_EOF) return AZO_PARSER_ERROR_UNEXPECTED_EOF;
	if (token->type != AZO_TOKEN_RIGHT_BRACKET) return AZO_PARSER_ERROR_SYNTAX;
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
	while (token->type != AZO_TOKEN_RIGHT_BRACE) {
		error = azo_parser_parse_expression (parser, token, AZO_PRECEDENCE_COMMA);
		if (error) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return error;
		}
		if (token->type == AZO_TOKEN_EOF) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return AZO_PARSER_ERROR_UNEXPECTED_EOF;
		}
		if (token->type == AZO_TOKEN_COMMA) {
			if (!azo_tokenizer_get_next_token (&parser->tokenizer, token)) {
				parser_pop (parser);
				parser_detach_last (parser);
				azo_node_free_tree (expr);
				return AZO_PARSER_ERROR_UNEXPECTED_EOF;
			}
		} else if (token->type != AZO_TOKEN_RIGHT_BRACE) {
			parser_pop (parser);
			parser_detach_last (parser);
			azo_node_free_tree (expr);
			return AZO_PARSER_ERROR_SYNTAX;
		}
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
* Current token is at keyword "new"
*/

/*
* Parse constructor
*   new EXPRESSION(ARGUMENTS)
*
* The class expression stops before the constructor argument list, so
* new MyObject().someMethod() constructs the object and then calls the method
*
* Current token is at keyword "new"
* After completing token points past the closing parenthesis
*/

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

