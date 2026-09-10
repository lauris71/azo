#define __AZO_NUMBER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <arikkei/arikkei-strlib.h>

#include <azo/node.h>

/*
 * Parse the integer literal digits (the tokenizer guarantees a valid form)
 * Returns the value and the number of characters parsed (the rest are suffixes)
 */

static uint64_t
parse_integer_digits (const uint8_t *cdata, unsigned int clen, unsigned int base, unsigned int *n_digits)
{
	uint64_t val = 0;
	unsigned int p = 0;
	while (p < clen) {
		unsigned int digit;
		uint8_t c = cdata[p];
		if ((c >= '0') && (c <= '9')) {
			digit = c - '0';
		} else if ((c >= 'a') && (c <= 'f')) {
			digit = c - 'a' + 10;
		} else if ((c >= 'A') && (c <= 'F')) {
			digit = c - 'A' + 10;
		} else {
			break;
		}
		if (digit >= base) break;
		/* On overflow simply saturate (the value is already out of range of every type) */
		if (val > (UINT64_MAX - digit) / base) {
			val = UINT64_MAX;
		} else {
			val = val * base + digit;
		}
		p += 1;
	}
	if (n_digits) *n_digits = p;
	return val;
}

AZONode *
azo_node_new_integer (const AZOSource *src, const AZOToken *token)
{
	const uint8_t *cdata = src->cdata + token->start;
	unsigned int clen = token->end - token->start;
	/* - and + are always operators, an integer token never starts with either */
	unsigned int base = 10;
	unsigned int p = 0;
	if ((clen > 2) && (cdata[0] == '0')) {
		if ((cdata[1] == 'x') || (cdata[1] == 'X')) {
			base = 16;
			p = 2;
		} else if ((cdata[1] == 'b') || (cdata[1] == 'B')) {
			base = 2;
			p = 2;
		}
	}
	unsigned int n_digits;
	uint64_t val = parse_integer_digits (cdata + p, clen - p, base, &n_digits);
	p += n_digits;
	/* Suffixes: u - unsigned, l - 64 bit (default is 32 bit) */
	unsigned int is_unsigned = 0;
	unsigned int is_long = 0;
	while (p < clen) {
		if ((cdata[p] == 'u') || (cdata[p] == 'U')) is_unsigned = 1;
		if ((cdata[p] == 'l') || (cdata[p] == 'L')) is_long = 1;
		p += 1;
	}
	/* Pick the type by suffix and magnitude (auto-widen if the value does not fit) */
	unsigned int az_type;
	if (!is_long) {
		if (!is_unsigned) {
			if (val <= INT32_MAX) {
				az_type = AZ_TYPE_INT32;
			} else if (val <= INT64_MAX) {
				az_type = AZ_TYPE_INT64;
			} else {
				az_type = AZ_TYPE_UINT64;
			}
		} else {
			az_type = (val <= UINT32_MAX) ? AZ_TYPE_UINT32 : AZ_TYPE_UINT64;
		}
	} else {
		if (!is_unsigned) {
			az_type = (val <= INT64_MAX) ? AZ_TYPE_INT64 : AZ_TYPE_UINT64;
		} else {
			az_type = AZ_TYPE_UINT64;
		}
	}
	AZONode *expr = azo_node_new (AZO_TERM_CONSTANT, az_type, token->start, token->end);
	if (az_type == AZ_TYPE_INT32) {
		az_packed_value_set_int (&expr->value, AZ_TYPE_INT32, (int) val);
	} else if (az_type == AZ_TYPE_UINT32) {
		az_packed_value_set_unsigned_int (&expr->value, AZ_TYPE_UINT32, (unsigned int) val);
	} else if (az_type == AZ_TYPE_INT64) {
		az_packed_value_set_i64 (&expr->value, (int64_t) val);
	} else {
		az_packed_value_set_u64 (&expr->value, val);
	}
	return expr;
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
	if (AZO_TOKEN_IS_INTEGER (token)) {
		return azo_node_new_integer (src, token);
	} else if (token->type == AZO_TOKEN_FLOATING_POINT) {
		return azo_node_new_floating_point (src, token);
	}
	return NULL;
}
