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

#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/source.h>

AZONode *
azo_node_new (unsigned int type, unsigned int subtype, unsigned int start, unsigned int end)
{
	AZONode *expr = (AZONode *) malloc (sizeof (AZONode));
	memset (expr, 0, sizeof (AZONode));
	expr->term = (AZOTerm) {type, subtype, start, end};
	return expr;
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

static void print_sentences (AZONode *expr, FILE *ofs);
static void print_sentence (AZONode *expr, FILE *ofs);

void
azo_node_print (AZONode *expr, FILE *ofs)
{
	static const char *suffixes[] = { "++", "--" };
	static const char *prefixes[] = { "++", "--", "+", "-", "!" };
	static const char *arithmetics[] = { "+", "-", "/", "*", "%", "<<", ">>", "&", "&&", "|", "||", "^" };
	static const char *comparisons[] = { "==", "!=", "<", "<=", ">", ">=" };
	static const char *assigns[] = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" };
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
	case AZO_TERM_FUNCTION:
		if (expr->term.subtype == AZO_TERM_FUNCTION_MEMBER) {
			azo_node_print (expr->children, ofs);
			fprintf (ofs, ".function ");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, "(");
			azo_node_print (expr->children->next->next, ofs);
			fprintf (ofs, ") ");
			if (expr->children->next->next->next) {
				print_sentence (expr->children->next->next->next, ofs);
			}
		} else {
			fprintf (ofs, "function ");
			azo_node_print (expr->children, ofs);
			fprintf (ofs, "(");
			azo_node_print (expr->children->next, ofs);
			fprintf (ofs, ") ");
			if (expr->children->next->next) {
				print_sentence (expr->children->next->next, ofs);
			}
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
			azo_node_print (expr->children->next->next->next, ofs);
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
	"AZO_TERM_ASSIGN_PLAIN",
	"COMMA",
	"TEST",

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
	fprintf (ofs, "{%s:%u [%s] [%u,%u]", expr_names[expr->term.type], expr->term.subtype, b, expr->term.start, expr->term.end);
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
