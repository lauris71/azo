#define __AZO_NUMBER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <arikkei/arikkei-strlib.h>

#include <azo/expression.h>

static AZOExpression *
azo_expression_new_negative_integer (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr;
	int64_t val;
	unsigned int len;
	len = arikkei_strtoll (src->cdata + token->start, token->end - token->start, &val);
	if (val >= INT32_MIN) {
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_INT32, token->start, token->end);
		az_packed_value_set_int (&expr->value, AZ_TYPE_INT32, ( int) val);
	} else {
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_INT64, token->start, token->end);
		az_packed_value_set_i64 (&expr->value, val);
	}
	return expr;
}

static AZOExpression *
azo_expression_new_positive_integer (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr;
	uint64_t val;
	unsigned int len;
	len = arikkei_strtoull (src->cdata + token->start, token->end - token->start, &val);
	if (val < INT32_MAX) {
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_INT32, token->start, token->end);
		az_packed_value_set_int (&expr->value, AZ_TYPE_INT32, ( int) val);
	} else if (val < INT64_MAX) {
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_INT64, token->start, token->end);
		az_packed_value_set_i64 (&expr->value, val);
	} else {
		expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_UINT64, token->start, token->end);
		az_packed_value_set_u64 (&expr->value, val);
	}
	return expr;
}

AZOExpression *
azo_expression_new_integer (const AZOSource *src, const AZOToken *token)
{
	if (src->cdata[token->start] == '-') {
		return azo_expression_new_negative_integer (src, token);
	} else {
		return azo_expression_new_positive_integer (src, token);
	}
}

AZOExpression *
azo_expression_new_real (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr;
	double val;
	unsigned int len;
	len = arikkei_strtod_exp (src->cdata + token->start, token->end - token->start, &val);
	expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_DOUBLE, token->start, token->end);
	az_packed_value_set_double(&expr->value, val);
	return expr;
}

AZOExpression *
azo_expression_new_complex (const AZOSource *src, const AZOToken *token)
{
	AZOExpression *expr;
	AZComplexDouble val;
	unsigned int len;
	val.r = 0;
	len = arikkei_strtod_exp (src->cdata + token->start, token->end - token->start, &val.i);
	expr = azo_expression_new (EXPRESSION_CONSTANT, AZ_TYPE_COMPLEX_DOUBLE, token->start, token->end);
	az_packed_value_set_from_type_value (&expr->value, AZ_TYPE_COMPLEX_DOUBLE, (const AZValue *) &val);
	return expr;
}

AZOExpression *
azo_expression_new_number (const AZOSource *src, const AZOToken *token)
{
	if (token->type == AZO_TOKEN_INTEGER) {
		return azo_expression_new_integer (src, token);
	} else if (token->type == AZO_TOKEN_REAL) {
		return azo_expression_new_real (src, token);
	} else if (token->type == AZO_TOKEN_IMAGINARY) {
		return azo_expression_new_complex (src, token);
	}
	return NULL;
}
