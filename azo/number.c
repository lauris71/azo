#define __AZO_NUMBER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <arikkei/arikkei-strlib.h>

#include <azo/node.h>

static AZONode *
azo_node_new_negative_integer (const AZOSource *src, const AZOToken *token)
{
	AZONode *expr;
	int64_t val;
	unsigned int len;
	len = arikkei_strtoll (src->cdata + token->start, token->end - token->start, &val);
	if (val >= INT32_MIN) {
		expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_INT32, token->start, token->end);
		az_packed_value_set_int (&expr->value, AZ_TYPE_INT32, ( int) val);
	} else {
		expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_INT64, token->start, token->end);
		az_packed_value_set_i64 (&expr->value, val);
	}
	return expr;
}

static AZONode *
azo_node_new_positive_integer (const AZOSource *src, const AZOToken *token)
{
	AZONode *expr;
	uint64_t val;
	unsigned int len;
	len = arikkei_strtoull (src->cdata + token->start, token->end - token->start, &val);
	if (val < INT32_MAX) {
		expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_INT32, token->start, token->end);
		az_packed_value_set_int (&expr->value, AZ_TYPE_INT32, ( int) val);
	} else if (val < INT64_MAX) {
		expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_INT64, token->start, token->end);
		az_packed_value_set_i64 (&expr->value, val);
	} else {
		expr = azo_node_new (AZO_TERM_CONSTANT, AZ_TYPE_UINT64, token->start, token->end);
		az_packed_value_set_u64 (&expr->value, val);
	}
	return expr;
}

AZONode *
azo_node_new_integer (const AZOSource *src, const AZOToken *token)
{
	if (src->cdata[token->start] == '-') {
		return azo_node_new_negative_integer (src, token);
	} else {
		return azo_node_new_positive_integer (src, token);
	}
}

AZONode *
azo_node_new_floating_point (const AZOSource *src, const AZOToken *token)
{
	const uint8_t *cdata = src->cdata + token->start;
	unsigned int clen = token->end - token->start;
	double val;
	unsigned int len = arikkei_strtod_exp (cdata, clen, &val);
	unsigned int is_float = 0;
	unsigned int is_imaginary = 0;
	while (len < clen) {
		if ((cdata[len] == 'f') || (cdata[len] == 'F')) is_float = 1;
		if ((cdata[len] == 'i') || (cdata[len] == 'I')) is_imaginary = 1;
		len += 1;
	}
	unsigned int az_type;
	AZValue az_val = {0};
	AZONode *expr;
	if (is_imaginary) {
		if (is_float) {
			az_type = AZ_TYPE_COMPLEX_FLOAT;
			az_val.cfloat_v.i = (float) val;
		} else {
			az_type = AZ_TYPE_COMPLEX_DOUBLE;
			az_val.cdouble_v.i = val;
		}
	} else {
		if (is_float) {
			az_type = AZ_TYPE_FLOAT;
			az_val.float_v = (float) val;
		} else {
			az_type = AZ_TYPE_DOUBLE;
			az_val.double_v = val;
		}
	}
	expr = azo_node_new (AZO_TERM_CONSTANT, az_type, token->start, token->end);
	az_packed_value_set_from_type_value (&expr->value, az_type, &az_val);
	return expr;
}

AZONode *
azo_node_new_number (const AZOSource *src, const AZOToken *token)
{
	if (token->type == AZO_TOKEN_INTEGER) {
		return azo_node_new_integer (src, token);
	} else if (token->type == AZO_TOKEN_FLOATING_POINT) {
		return azo_node_new_floating_point (src, token);
	}
	return NULL;
}
