#define __AZO_EXPRESSION_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2021
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arikkei/arikkei-strlib.h>

#include <az/string.h>

#include <azo/expression.h>
#include <azo/keyword.h>
#include <azo/source.h>

AZOExpression *
azo_expression_new (unsigned int type, unsigned int subtype, unsigned int start, unsigned int end)
{
	AZOExpression *expr = (AZOExpression *) malloc (sizeof (AZOExpression));
	memset (expr, 0, sizeof (AZOExpression));
	expr->term = (AZOTerm) {type, subtype, start, end};
	return expr;
}

void
azo_expression_free (AZOExpression *expr)
{
	az_packed_value_clear (&expr->value);
	free (expr);
}

void
azo_expression_free_tree (AZOExpression *expr)
{
	azo_expression_clear_children(expr);
	azo_expression_free (expr);
}

void
azo_expression_clear_children (AZOExpression *expr)
{
	while (expr->children) {
		AZOExpression *next = expr->children->next;
		azo_expression_free_tree (expr->children);
		expr->children = next;
	}
}

AZOExpression *
azo_expression_clone_tree (AZOExpression *expr)
{
	AZOExpression *clone, *child, *prev;
	clone = azo_expression_new (expr->term.type, expr->term.subtype, expr->term.start, expr->term.end);
	if (expr->value.impl) {
		az_packed_value_copy (&clone->value, &expr->value);
	}
	prev = NULL;
	for (child = expr->children; child; child = child->next) {
		AZOExpression *cloned_child;
		cloned_child = azo_expression_clone_tree (child);
		if (!prev) {
			clone->children = cloned_child;
		} else {
			prev->next = cloned_child;
		}
		prev = cloned_child;
	}
	return clone;
}

unsigned int
azo_expression_count_nodes(AZOExpression *tree)
{
	unsigned int count = 1;
	for (AZOExpression *child = tree->children; child; child = child->next) {
		count += azo_expression_count_nodes(child);
	}
	return count;
}

AZOExpression *
azo_expression_new_text (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_STRING, token->start + 1, token->end - 1);
	AZString *str = az_string_new_length (src->cdata + token->start + 1, token->end - token->start - 2);
	az_packed_value_transfer_string (&expr->value, str);
	return expr;
}

AZOExpression *
azo_expression_new_reference (unsigned int subtype, const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr = azo_expression_new (EXPRESSION_REFERENCE, subtype, token->start, token->end);
	AZString *str = az_string_new_length (src->cdata + token->start, token->end - token->start);
	az_packed_value_transfer_string (&expr->value, str);
	return expr;
}

static void print_sentences (AZOExpression *expr, FILE *ofs);
static void print_sentence (AZOExpression *expr, FILE *ofs);

void
azo_print_expression (AZOExpression *expr, FILE *ofs)
{
	static const char *suffixes[] = { "++", "--" };
	static const char *prefixes[] = { "++", "--", "+", "-", "!" };
	static const char *arithmetics[] = { "+", "-", "/", "*", "%", "<<", ">>", "&", "&&", "|", "||", "^" };
	static const char *comparisons[] = { "==", "!=", "<", "<=", ">", ">=" };
	static const char *assigns[] = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" };
	uint8_t b[1024];
	AZClass *klass;
	AZOExpression *child;
	switch (expr->term.type) {
	case AZO_TERM_INVALID:
		fprintf (ofs, "INVALID ");
		break;
	case AZO_TERM_EMPTY:
		fprintf (ofs, "EMPTY ");
		break;
	case AZO_EXPRESSION_PROGRAM:
		fprintf (ofs, "BEGIN_PROGRAM\n");
		azo_print_expression_list (expr->children, ofs, "\n");
		fprintf (ofs, "\nEND_PROGRAM\n");
		break;
	case AZO_EXPRESSION_BLOCK:
		fprintf (ofs, "{\n");
		azo_print_expression_list (expr->children, ofs, "\n");
		fprintf (ofs, "}\n");
		break;
	case EXPRESSION_KEYWORD:
		switch (expr->term.subtype) {
		case AZO_KEYWORD_NULL:
			fprintf (ofs, "null ");
			break;
		case AZO_KEYWORD_FOR:
			fprintf (ofs, "for (");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_NEW:
			fprintf (ofs, "new ");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, ") ");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ") {\n");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {\n");
				azo_print_expression (expr->children->next->next, ofs);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_RETURN:
			azo_print_keyword (expr->term.subtype, ofs);
			fprintf(ofs, " ");
			if (expr->children) {
				azo_print_expression (expr->children, ofs);
			}
			break;
		default:
			azo_print_keyword (expr->term.subtype, ofs);
			/*azo_print_expression_list (expr->children, ofs, " ");
			fprintf (ofs, "\n");*/
			break;
		}
		break;
	case EXPRESSION_DECLARATION:
		azo_print_expression (expr->children, ofs);
		if (expr->children->next) {
			fprintf (ofs, "= ");
			azo_print_expression (expr->children->next, ofs);
		}
		break;
	case EXPRESSION_DECLARATION_LIST:
		azo_print_expression (expr->children, ofs);
		for (child = expr->children->next; child; child = child->next) {
			azo_print_expression (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case EXPRESSION_FUNCTION:
		if (expr->term.subtype == FUNCTION_MEMBER) {
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ".function ");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			if (expr->children->next->next->next) {
				print_sentence (expr->children->next->next->next, ofs);
			}
		} else {
			fprintf (ofs, "function ");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, ") ");
			if (expr->children->next->next) {
				print_sentence (expr->children->next->next, ofs);
			}
		}
		break;
	case EXPRESSION_FUNCTION_CALL:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "(");
		azo_print_expression (expr->children->next, ofs);
		fprintf (ofs, ")");
		break;
	case EXPRESSION_ARRAY_ELEMENT:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "[");
		azo_print_expression (expr->children->next, ofs);
		fprintf (ofs, "]");
		break;
	case EXPRESSION_LIST:
		for (child = expr->children; child; child = child->next) {
			azo_print_expression (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case EXPRESSION_REFERENCE:
		switch (expr->term.subtype) {
		case REFERENCE_VARIABLE:
			fprintf (ofs, "%s", expr->value.v.string->str);
			fprintf (ofs, " ");
			break;
		case REFERENCE_MEMBER:
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ".");
			azo_print_expression (expr->children->next, ofs);
			break;
		default:
			fprintf (ofs, "REFERENCE");
			break;
		}
		break;
	case EXPRESSION_LITERAL_ARRAY:
		fprintf (ofs, "{");
		for (child = expr->children; child; child = child->next) {
			azo_print_expression (child, ofs);
			if (child->next) fprintf (ofs, ", ");
		}
		fprintf (ofs, "}");
		break;
	case EXPRESSION_SUFFIX:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "%s", suffixes[expr->term.subtype]);
		fprintf (ofs, " ");
		break;
	case EXPRESSION_PREFIX:
		fprintf (ofs, "%s", prefixes[expr->term.subtype]);
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, " ");
		break;
	case EXPRESSION_BINARY:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "%s", arithmetics[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs);
		break;
	case EXPRESSION_COMPARISON:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "%s", comparisons[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs);
		break;
	case EXPRESSION_ASSIGN:
		azo_print_expression (expr->children, ofs);
		fprintf (ofs, "%s", assigns[expr->term.subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs);
		break;
	case EXPRESSION_CONSTANT:
		az_instance_to_string(expr->value.impl, az_value_get_inst(expr->value.impl, &expr->value.v), b, 1024);
		fprintf (ofs, "%s ", b);
		break;
	case EXPRESSION_VARIABLE:
		fprintf (ofs, "##VAR(%u/%u) ", expr->term.type, expr->term.subtype);
		azo_print_expression_list (expr->children, ofs, " ");
		fprintf (ofs, "## ");
		break;
	case EXPRESSION_TYPE:
		klass = AZ_CLASS_FROM_TYPE(expr->term.subtype);
		fprintf (ofs, "%s ", klass->name);
		break;
	default:
		fprintf (ofs, "##(%u/%u) ", expr->term.type, expr->term.subtype);
		azo_print_expression_list (expr->children, ofs, " ");
		fprintf (ofs, "## ");
		break;
	}
}

static void
print_line (AZOExpression *expr, FILE *ofs)
{
	switch (expr->term.type) {
	case AZO_TERM_INVALID:
		fprintf (ofs, "INVALID;");
		break;
	case AZO_TERM_EMPTY:
		fprintf (ofs, "EMPTY;");
		break;
	case AZO_EXPRESSION_PROGRAM:
		fprintf (ofs, "BEGIN_PROGRAM\n");
		print_sentences (expr->children, ofs);
		fprintf (ofs, "END_PROGRAM");
		break;
	default:
		azo_print_expression (expr, ofs);
	}
}

static void
print_sentence (AZOExpression *expr, FILE *ofs)
{
	switch (expr->term.type) {
	case AZO_EXPRESSION_BLOCK:
		fprintf (ofs, "{\n");
		print_sentences (expr->children, ofs);
		fprintf (ofs, "}\n");
		break;
	case EXPRESSION_KEYWORD:
		switch (expr->term.subtype) {
		case AZO_KEYWORD_FOR:
			fprintf (ofs, "for (");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_WHILE:
			fprintf (ofs, "while (");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_print_expression (expr->children, ofs);
			fprintf (ofs, ") {\n");
			azo_print_expression (expr->children->next, ofs);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {");
				azo_print_expression (expr->children->next->next, ofs);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		}
		break;
	default:
		print_line (expr, ofs);
		break;
	}
}

static void
print_sentences (AZOExpression *expr, FILE *ofs)
{
	while (expr) {
		print_sentence (expr, ofs);
		fprintf (ofs, "\n");
		expr = expr->next;
	}
}

void
azo_print_expression_list (AZOExpression *expr, FILE *ofs, const char *separator)
{
	while (expr) {
		azo_print_expression (expr, ofs);
		if (expr->next) fprintf (ofs, "%s", separator);
		expr = expr->next;
	}
}

const char *expr_names[] = {
	"INVALID",
	"EMPTY",
	"PROGRAM",
	"BLOCK",
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
	"COMMA",
	"TEST",

	"CONSTANT",
	"VARIABLE",
	"TYPE",
};

void
azo_expression_print_info(AZOExpression *expr, FILE *ofs, AZOSource *src, unsigned int indent)
{
	for (unsigned int i = 0; i < indent; i++) fprintf(ofs, " ");
	uint8_t b[256];
	arikkei_utf8_strncpy_len_shorten(b, 255, src->cdata + expr->term.start, expr->term.end - expr->term.start);
	b[255] = 0;
	for (unsigned int i = 0; b[i]; i++) if (b[i] == '\n') b[i] = ' ';
	fprintf (ofs, "{%s:%u [%s] [%u,%u]", expr_names[expr->term.type], expr->term.subtype, b, expr->term.start, expr->term.end);
	if (expr->children) {
		fprintf(ofs, "\n");
		for (AZOExpression *child = expr->children; child; child = child->next) {
			azo_expression_print_info(child, ofs, src, indent + 2);
		}
		for (unsigned int i = 0; i < indent; i++) fprintf(ofs, " ");
		fprintf(ofs, "}\n");
	} else {
		fprintf(ofs, "}\n");
	}
}
