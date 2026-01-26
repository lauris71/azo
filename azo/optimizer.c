#define __AZO_OPTIMIZER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/


#define debug 0
#define debug_literals 0
#define debug_calculate 0
#define debug_references 0

typedef struct _AZOOptimizer AZOOptimizer;

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <arikkei/arikkei-strlib.h>

#include <az/class.h>
#include <az/complex.h>
#include <az/field.h>
#include <az/string.h>
#include <az/primitives.h>

#include <azo/keyword.h>
#include <azo/optimizer.h>

struct _AZOOptimizer {
	const AZOSource *src;
};

static unsigned int
token_is_equal (AZOOptimizer *opt, AZOExpression *expr, const char *word, const unsigned char *cdata)
{
	unsigned int i;
	for (i = expr->term.start; i < expr->term.end; i++) {
		if (opt->src->cdata[i] != word[i]) return 0;
	}
	return 1;
}

#if 0
static AZOExpression *
resolve_member_reference (AZOOptimizer *opt, AZOExpression *expr)
{
	AZOExpression *lhs, *rhs;
	AZPackedValue val;

	lhs = expr->children;
	rhs = lhs->next;
	lhs = resolve (opt, lhs);
	rhs = resolve (opt, rhs);
	if (lhs->type != EXPRESSION_CONSTANT) return expr;
	if ((rhs->type != EXPRESSION_REFERENCE) || (rhs->subtype != REFERENCE_VARIABLE)) {
		fprintf (stderr, "resolve_member_reference: Invalid rhs type %u/%u\n", rhs->type, rhs->subtype);
		expr->type = EXPRESSION_INVALID;
		return expr;
	}
	if (!az_instance_get_property (lhs->value.impl, az_instance_from_value (lhs->value.impl, &lhs->value.v), rhs->value.v.string->str, &val)) {
		AZClass *klass = az_type_get_class (lhs->value.impl->type);
		fprintf (stderr, "resolve_member_reference: Invalid member %s of constant value of type %s\n", rhs->value.v.string->str, klass->name);
		expr->type = EXPRESSION_INVALID;
		return expr;
	}
	az_packed_value_set_from_impl_value (&expr->value, val.impl, &val.v);
	expr->type = EXPRESSION_CONSTANT;
	expr->subtype = val.impl->type;
	az_packed_value_clear (&val);
	expr->children = NULL;
	azo_expression_free (lhs);
	azo_expression_free (rhs);
	return expr;
}

static unsigned int
resolve_references (AZOOptimizer *opt, AZOExpression *expr, AZPackedValue *thisval, const unsigned char *cdata)
{
	AZOExpression *child;
	unsigned int result;
	result = 0;
	for (child = expr->children; child; child = child->next) {
		result = result || resolve_references (opt, child, thisval, cdata);
	}
	if (expr->type == EXPRESSION_REFERENCE) {
		if (expr->subtype == REFERENCE_VARIABLE) {
			/* fixme: Look for variables */
			const unsigned char *word = opt->src->cdata + expr->start;
			unsigned int len = expr->end - expr->start;
			if (thisval->impl && thisval->impl->type && (len < 1024)) {
				AZClass *this_klass;
				void *this_inst;
				AZImplementation *prop_impl;
				void *prop_inst;
				AZField *prop;
				unsigned char c[1024];
				int idx;

				memcpy (c, word, len);
				c[len] = 0;

				this_klass = az_type_get_class (thisval->impl->type);
				this_inst = az_packed_value_get_instance (thisval);
				idx = az_lookup_property (this_klass, &this_klass->implementation, this_inst, c, &prop_impl, &prop_inst, &prop);
				if ((idx >= 0) && prop->is_final) {
					AZPackedValue propval[4] = { 0 };
					if (prop->read == AZ_FIELD_READ_NONE) {
						fprintf (stderr, "resolve_references: Property %s is not readable\n", c);
						return 0;
					}
					if (az_instance_get_property_by_id (prop_impl, prop_inst, idx, &propval[0].impl, &propval[0].v, NULL)) {
#if 0
						expr->is_const = 1;
#endif
						az_packed_value_copy (&expr->value, &propval[0]);
						if (debug_references) {
							AZClass *kl = az_type_get_class (propval[0].impl->type);
							fprintf (stderr, "resolve_references: Resolved literal string %s to %s\n", c, kl->name);
						}
					}
				}
				/* fixme: If not final we can still detect type */
			}
		} else if (expr->subtype == REFERENCE_MEMBER) {
		}
	}
	return result;
}
#endif

