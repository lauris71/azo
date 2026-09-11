#define __AZO_NODE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2021
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <arikkei/arikkei-strlib.h>

#include <az/string.h>

#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/source.h>

AZONode *
azo_node_new (unsigned int type, unsigned int subtype, unsigned int start, unsigned int end)
{
	AZONode *expr = (AZONode *) malloc (sizeof (AZONode));
	memset (expr, 0, sizeof (AZONode));
	expr->term = (AZOTerm) {type, 0, subtype, start, end};
	return expr;
}

AZONode *
azo_node_new_with_children(unsigned int type, unsigned int subtype, unsigned int start, unsigned int end, unsigned int n_children, ...)
{
	AZONode *node = azo_node_new(type, subtype, start, end);
	if (n_children > 0) {
		AZONode *last = NULL;
		va_list ap;
		va_start (ap, n_children);
		for (unsigned int i = 0; i < n_children; i++) {
			AZONode *child = va_arg (ap, AZONode *);
			/* Only the tail child may be NULL */
			assert (child || (i == n_children - 1));
			if (!last) {
				node->children = child;
			} else {
				last->next = child;
			}
			last = child;
		}
		va_end (ap);
	}
	return node;
}

void
azo_node_free (AZONode *expr)
{
	az_packed_value_clear (&expr->value);
	free (expr);
}

void
azo_node_free_tree (AZONode *expr)
{
	azo_node_clear_children(expr);
	azo_node_free (expr);
}

void
azo_node_clear_children (AZONode *expr)
{
	while (expr->children) {
		AZONode *next = expr->children->next;
		azo_node_free_tree (expr->children);
		expr->children = next;
	}
}

static unsigned int
node_flatten (AZONode *node, AZONode **nodes, unsigned int max_nodes, unsigned int pos)
{
	while (node) {
		if (pos < max_nodes) nodes[pos] = node;
		pos += 1;
		if (node->children) pos = node_flatten (node->children, nodes, max_nodes, pos);
		node = node->next;
	}
	return pos;
}

unsigned int
azo_node_flatten (AZONode *node, AZONode **nodes, unsigned int max_nodes)
{
	return node_flatten (node, nodes, max_nodes, 0);
}

AZONode *
azo_node_new_text (const AZOSource *src, const AZOToken *token)
{
	AZONode *expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_STRING, token->start + 1, token->end - 1);
	AZString *str = az_string_new_length (src->cdata + token->start + 1, token->end - token->start - 2);
	az_packed_value_transfer_string (&expr->value, str);
	return expr;
}

AZONode *
azo_node_new_reference (unsigned int subtype, const AZOSource *src, const AZOToken *token)
{
	AZONode *expr = azo_node_new (AZO_TERM_REFERENCE, subtype, token->start, token->end);
	AZString *str = az_string_new_length (src->cdata + token->start, token->end - token->start);
	az_packed_value_transfer_string (&expr->value, str);
	return expr;
}
int
azo_token_get_prefix_term(const AZOToken *token)
{
	if (AZO_TOKEN_IS_OPERATOR (token)) {
		switch (AZO_TOKEN_OPERATOR_CODE(token)) {
			case AZO_OPERATOR_PLUSPLUS:
				return AZO_TERM_PREFIX_INCREMENT;
			case AZO_OPERATOR_MINUSMINUS:
				return AZO_TERM_PREFIX_DECREMENT;
			case AZO_OPERATOR_PLUS:
				return AZO_TERM_PREFIX_PLUS;
			case AZO_OPERATOR_MINUS:
				return AZO_TERM_PREFIX_MINUS;
			case AZO_OPERATOR_NOT:
				return AZO_TERM_PREFIX_NOT;
			case AZO_OPERATOR_TILDE:
				return AZO_TERM_PREFIX_TILDE;
			default:
				break;
		}
	}
	return -1;
}

int
azo_token_get_assignment_term(const AZOToken *token)
{
	if (AZO_TOKEN_IS_OPERATOR (token)) {
		switch (AZO_TOKEN_OPERATOR_CODE(token)) {
			case AZO_OPERATOR_ASSIGN:
				return AZO_TERM_ASSIGN_PLAIN;
			case AZO_OPERATOR_PLUSASSIGN:
				return AZO_TERM_ASSIGN_PLUS;
			case AZO_OPERATOR_MINUSASSIGN:
				return AZO_TERM_ASSIGN_MINUS;
			case AZO_OPERATOR_SLASHASSIGN:
				return AZO_TERM_ASSIGN_SLASH;
			case AZO_OPERATOR_STARASSIGN:
				return AZO_TERM_ASSIGN_STAR;
			case AZO_OPERATOR_PERCENT_ASSIGN:
				return AZO_TERM_ASSIGN_PERCENT;
			case AZO_OPERATOR_SHIFT_LEFT_ASSIGN:
				return AZO_TERM_ASSIGN_SHIFT_LEFT;
			case AZO_OPERATOR_SHIFT_RIGHT_ASSIGN:
				return AZO_TERM_ASSIGN_SHIFT_RIGHT;
			case AZO_OPERATOR_AND_ASSIGN:
				return AZO_TERM_ASSIGN_AND;
			case AZO_OPERATOR_OR_ASSIGN:
				return AZO_TERM_ASSIGN_OR;
			case AZO_OPERATOR_CARET_ASSIGN:
				return AZO_TERM_ASSIGN_XOR;
			default:
				break;
		}
	}
	return -1;
}

int
azo_token_get_comparison_term(const AZOToken *token)
{
	if (AZO_TOKEN_IS_OPERATOR(token)) {
		switch (AZO_TOKEN_OPERATOR_CODE(token)) {
			case AZO_OPERATOR_EQUAL:
				return AZO_TERM_COMPARISON_E;
			case AZO_OPERATOR_NE:
				return AZO_TERM_COMPARISON_NE;
			case AZO_OPERATOR_IDENTICAL:
				return AZO_TERM_COMPARISON_IDENTICAL;
			case AZO_OPERATOR_NOT_IDENTICAL:
				return AZO_TERM_COMPARISON_NOT_IDENTICAL;
			case AZO_OPERATOR_GE:
				return AZO_TERM_COMPARISON_GE;
			case AZO_OPERATOR_GT:
				return AZO_TERM_COMPARISON_GT;
			case AZO_OPERATOR_LE:
				return AZO_TERM_COMPARISON_LE;
			case AZO_OPERATOR_LT:
				return AZO_TERM_COMPARISON_LT;
		}
	}
	return -1;
}

int
azo_token_get_binary_term(const AZOToken *token)
{
	if (AZO_TOKEN_IS_OPERATOR(token)) {
		switch (AZO_TOKEN_OPERATOR_CODE(token)) {
			case AZO_OPERATOR_PLUS:
				return AZO_TERM_ARITHMETIC_PLUS;
			case AZO_OPERATOR_MINUS:
				return AZO_TERM_ARITHMETIC_MINUS;
			case AZO_OPERATOR_SLASH:
				return AZO_TERM_ARITHMETIC_SLASH;
			case AZO_OPERATOR_STAR:
				return AZO_TERM_ARITHMETIC_STAR;
			case AZO_OPERATOR_PERCENT:
				return AZO_TERM_ARITHMETIC_PERCENT;
			case AZO_OPERATOR_SHIFT_LEFT:
				return AZO_TERM_ARITHMETIC_SHIFT_LEFT;
			case AZO_OPERATOR_SHIFT_RIGHT:
				return AZO_TERM_ARITHMETIC_SHIFT_RIGHT;
			case AZO_OPERATOR_ANDAND:
				return AZO_TERM_ARITHMETIC_ANDAND;
			case AZO_OPERATOR_AND:
				return AZO_TERM_ARITHMETIC_AND;
			case AZO_OPERATOR_OROR:
				return AZO_TERM_ARITHMETIC_OROR;
			case AZO_OPERATOR_OR:
				return AZO_TERM_ARITHMETIC_OR;
			case AZO_OPERATOR_CARET:
				return AZO_TERM_ARITHMETIC_CARET;
		}
	}
	return -1;
}

static void print_sentences (AZONode *expr, FILE *ofs);
static void print_sentence (AZONode *expr, FILE *ofs);

void
azo_node_print (AZONode *expr, FILE *ofs)
{
	static const char *suffixes[] = { "++", "--" };
	static const char *prefixes[] = { "++", "--", "+", "-", "!", "~" };
	static const char *arithmetics[] = { "+", "-", "/", "*", "%", "<<", ">>", "&", "&&", "|", "||", "^" };
	static const char *comparisons[] = { "==", "!=", "<", "<=", ">", ">=", "===", "!==" };
	static const char *assigns[] = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=" };
	static const char *tests[] = { "is", "implements" };
	uint8_t b[1024];
	AZClass *klass;
	AZONode *child;
	switch (expr->term.type) {
	case AZO_TERM_INVALID:
		fprintf (ofs, "INVALID ");
		break;
	case AZO_TERM_EMPTY:
		fprintf (ofs, "EMPTY ");
		break;
	case AZO_TERM_PROGRAM:
		fprintf (ofs, "BEGIN_PROGRAM\n");
		azo_node_print_list (expr->children, ofs, "\n");
		fprintf (ofs, "\nEND_PROGRAM\n");
		break;
	case AZO_TERM_BLOCK:
		fprintf (ofs, "{\n");
		azo_node_print_list (expr->children, ofs, "\n");
		fprintf (ofs, "}\n");
		break;
	case AZO_TERM_STATEMENT_GROUP:
		fprintf (ofs, "statement_group {\n");
		azo_node_print_list (expr->children, ofs, "\n");
		fprintf (ofs, "}\n");
		break;
	case AZO_TERM_KEYWORD:
		switch (expr->term.subtype) {
		case AZO_KEYWORD_NULL:
			fprintf (ofs, "null ");
			break;
		case AZO_KEYWORD_FOR:
			fprintf (ofs, "for (");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ";");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ";");
			azo_node_print (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			azo_node_print (expr->children->next->next->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_NEW:
			fprintf (ofs, "new ");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, "(");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ") ");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ") {\n");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {\n");
				azo_node_print (expr->children->next->next, ofs);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_RETURN:
			azo_print_keyword (expr->term.subtype, ofs);
			fprintf(ofs, " ");
			if (expr->children) {
				azo_node_print (expr->children, ofs);
			}
			break;
		default:
			azo_print_keyword (expr->term.subtype, ofs);
			/*azo_node_print_list (expr->children, ofs, " ");
			fprintf (ofs, "\n");*/
			break;
		}
		break;
	case AZO_TERM_DECLARATION:
		azo_node_print (expr->children, ofs);
		if (expr->children->next) {
			fprintf (ofs, "= ");
			azo_node_print (expr->children->next, ofs);
		}
		break;
	case AZO_TERM_DECLARATION_LIST:
		azo_node_print (expr->children, ofs);
		for (child = expr->children->next; child; child = child->next) {
			azo_node_print (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case AZO_TERM_ARGUMENT_DECLARATION:
		/* children: [type, name] - type is EMPTY for an untyped argument */
		if (expr->children->term.type != AZO_TERM_EMPTY) {
			azo_node_print (expr->children, ofs);
		}
		azo_node_print (expr->children->next, ofs);
		break;
	case AZO_TERM_FUNCTION:
		/* Lambda - children: [return_type, args, body] (static) or [return_type, object, args, body] (member) */
		if (expr->term.subtype == AZO_TERM_FUNCTION_MEMBER) {
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ".");
			azo_node_print (expr->children->next->next, ofs);
			fprintf (ofs, " ");
			if (expr->children->next->next->next) print_sentence (expr->children->next->next->next, ofs);
		} else {
			fprintf (ofs, "(");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ") ");
			if (expr->children->term.type != AZO_TERM_EMPTY) {
				azo_node_print (expr->children, ofs);
				fprintf (ofs, " ");
			}
			fprintf (ofs, "=> ");
			if (expr->children->next->next) print_sentence (expr->children->next->next, ofs);
		}
		break;
	case AZO_TERM_FUNCTION_CALL:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "(");
		azo_node_print (expr->children->next, ofs);
		fprintf (ofs, ")");
		break;
	case AZO_TERM_ARRAY_ELEMENT:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "[");
		azo_node_print (expr->children->next, ofs);
		fprintf (ofs, "]");
		break;
	case AZO_TERM_LIST:
		for (child = expr->children; child; child = child->next) {
			azo_node_print (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case AZO_TERM_REFERENCE:
		switch (expr->term.subtype) {
		case AZO_TERM_REFERENCE_VARIABLE:
		case AZO_TERM_REFERENCE_PROPERTY:
			fprintf (ofs, "%s", expr->value.v.string->str);
			fprintf (ofs, " ");
			break;
		case AZO_TERM_REFERENCE_MEMBER:
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ".");
			azo_node_print (expr->children->next, ofs);
			break;
		default:
			fprintf (ofs, "REFERENCE");
			break;
		}
		break;
	case AZO_TERM_LITERAL_ARRAY:
		fprintf (ofs, "{");
		for (child = expr->children; child; child = child->next) {
			azo_node_print (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		fprintf (ofs, "}");
		break;
	case AZO_TERM_CAST:
		/* children: [type, value] for both subtypes */
		if (expr->term.subtype == AZO_TERM_CAST_AS) {
			/* Checked class/interface conversion - value as type */
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, "as ");
			azo_node_print (expr->children, ofs);
		} else {
			/* Primitive value conversion - (type) value */
			fprintf (ofs, "(");
			azo_node_print (expr->children, ofs);
			if (expr->term.flags & AZO_TERM_FLAG_EXACT) fprintf (ofs, " exact");
			if (expr->term.flags & AZO_TERM_FLAG_ROUNDED) fprintf (ofs, " rounded");
			fprintf (ofs, ") ");
			azo_node_print (expr->children->next, ofs);
		}
		break;
	case AZO_TERM_TEST:
		/* children: [value, type] */
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "%s ", tests[expr->term.subtype]);
		azo_node_print (expr->children->next, ofs);
		break;
	case AZO_TERM_SELECT:
		/* children: [condition, iftrue, iffalse] */
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "? ");
		azo_node_print (expr->children->next, ofs);
		fprintf (ofs, ": ");
		azo_node_print (expr->children->next->next, ofs);
		break;
	case AZO_TERM_SUFFIX:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "%s", suffixes[expr->term.subtype]);
		fprintf (ofs, " ");
		break;
	case AZO_TERM_PREFIX:
		fprintf (ofs, "%s", prefixes[expr->term.subtype]);
		azo_node_print (expr->children, ofs);
		fprintf (ofs, " ");
		break;
	case AZO_TERM_BINARY:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "%s", arithmetics[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_node_print (expr->children->next, ofs);
		break;
	case AZO_TERM_COMPARISON:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "%s", comparisons[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_node_print (expr->children->next, ofs);
		break;
	case AZO_TERM_ASSIGN:
		azo_node_print (expr->children, ofs);
		fprintf (ofs, "%s", assigns[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_node_print (expr->children->next, ofs);
		break;
	case AZO_TERM_CONSTANT:
		az_instance_to_string(expr->value.impl, az_value_get_inst(expr->value.impl, &expr->value.v), b, 1024);
		fprintf (ofs, "%s ", b);
		break;
	case AZO_TERM_VARIABLE:
		fprintf (ofs, "##VAR(%u/%u) ", expr->term.type, expr->term.subtype);
		azo_node_print_list (expr->children, ofs, " ");
		fprintf (ofs, "## ");
		break;
	case AZO_TERM_TYPE:
		klass = AZ_CLASS_FROM_TYPE(expr->term.subtype);
		fprintf (ofs, "%s ", klass->name);
		break;
	default:
		fprintf (ofs, "##(%u/%u) ", expr->term.type, expr->term.subtype);
		azo_node_print_list (expr->children, ofs, " ");
		fprintf (ofs, "## ");
		break;
	}
}

static void
print_line (AZONode *expr, FILE *ofs)
{
	switch (expr->term.type) {
	case AZO_TERM_INVALID:
		fprintf (ofs, "INVALID;");
		break;
	case AZO_TERM_EMPTY:
		fprintf (ofs, "EMPTY;");
		break;
	case AZO_TERM_PROGRAM:
		fprintf (ofs, "BEGIN_PROGRAM\n");
		print_sentences (expr->children, ofs);
		fprintf (ofs, "END_PROGRAM");
		break;
	default:
		azo_node_print (expr, ofs);
	}
}

static void
print_sentence (AZONode *expr, FILE *ofs)
{
	switch (expr->term.type) {
	case AZO_TERM_BLOCK:
		fprintf (ofs, "{\n");
		print_sentences (expr->children, ofs);
		fprintf (ofs, "}\n");
		break;
	case AZO_TERM_KEYWORD:
		switch (expr->term.subtype) {
		case AZO_KEYWORD_FOR:
			fprintf (ofs, "for (");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ";");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ";");
			azo_node_print (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			azo_node_print (expr->children->next->next->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_WHILE:
			fprintf (ofs, "while (");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ") ");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ") {\n");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {");
				azo_node_print (expr->children->next->next, ofs);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		default:
			/* return, break, continue, do, ... */
			print_line (expr, ofs);
			break;
		}
		break;
	default:
		print_line (expr, ofs);
		break;
	}
}

static void
print_sentences (AZONode *expr, FILE *ofs)
{
	while (expr) {
		print_sentence (expr, ofs);
		fprintf (ofs, "\n");
		expr = expr->next;
	}
}

void
azo_node_print_list (AZONode *expr, FILE *ofs, const char *separator)
{
	while (expr) {
		azo_node_print (expr, ofs);
		if (expr->next) fprintf (ofs, "%s", separator);
		expr = expr->next;
	}
}

const char *expr_names[] = {
	"INVALID",
	"EMPTY",
	"PROGRAM",
	"BLOCK",
	"STATEMENT_GROUP",
	"KEYWORD",
	"DECLARATION_LIST",
	"DECLARATION",
	"ARGUMENT_DECLARATION",
	"FUNCTION",
	"FUNCTION_CALL",
	"ARRAY_ELEMENT",
	"LIST",
	"REFERENCE",
	"LITERAL_ARRAY",
	"CAST",

	"SUFFIX",
	"PREFIX",
	"BINARY",
	"COMPARISON",
	"ASSIGN",
	"TEST",
	"SELECT",

	"CONSTANT",
	"VARIABLE",
	"TYPE",
};

void
azo_node_print_info(AZONode *expr, FILE *ofs, AZOSource *src, unsigned int indent)
{
	for (unsigned int i = 0; i < indent; i++) fprintf(ofs, " ");
	uint8_t b[256];
	arikkei_utf8_strncpy_len_shorten(b, 255, src->cdata + expr->term.start, expr->term.end - expr->term.start);
	b[255] = 0;
	for (unsigned int i = 0; b[i]; i++) if (b[i] == '\n') b[i] = ' ';
	const char *name = (expr->term.type < AZO_NUM_TERM_TYPES) ? expr_names[expr->term.type] : "?";
	fprintf (ofs, "{%s:%u [%s] [%u,%u]", name, expr->term.subtype, b, expr->term.start, expr->term.end);
	if (expr->children) {
		fprintf(ofs, "\n");
		for (AZONode *child = expr->children; child; child = child->next) {
			azo_node_print_info(child, ofs, src, indent + 2);
		}
		for (unsigned int i = 0; i < indent; i++) fprintf(ofs, " ");
		fprintf(ofs, "}\n");
	} else {
		fprintf(ofs, "}\n");
	}
}
