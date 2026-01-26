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
	expr->type = type;
	expr->subtype = subtype;
	expr->start = start;
	expr->end = end;
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
	while (expr->children) {
		AZOExpression *next = expr->children->next;
		azo_expression_free_tree (expr->children);
		expr->children = next;
	}
	azo_expression_free (expr);
}

AZOExpression *
azo_expression_clone_tree (AZOExpression *expr)
{
	AZOExpression *clone, *child, *prev;
	clone = azo_expression_new (expr->type, expr->subtype, expr->start, expr->end);
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

AZOExpression *
azo_expression_new_text (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_STRING, token->start, token->end);
	AZString *str = az_string_new_length (src->cdata + token->start, token->end - token->start);
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

static void print_sentences (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level);
static void print_sentence (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level);

static void
print_indent (FILE *ofs, unsigned int level)
{
	unsigned int i;
	for (i = 0; i < level; i++) fprintf (ofs, "    ");
}

static void
azo_print_expression (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level)
{
	static const char *suffixes[] = { "++", "--" };
	static const char *prefixes[] = { "++", "--", "+", "-", "!" };
	static const char *arithmetics[] = { "+", "-", "/", "*", "%", "<<", ">>", "&", "&&", "|", "||", "^" };
	static const char *comparisons[] = { "==", "!=", "<", "<=", ">", ">=" };
	static const char *assigns[] = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" };
	AZOExpression *child;
	switch (expr->type) {
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
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "{\n");
		azo_print_expression_list (expr->children, ofs, "\n");
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "}\n");
		break;
	case EXPRESSION_KEYWORD:
		switch (expr->subtype) {
		case AZO_KEYWORD_NULL:
			fprintf (ofs, "null ");
			break;
		case AZO_KEYWORD_FOR:
			fprintf (ofs, "for (");
			azo_print_expression (expr->children, ofs, indent, level);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next, ofs, indent, level);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next->next, ofs, indent, level);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs, indent, level);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_NEW:
			fprintf (ofs, "new ");
			azo_print_expression (expr->children, ofs, indent, level);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next, ofs, indent, level);
			fprintf (ofs, ") ");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_print_expression (expr->children, ofs, indent, level);
			fprintf (ofs, ") {\n");
			azo_print_expression (expr->children->next, ofs, indent, level);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {");
				azo_print_expression (expr->children->next->next, ofs, indent, level);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		default:
			azo_print_keyword (expr->subtype, ofs);
			/*azo_print_expression_list (expr->children, ofs, " ");
			fprintf (ofs, "\n");*/
			break;
		}
		break;
	case EXPRESSION_DECLARATION:
		azo_print_expression (expr->children, ofs, indent, level);
		if (expr->children->next) {
			fprintf (ofs, "= ");
			azo_print_expression (expr->children->next, ofs, indent, level);
		}
		break;
	case EXPRESSION_DECLARATION_LIST:
		azo_print_expression (expr->children, ofs, indent, level);
		for (child = expr->children->next; child; child = child->next) {
			azo_print_expression (child, ofs, indent, level);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case EXPRESSION_FUNCTION:
		if (expr->subtype == FUNCTION_MEMBER) {
			if (indent) print_indent (ofs, level);
			azo_print_expression (expr->children, ofs, 0, level);
			fprintf (ofs, ".function ");
			azo_print_expression (expr->children->next, ofs, 0, level);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next->next, ofs, 0, level);
			fprintf (ofs, ") ");
			if (expr->children->next->next->next) {
				print_sentence (expr->children->next->next->next, ofs, 1, level + 1);
			}
		} else {
			if (indent) print_indent (ofs, level);
			fprintf (ofs, "function ");
			azo_print_expression (expr->children, ofs, indent, level);
			fprintf (ofs, "(");
			azo_print_expression (expr->children->next, ofs, indent, level);
			fprintf (ofs, ") ");
			if (expr->children->next->next) {
				print_sentence (expr->children->next->next, ofs, 1, level + 1);
			}
		}
		break;
	case EXPRESSION_FUNCTION_CALL:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "(");
		azo_print_expression (expr->children->next, ofs, indent, level);
		fprintf (ofs, ")");
		break;
	case EXPRESSION_ARRAY_ELEMENT:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "[");
		azo_print_expression (expr->children->next, ofs, indent, level);
		fprintf (ofs, "]");
		break;
	case EXPRESSION_LIST:
		for (child = expr->children; child; child = child->next) {
			azo_print_expression (child, ofs, indent, level);
			if (child->next) fprintf (ofs, ", ");
		}
		break;
	case EXPRESSION_REFERENCE:
		switch (expr->subtype) {
		case REFERENCE_VARIABLE:
			fprintf (ofs, "%s", expr->value.v.string->str);
			fprintf (ofs, " ");
			break;
		case REFERENCE_MEMBER:
			azo_print_expression (expr->children, ofs, indent, level);
			fprintf (ofs, ".");
			azo_print_expression (expr->children->next, ofs, indent, level);
			break;
		default:
			fprintf (ofs, "REFERENCE");
			break;
		}
		break;
	case EXPRESSION_LITERAL_ARRAY:
		fprintf (ofs, "{");
		for (child = expr->children; child; child = child->next) {
			azo_print_expression (child, ofs, indent, level);
			if (child->next) fprintf (ofs, ",");
		}
		fprintf (ofs, "}");
		break;
	case EXPRESSION_SUFFIX:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "%s", suffixes[expr->subtype]);
		fprintf (ofs, " ");
		break;
	case EXPRESSION_PREFIX:
		fprintf (ofs, "%s", prefixes[expr->subtype]);
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, " ");
		break;
	case EXPRESSION_BINARY:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "%s", arithmetics[expr->subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs, indent, level);
		break;
	case EXPRESSION_COMPARISON:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "%s", comparisons[expr->subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs, indent, level);
		break;
	case EXPRESSION_ASSIGN:
		azo_print_expression (expr->children, ofs, indent, level);
		fprintf (ofs, "%s", assigns[expr->subtype]);
		fprintf (ofs, " ");
		azo_print_expression (expr->children->next, ofs, indent, level);
		break;
	default:
		fprintf (ofs, "##(%u/%u) ", expr->type, expr->subtype);
		azo_print_expression_list (expr->children, ofs, " ");
		fprintf (ofs, "## ");
		break;
	}
}

static void
print_line (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level)
{
	switch (expr->type) {
	case AZO_TERM_INVALID:
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "INVALID;");
		break;
	case AZO_TERM_EMPTY:
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "EMPTY;");
		break;
	case AZO_EXPRESSION_PROGRAM:
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "BEGIN_PROGRAM\n");
		print_sentences (expr->children, ofs, 1, level + 1);
		print_indent (ofs, level);
		fprintf (ofs, "END_PROGRAM");
		break;
	default:
		azo_print_expression (expr, ofs, indent, level);
	}
}

static void
print_sentence (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level)
{
	switch (expr->type) {
	case AZO_EXPRESSION_BLOCK:
		if (indent) print_indent (ofs, level);
		fprintf (ofs, "{\n");
		print_sentences (expr->children, ofs, 1, level + 1);
		print_indent (ofs, level);
		fprintf (ofs, "}\n");
		break;
	case EXPRESSION_KEYWORD:
		switch (expr->subtype) {
		case AZO_KEYWORD_FOR:
			if (indent) print_indent (ofs, level);
			fprintf (ofs, "for (");
			azo_print_expression (expr->children, ofs, 0, level);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next, ofs, 0, level);
			fprintf (ofs, ";");
			azo_print_expression (expr->children->next->next, ofs, 0, level);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs, 0, level);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_WHILE:
			if (indent) print_indent (ofs, level);
			fprintf (ofs, "for (");
			azo_print_expression (expr->children, ofs, 0, level);
			fprintf (ofs, ") ");
			azo_print_expression (expr->children->next->next->next, ofs, 0, level);
			fprintf (ofs, "\n");
			break;
		case AZO_KEYWORD_IF:
			fprintf (ofs, "if (");
			azo_print_expression (expr->children, ofs, 0, level);
			fprintf (ofs, ") {\n");
			azo_print_expression (expr->children->next, ofs, 0, level);
			fprintf (ofs, " }");
			if (expr->children->next->next) {
				fprintf (ofs, " else {");
				azo_print_expression (expr->children->next->next, ofs, 0, level);
				fprintf (ofs, " }");
			}
			fprintf (ofs, "\n");
			break;
		}
		break;
	default:
		print_line (expr, ofs, 0, level);
		break;
	}
}

static void
print_sentences (AZOExpression *expr, FILE *ofs, unsigned int indent, unsigned int level)
{
	while (expr) {
		print_sentence (expr, ofs, indent, level);
		fprintf (ofs, "\n");
		expr = expr->next;
	}
}

void
azo_print_expression_list (AZOExpression *expr, FILE *ofs, const char *separator)
{
	while (expr) {
		azo_print_expression (expr, ofs, 0, 0);
		if (expr->next) fprintf (ofs, "%s", separator);
		expr = expr->next;
	}
}

