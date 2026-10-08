#define __AZO_COMPILER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

static const int debug = 0;

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <arikkei/arikkei-iolib.h>
#include <arikkei/arikkei-strlib.h>
#include <az/classes/active-object.h>
#include <az/class.h>
#include <az/string.h>
#include <az/function.h>
#include <az/function-value.h>
#include <az/field.h>

#include <azo/parser.h>
#include <azo/compiled-function.h>
/* Bytecodes */
#include <azo/bytecode.h>
#include <azo/keyword.h>
#include <azo/errors.h>

#include <azo/compiler/arithmetic.h>
#include <azo/compiler/helpers.h>
#include <azo/compare.h>

#include <azo/compiler/compiler.h>

typedef struct _LValue LValue;

enum {
	/* Stack variable, left is relative position */
	LVALUE_STACK,
	/* Property or attribute, stack(1) is object, stack(0) is property name */
	LVALUE_PROPERTY,
	/* Attribute (arrow operator), stack(1) is object, stack(0) is attribute name */
	LVALUE_ATTRIBUTE,
	/* Property or attribute, stack(1) is object, stack(0) is the name */
	LVALUE_PROPERTY_OR_ATTRIBUTE,
	/* Array element, stack(1) is array, stack(0) is index */
	LVALUE_ELEMENT,
	/* Constants */
	LVALUE_SHARED,
	LVALUE_CAPTURE
};

struct _LValue {
	unsigned int type;
	/* Variable location */
	unsigned int pos;
	/* Number of elements pushed */
	unsigned int n_elements;
};

void
azo_compiler_setup(AZOCompiler *compiler, AZOContext *globals, AZOSource *src)
{
	memset (compiler, 0, sizeof (AZOCompiler));
	compiler->globals = globals;
	compiler->src = src;
	az_object_ref((AZObject *) src);
	compiler->check_args = 1;
}

void
azo_compiler_release(AZOCompiler *compiler)
{
	if (compiler->src) az_object_unref((AZObject *) compiler->src);
	if (compiler->n_frames) {
		for (unsigned int i = 0; i < compiler->n_frames; i++) {
			azo_frame_delete(compiler->frames[i]);
		}
		free(compiler->frames);
	}
}

AZOFrame *
azo_compiler_new_frame(AZOCompiler *comp, AZOFrame *parent, unsigned int capture_this, unsigned int n_args, unsigned int ret_type)
{
	if (comp->n_frames >= comp->n_frames_allocated) {
		comp->n_frames_allocated += 8;
		comp->frames = realloc(comp->frames, comp->n_frames_allocated * sizeof(AZOFrame));
	}
	AZOFrame *frame = azo_frame_new (parent, capture_this, n_args, ret_type, comp->debug);
	frame->parent = parent;
	comp->frames[comp->n_frames++] = frame;
	return frame;
}

static void
compile_PUSH_VALUE_const(AZOCompiler *comp, AZOCompilerContext *ctx, unsigned int type, const AZValue *val, const AZONode *node)
{
	unsigned int pos = azo_frame_append_value (ctx->frame, type, val);
	azo_code_write_PUSH_VALUE(&ctx->frame->code, pos, node);
}

static void
compile_PUSH_VALUE_const_string(AZOCompiler *comp, AZOCompilerContext *ctx, AZString *str, const AZONode *node)
{
	unsigned int pos = azo_frame_append_string (ctx->frame, str);
	azo_code_write_PUSH_VALUE(&ctx->frame->code, pos, node);
}

static void
compile_PUSH_VALUE_const_object(AZOCompiler *comp, AZOCompilerContext *ctx, AZObject *obj, const AZONode *node)
{
	unsigned int pos = azo_frame_append_object (ctx->frame, obj);
	azo_code_write_PUSH_VALUE(&ctx->frame->code, pos, node);
}

/* End temporary */

static unsigned int compile_sentences (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src);
static unsigned int compile_sentence (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src);

/* Expressions */

static unsigned int
azo_compiler_compile_constant (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	if (expr->term.subtype == AZ_TYPE_NONE) {
		/* Constant none is NULL */
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_NONE, NULL, expr);
	} else if (AZ_TYPE_IS_PRIMITIVE (expr->term.subtype)) {
		azo_code_write_PUSH_IMMEDIATE (code, expr->term.subtype, &expr->value.v, expr);
	} else if (expr->term.subtype == AZ_TYPE_STRING) {
		compile_PUSH_VALUE_const_string (comp, ctx, expr->value.v.string, expr);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_OBJECT)) {
		compile_PUSH_VALUE_const_object (comp, ctx, ( AZObject *) expr->value.v.reference, expr);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_REFERENCE)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE_const (comp, ctx, expr->term.subtype, &expr->value.v, expr);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_BLOCK)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE_const (comp, ctx, expr->term.subtype, &expr->value.v, expr);
	} else if (az_type_is_a (expr->term.subtype, AZ_TYPE_FUNCTION_VALUE)) {
		/* fixme: Implement lookup if already pushed */
		compile_PUSH_VALUE_const (comp, ctx, expr->term.subtype, &expr->value.v, expr);
	} else {
		fprintf (stderr, "azo_compiler_compile_constant: Unknown constant type %u\n", expr->term.subtype);
		return 0;
	}
	return 1;
}

static int
compile_this(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	switch (ctx->this_variant) {
		case AZO_COMPILER_NO_THIS:
			fprintf(stderr, "Error: 'this' used in non-method context\n");
			return 0;
		case AZO_COMPILER_THIS_IS_ARGUMENT:
		case AZO_COMPILER_THIS_IS_VARIABLE:
			//fprintf(stderr, "This as varaible at frame:%u\n", ctx->this_var_pos);
			azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE_FRAME, ctx->this_var_pos, node);
			break;
		case AZO_COMPILER_THIS_IS_SHARED:
			//fprintf(stderr, "This is shared at pos:%u\n", ctx->this_static_pos);
			azo_code_write_PUSH_VALUE(&ctx->frame->code, ctx->this_static_pos, node);
			break;
		case AZO_COMPILER_THIS_IS_CAPTURE:
			//fprintf(stderr, "This is captured at pos:%u\n", ctx->this_capture_pos);
			azo_code_write_PUSH_CAPTURE(&ctx->frame->code, ctx->this_capture_pos, node);
			break;
	}
	return 1;
}

/**
 * @brief Assign top of stack to already compiled lvalue
 * 
 * Only LVALUE_STACK, LVALUE_PROPERTY, LVALUE_ATTRIBUTE and LVALUE_ELEMENT allowed
 * 
 */

static unsigned int
compile_assign_to_lvalue (AZOCompiler *comp, AZOCompilerContext *ctx, LValue *lval, const AZONode *expr)
{
	AZOCode *code = &ctx->frame->code;
	if (lval->type == LVALUE_STACK) {
		/* [..., prev, ..., value] */
		azo_code_write_ic_u32 (code, AZO_TC_EXCHANGE_FRAME, lval->pos, expr);
		/* [..., value, ..., prev] */
		azo_code_write_POP (code, 1, expr);
		/* [..., value, ...] */
	} else if (lval->type == LVALUE_PROPERTY) {
		/* [instance, key, value] */
		azo_code_write_SET_PROPERTY (code, expr);
		/* [true] */
		/* [instance, key, value, false] */
		unsigned int finished = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
		/* [instance, key, value] */
		azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_PROPERTY, expr);
		/* [] */
		azo_code_update_JMP32 (code, finished);
	} else if (lval->type == LVALUE_ATTRIBUTE) {
		/* [instance, key, value] */
		azo_code_write_SET_ATTRIBUTE (code, expr);
		/* [] */
	} else if (lval->type == LVALUE_PROPERTY_OR_ATTRIBUTE) {
		/* [instance, key, value] */
		azo_code_write_SET_PROPERTY (code, expr);
		/* [true] */
		/* [instance, key, value, false] */
		unsigned int finished = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
		/* [instance, key, value] */
		azo_code_write_SET_ATTRIBUTE (code, expr);
		/* [] */
		azo_code_update_JMP32 (code, finished);
	} else if (lval->type == LVALUE_ELEMENT) {
		/* [array, index, value] */
		azo_code_write_ic (code, WRITE_ARRAY_ELEMENT, expr);
		/* [array] */
		azo_code_write_POP (code, 1, expr);
		/* [] */
	} else {
		fprintf (stderr, "Unassignable lvalue type\n");
		return 0;
	}
	return 1;
}

/* Compile LValue expression into LValue structure */
/* Only variable, reference, function call and array element expressions are allowed here */

static unsigned int
compile_lvalue (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src, LValue *lvalue, unsigned int read_only)
{
	if (expr->term.type == AZO_TERM_VARIABLE) {
		if (expr->term.subtype == AZO_TERM_VARIABLE_LOCAL) {
			/*
			 * Declared in current instance
			 *
			 * No code, bare LValue
			 */
			lvalue->type = LVALUE_STACK;
			//lvalue->pos = comp->current->n_sig_vars + comp->current->n_parent_vars + expr->var_pos;
			lvalue->pos = expr->var_pos;
			lvalue->n_elements = 0;
			return 1;
		} else if (expr->term.subtype == AZO_TERM_VARIABLE_SHARED) {
			/*
			 * No code, bare LValue
			 */
			// fixme: Track writable shared declarations
			if (!read_only) {
				fprintf (stderr, "Shared variable in writable lvalue\n");
				return 0;
			}
			lvalue->type = LVALUE_SHARED;
			lvalue->pos = expr->var_pos;
			lvalue->n_elements = 0;
			return 1;
		} else if (expr->term.subtype == AZO_TERM_VARIABLE_CAPTURE) {
			/*
			 * Declared in parent frame
			 *
			 * No code, bare LValue
			 */
			// fixme: Track writable static declarations
			if (!read_only) {
				fprintf (stderr, "Parent variable in writable lvalue\n");
				return 0;
			}
			lvalue->type = LVALUE_CAPTURE;
			lvalue->pos = expr->var_pos;
			lvalue->n_elements = 0;
			return 1;
		} else {
			fprintf (stderr, "compile_lvalue: Invalid variable subtype %u\n", expr->term.subtype);
			return 0;
		}
	} else if (expr->term.type == AZO_TERM_REFERENCE) {
		if (expr->term.subtype == AZO_TERM_REFERENCE_PROPERTY) {
			lvalue->type = LVALUE_PROPERTY;
			AZONode *left = expr->children;
			AZONode *right = left->next;
			if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
			compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, expr);
			lvalue->n_elements = 2;
		} else if (expr->term.subtype == AZO_TERM_REFERENCE_ATTRIBUTE) {
			lvalue->type = LVALUE_ATTRIBUTE;
			AZONode *left = expr->children;
			AZONode *right = left->next;
			if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
			compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, expr);
			lvalue->n_elements = 2;
		} else if (expr->term.subtype == AZO_TERM_REFERENCE_PROPERTY_OR_ATTRIBUTE) {
			lvalue->type = LVALUE_PROPERTY_OR_ATTRIBUTE;
			AZONode *left = expr->children;
			AZONode *right = left->next;
			if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
			compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, expr);
			lvalue->n_elements = 2;
		} else {
			fprintf (stderr, "compile_lvalue: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
	} else if (expr->term.type == AZO_TERM_ARRAY_ELEMENT) {
		lvalue->type = LVALUE_ELEMENT;
		AZONode *left = expr->children;
		AZONode *right = left->next;
		if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
		if (!azo_compiler_compile_expression (comp, ctx, right, src)) return 0;
		lvalue->n_elements = 2;
	} else {
		fprintf (stderr, "compile_lvalue: Invalid expression type\n");
		return 0;
	}
	return 1;
}


static unsigned int
compile_call (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *func, const AZONode *list, AZOSource *src, unsigned int has_this, unsigned int test_implementation)
{
	unsigned int is_function;
	unsigned int n_args;
	AZONode *child;
	AZOCode *code = &ctx->frame->code;
	/* [func] */
	/* [func] */
	if (test_implementation) {
		azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 0, AZ_TYPE_FUNCTION, func);
		is_function = azo_code_write_JMP32 (code, JMP_32_IF, 0, func);
		azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, func);
		azo_code_update_JMP32 (code, is_function);
	}
	n_args = 0;
	if (has_this) {
		// has_this indicates that this is one element BEFORE function
		// e.g. [this, function]
		azo_code_write_DUPLICATE (code, 1, func);
		n_args += 1;
	} else {
		azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE_FRAME, 0, func);
		n_args += 1;
		//fprintf(stderr, "%d\n", func->term.subtype);
	}
	/* [func, this] */
	for (child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, ctx, child, src);
		n_args += 1;
	}
	if (n_args > 32) {
		fprintf(stderr, "Too many function arguments: %d\n", n_args);
		return 1;
	}
	/* [func, this, arg1...] */
	azo_code_write_PUSH_FRAME (code, n_args, func);
	/* [func : this, arg1...] */
	azo_code_write_ic_u8 (code, AZO_TC_INVOKE, n_args, func);
	/* [func : this, arg1..., result] */
	azo_code_write_ic (code, AZO_TC_POP_FRAME, func);
	/* [func, this, arg1..., result] */
	azo_code_write_REMOVE (code, 1, n_args + 1, func);
	/* [result] */
	return 0;
}

static void
compile_call_inst_func_args (AZOCompiler *comp, AZOCompilerContext *ctx, unsigned int n_args, const AZONode *expr)
{
	AZOCode *code = &ctx->frame->code;
	/* [inst, funcobj, arg1...] */
	azo_code_write_ic_u32(code, AZO_TC_PUSH_FRAME, n_args, expr);
	/* [inst, funcobj : arg1...] */
	azo_code_write_ic_u8 (code, AZO_TC_INVOKE, n_args, expr);
	/* [inst, funcobj : arg1..., retval] */
	azo_code_write_ic (code, AZO_TC_POP_FRAME, expr);
	/* [inst, funcobj, arg1..., retval] */
	azo_code_write_REMOVE (code, 1, n_args + 2, expr);
	/* [retval] */
}

static unsigned int
compile_call_property (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *func, const AZONode *list, AZOSource *src)
{
	unsigned int is_member_function, is_class, not_active_obj, no_static_function, invalid_type, finished, finished_2, finished_3;
	AZOCode *code = &ctx->frame->code;

	/* Instance, String */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, 1, func);
	/* Instance, String, This */
	unsigned int n_args = 1;
	for (const AZONode *child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, ctx, child, src);
		n_args += 1;
	}
	/* Instance, String, This, Arguments */
	/* Try GET_FUNCTION */
	/* Instance, String, Arguments */
	azo_code_write_GET_FUNCTION(code, n_args, func);
	/* Instance, String, Arguments, Function|null */
	azo_code_compile_last_is_none (code, NULL, &is_member_function, func);

	/* Instance, String, Arguments, null */
	azo_code_write_POP (code, 1, func);
	/* Instance, String, Arguments */
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, n_args + 1, AZ_TYPE_CLASS, &is_class, NULL, func);

	/* Try GET_ATTRIBUTE */
	/* Instance, String, Arguments */
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, n_args + 1, AZ_TYPE_ATTRIBUTE_DICT, NULL, &not_active_obj, func);
	/* ActiveObj, String, Arguments */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, n_args + 1, func);
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, n_args + 1, func);
	azo_code_write_GET_ATTRIBUTE(code, func);
	/* ActiveObj, String, Arguments, Value|null */
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 0, AZ_TYPE_FUNCTION, NULL, &invalid_type, func);
	/* ActiveObj, String, Arguments, Function */

	/* Invoke member function */
	azo_code_update_JMP32 (code, is_member_function);
	/* Instance, String, Arguments, Function */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_EXCHANGE, n_args + 1, func);
	/* Instance, Function, Arguments, String */
	azo_code_write_POP (code, 1, func);
	/* Instance, Function, Arguments */
	compile_call_inst_func_args (comp, ctx, n_args, func);
	/* Retval */
	finished = azo_code_write_JMP32 (code, JMP_32, 0, func);

	/* Try GET_STATIC_FUNCTION */
	azo_code_update_JMP32 (code, is_class);
	n_args -= 1;
	/* Class, String, Class, Arguments */
	azo_code_write_REMOVE (code, n_args, 1, func);
	/* Class, String, Arguments */
	azo_code_write_GET_STATIC_FUNCTION(code, n_args, func);
	/* Class, String, Arguments, Function | null */
	azo_code_compile_last_is_none (code, &no_static_function, NULL, func);

	/* Class, String, Arguments, Function */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_EXCHANGE, n_args + 1, func);
	/* Class, Function, Arguments, String */
	azo_code_write_POP (code, 1, func);
	/* Class, Function, Arguments */
	compile_call_inst_func_args (comp, ctx, n_args, func);
	/* Retval */
	finished_2 = azo_code_write_JMP32 (code, JMP_32, 0, func);

	/* Value */
	azo_code_update_JMP32 (code, no_static_function);
	azo_code_update_JMP32 (code, not_active_obj);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_PROPERTY, func);
	finished_3 = azo_code_write_JMP32 (code, JMP_32, 0, func);
	azo_code_update_JMP32 (code, invalid_type);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, func);
	azo_code_update_JMP32 (code, finished);
	azo_code_update_JMP32 (code, finished_2);
	azo_code_update_JMP32 (code, finished_3);
	return 1;
}

/*
 * compile_call_attribute - call a function stored as an attribute
 *
 * Stack after completion: [result]
 *
 */
static unsigned int
compile_call_attribute (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *func, const AZONode *list, AZOSource *src)
{
	unsigned int not_active_obj, invalid_type, finished;
	AZOCode *code = &ctx->frame->code;

	/* Instance, Key */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, 1, func);
	/* Instance, Key, Instance */
	unsigned int n_args = 1;
	for (const AZONode *child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, ctx, child, src);
		n_args += 1;
	}
	/* Instance, Key, Arguments */
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, n_args + 1, AZ_TYPE_ATTRIBUTE_DICT, NULL, &not_active_obj, func);
	/* AttribDict, String, Arguments */
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, n_args + 1, func);
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE, n_args + 1, func);
	/* AttribDict, String, Arguments, AttribDict, String */
	azo_code_write_GET_ATTRIBUTE(code, func);
	/* AttribDict, String, Arguments, Value|null */
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 0, AZ_TYPE_FUNCTION, NULL, &invalid_type, func);
	/* AttribDict, String, Arguments, Function */

	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_EXCHANGE, n_args + 1, func);
	/* Instance, Function, Arguments, String */
	azo_code_write_POP (code, 1, func);
	/* Instance, Function, Arguments */
	compile_call_inst_func_args (comp, ctx, n_args, func);
	/* Retval */
	finished = azo_code_write_JMP32 (code, JMP_32, 0, func);
	azo_code_update_JMP32 (code, not_active_obj);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_PROPERTY, func);
	azo_code_update_JMP32 (code, invalid_type);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, func);
	azo_code_update_JMP32 (code, finished);
	return 1;
}

#define noDEBUG_NEW

static unsigned int
compile_new (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *klass, const AZONode *list, AZOSource *src)
{
	AZONode *child;
	unsigned int invalid_type_2, finished;
	AZOCode *code = &ctx->frame->code;

	static AZString *newstr = NULL;
	unsigned int n_args = 0;
	if (!newstr) newstr = az_string_new ((const unsigned char *) "new");

	assert(klass->term.type == AZO_TERM_TYPE);

	AZClass *tklass = AZ_CLASS_FROM_TYPE(klass->term.subtype);
	compile_PUSH_VALUE_const(comp, ctx, AZ_TYPE_CLASS, (const AZValue *) &tklass, klass);

	/* [Class] */
	compile_PUSH_VALUE_const_string (comp, ctx, newstr, klass);
	/* [Class, "new"] */
	for (child = list->children; child; child = child->next) {
		azo_compiler_compile_expression (comp, ctx, child, src);
		n_args += 1;
	}
	if (n_args > 32) {
		fprintf(stderr, "Too many function arguments: %d\n", n_args);
		return 0;
	}
	/* [Class, "new", arg1...] */
	azo_code_write_GET_STATIC_FUNCTION(code, n_args, list);
	/* [Class, "new", arg1..., funcobj | null] */
	azo_code_compile_last_is_none (code, &invalid_type_2, NULL, klass);
	/* [Class, "new", arg1..., funcobj] */
	azo_code_write_EXCHANGE (code, n_args + 1, list);
	/* [Class, funcobj, arg1..., "new"] */
	azo_code_write_POP (code, 1, list);
	/* [Class, funcobj, arg1...] */
	compile_call_inst_func_args (comp, ctx, n_args, klass);
	finished = azo_code_write_JMP32 (code, JMP_32, 0, NULL);
	/* Invalid type */
	azo_code_update_JMP32 (code, invalid_type_2);
	azo_code_write_EXCEPTION (code, AZO_EXCEPTION_INVALID_TYPE, NULL);
	/* Finished */
	azo_code_update_JMP32 (code, finished);

	return 1;
}

static unsigned int
compile_prefix_arithmetic (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, const AZONode *left, AZOSource *src, unsigned int silent)
{
	AZOCode *code = &ctx->frame->code;
	LValue lval;
	if (silent) {
		if (!compile_lvalue (comp, ctx, left, src, &lval, 0)) return 0;
		/* LValue */
		if (expr->term.subtype == AZO_TERM_PREFIX_INCREMENT) {
			if (!azo_compiler_compile_increment (comp, ctx, left, expr, src)) return 0;
		} else if (expr->term.subtype == AZO_TERM_PREFIX_DECREMENT) {
			if (!azo_compiler_compile_decrement (comp, ctx, left, expr, src)) return 0;
		} else {
			fprintf (stderr, "compile_prefix_arithmetic: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
		/* LValue, Value */
		compile_assign_to_lvalue (comp, ctx, &lval, NULL);
	} else {
		azo_code_write_PUSH_EMPTY (code, AZ_TYPE_NONE, expr);
		/* null */
		if (!compile_lvalue (comp, ctx, left, src, &lval, 0)) return 0;
		/* null, [LValue] */
		if (expr->term.subtype == AZO_TERM_PREFIX_INCREMENT) {
			if (!azo_compiler_compile_increment (comp, ctx, left, expr, src)) return 0;
		} else if (expr->term.subtype == AZO_TERM_PREFIX_DECREMENT) {
			if (!azo_compiler_compile_decrement (comp, ctx, left, expr, src)) return 0;
		} else {
			fprintf (stderr, "compile_prefix_arithmetic: Invalid expression subtype %u\n", expr->term.subtype);
			return 0;
		}
		/* null, [LValue], Value */
		azo_code_write_EXCHANGE (code, lval.n_elements + 1, expr);
		/* Value, [LValue], null */
		azo_code_write_DUPLICATE (code, lval.n_elements + 1, expr);
		/* Value, [LValue], Value */
		compile_assign_to_lvalue (comp, ctx, &lval, NULL);
		/* Value */
	}

	return 1;
}

static unsigned int
compile_prefix (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, const AZONode *left, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	if (expr->term.subtype == AZO_TERM_PREFIX_PLUS) {
		if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
		/* NOP */
	} else if (expr->term.subtype == AZO_TERM_PREFIX_MINUS) {
		if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
		azo_code_write_ic (code, AZO_TC_NEGATE, expr);
	} else if (expr->term.subtype == AZO_TERM_PREFIX_NOT) {
		if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
		azo_code_write_ic (code, AZO_TC_LOGICAL_NOT, expr);
	} else if (expr->term.subtype == AZO_TERM_PREFIX_TILDE) {
		if (!azo_compiler_compile_tilde (comp, ctx, left, src)) return 0;
	} else if (expr->term.subtype == AZO_TERM_PREFIX_INCREMENT) {
		if (!compile_prefix_arithmetic (comp, ctx, expr, left, src, 0)) return 0;
	} else if (expr->term.subtype == AZO_TERM_PREFIX_DECREMENT) {
		if (!compile_prefix_arithmetic (comp, ctx, expr, left, src, 0)) return 0;
	} else {
		fprintf (stderr, "compile_prefix: Unimplemented or unknown prefix type %u\n", left->term.subtype);
		return 0;
	}
	return 1;
}

static unsigned int
compile_suffix (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, const AZONode *left, AZOSource *src, unsigned int silent)
{
	LValue lval;
	if (!silent) {
		/* Push original value */
		/* fixme: Do it more intelligently */
		if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
	}
	/* Set variable target */
	if (!compile_lvalue (comp, ctx, left, src, &lval, 0)) return 0;
	/* Calculate new value */
	if (expr->term.subtype == AZO_TERM_SUFFIX_INCREMENT) {
		if (!azo_compiler_compile_increment (comp, ctx, left, expr, src)) return 0;
	} else if (expr->term.subtype == AZO_TERM_SUFFIX_DECREMENT) {
		if (!azo_compiler_compile_decrement (comp, ctx, left, expr, src)) return 0;
	} else {
		fprintf (stderr, "compile_suffix: Invalid expression subtype %u\n", expr->term.subtype);
		return 0;
	}
	compile_assign_to_lvalue (comp, ctx, &lval, NULL);
	return 1;
}

static unsigned int
compile_function_call_member (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *base = node->children;
	AZONode *name = base->next;
	AZONode *list = name->next;
	if (!azo_compiler_compile_expression (comp, ctx, base, src)) return 0;
	compile_PUSH_VALUE_const_string (comp, ctx, name->value.v.string, name);
	/* instance, key */
	switch (node->term.subtype) {
		case AZO_TERM_FUNCTION_CALL_PROPERTY:
			compile_call_property (comp, ctx, node, list, src);
			break;
		case AZO_TERM_FUNCTION_CALL_ATTRIBUTE:
			compile_call_attribute (comp, ctx, node, list, src);
			break;
		case AZO_TERM_FUNCTION_CALL_PROPERTY_OR_ATTRIBUTE:
			compile_call_property (comp, ctx, node, list, src);
			break;
		default:
			fprintf (stderr, "compile_function_call_member: Unknown function call type %u\n", node->term.subtype);
			return 0;
	}
	return 1;
}

static unsigned int
compile_function_call (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src, unsigned int silent)
{
	AZOCode *code = &ctx->frame->code;
	if (node->term.subtype == AZO_TERM_FUNCTION_CALL_PLAIN) {
		AZONode *func = node->children;
		AZONode *list = func->next;
		if (!azo_compiler_compile_expression(comp, ctx, func, src)) return 0;
		unsigned int result = compile_call (comp, ctx, func, list, src, 0, 1);
		if (result) return 0;
	} else {
		if (!compile_function_call_member (comp, ctx, node, src)) return 0;
	}
	if (silent) {
		azo_code_write_POP (code, 1, node);
	}
	return 1;
}

#define noDEBUG_FUNCTION

static unsigned int
compile_function (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *type, *args, *body;
	AZONode *child;
	AZOProgram *prog;
	AZOCompiledFunction *cfunc;
	AZOCode *code = &ctx->frame->code;
	type = node->children;
	args = type->next;
	body = args->next;

	/* Return type */
	assert (type->term.type == AZO_TERM_TYPE);
	unsigned int ret_type = type->term.subtype;

	/* Compile function body in it's own resolved frame */
	assert(node->frame);

	AZOFrame *func_frame = node->frame;

	AZOCompilerContext func_ctx = *ctx;
	func_ctx.ret_type = ret_type;
	func_ctx.frame = func_frame;
	
	prog = azo_compiler_compile(comp, &func_ctx, body, src);
	/* Restore the previous frame */

	if (!prog) {
		fprintf (stderr, "compile_function: error compiling function\n");
		return 0;
	}

	compile_PUSH_VALUE_const(comp, ctx, AZO_TYPE_PROGRAM, (const AZValue *) &prog, node);
	/* program */
	if (func_frame->this_is_captured) {
		// fixme: This should be fetched from context
		fprintf(stderr, "This is captured\n");
		compile_this(comp, ctx, node, src);
		/* program [this] */
	}
	for (AZOVariableList *list = func_frame->parent_vars; list; list = list->next) {
		/* list->var.parent is variable in *current* frame */
		AZOVariable *var = list->var.parent;
		fprintf(stderr, "%s is captured\n", var->name->str);
		if (var->parent) {
			azo_code_write_PUSH_CAPTURE(code, var->pos, node);
		} else {
			azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE_FRAME, var->pos, node);
		}
	}
	/* program [this] val1 ... */
	azo_code_write_ic_u32(code, AZO_TC_CLOSURE, prog->n_captures, node);
	/* closure */

	azo_program_unref(prog);

	return 1;
}

static unsigned int
compile_array_literal (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	const AZONode *child;
	unsigned int size = 0, idx = 0;
	AZOCode *code = &ctx->frame->code;

	for (child = node->children; child; child = child->next) size += 1;
	/* Create new array */
	azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_UINT32, (const AZValue *) &size, node);
	azo_code_write_ic (code, NEW_ARRAY, node);
	for (child = node->children; child; child = child->next) {
		azo_code_write_PUSH_IMMEDIATE (code, AZ_TYPE_UINT32, (const AZValue *) &idx, node);
		azo_compiler_compile_expression (comp, ctx, child, src);
		azo_code_write_ic (code, WRITE_ARRAY_ELEMENT, node);
		idx += 1;
	}
	return 1;
}

static unsigned int
compile_test (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	AZONode *lhs = node->children;
	AZONode *rhs = lhs->next;
	/* fixme: Implement expression type? */
	if (!azo_compiler_compile_expression (comp, ctx, lhs, src)) return 0;
	if (!azo_compiler_compile_expression (comp, ctx, rhs, src)) return 0;
	azo_code_write_ic_u8(code, AZO_TC_TYPE_OF_CLASS, 0, node);
	if (node->term.subtype == AZO_TERM_TEST_IS) {
		azo_code_write_TEST_TYPE (code, AZO_TC_TYPE_IS, 2, node);
	} else {
		azo_code_write_TEST_TYPE (code, AZO_TC_TYPE_IMPLEMENTS, 2, node);
	}
	azo_code_write_REMOVE (code, 1, 2, node);
	return 1;
}

static unsigned int
compile_cast (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *type = node->children;
	AZONode *val = type->next;

	if (node->term.subtype != AZO_TERM_CAST_CONVERT) {
		/* fixme: implement checked class/interface conversion (as) */
		fprintf (stderr, "compile_cast: only primitive conversion casts are implemented\n");
		return 0;
	}
	if (!azo_compiler_compile_expression (comp, ctx, val, src)) return 0;
	azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_CONVERT_TYPE, type->term.subtype, node);
	return 1;
}

/* Compile rvalue expression (this, null, literal...) */

#define noDEBUG_PARENT_VAR

static unsigned int
compile_expression_rvalue (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	if (node->term.type == AZO_TERM_VARIABLE) {
		if (node->term.subtype == AZO_TERM_VARIABLE_LOCAL) {
			azo_code_write_ic_u32(&ctx->frame->code, AZO_TC_DUPLICATE_FRAME, node->var_pos, node);
		} else if (node->term.subtype == AZO_TERM_VARIABLE_SHARED) {
			azo_code_write_PUSH_VALUE(code, node->var_pos, node);
		} else if (node->term.subtype == AZO_TERM_VARIABLE_CAPTURE) {
			azo_code_write_PUSH_CAPTURE(code, node->var_pos, node);
		} else {
			fprintf (stderr, "compile_expression_rvalue: Invalid variable subtype %u\n", node->term.subtype);
			return 0;
		}
	} else if (node->term.type == AZO_TERM_CONSTANT) {
		if (!azo_compiler_compile_constant (comp, ctx, node, src)) return 0;
	} else if (node->term.type == AZO_TERM_KEYWORD) {
		fprintf (stderr, "compile_expression_rvalue: Unknown keyword subtype %u\n", node->term.subtype);
		return 0;
	} else if (node->term.type == AZO_TERM_FUNCTION) {
		if (!compile_function (comp, ctx, node, src)) return 0;
	} else if (node->term.type == AZO_TERM_FUNCTION_CALL) {
		if (!compile_function_call (comp, ctx, node, src, 0)) return 0;
	} else if (node->term.type == AZO_TERM_LITERAL_ARRAY) {
		if (!compile_array_literal (comp, ctx, node, src)) return 0;
		/* fixme: Do we allow operators here? (Lauris) */
	} else if (node->term.type == AZO_TERM_PREFIX) {
		if (!compile_prefix (comp, ctx, node, node->children, src)) return 0;
	} else if (node->term.type == AZO_TERM_SUFFIX) {
		if (!compile_suffix (comp, ctx, node, node->children, src, 0)) return 0;
	} else if (node->term.type == AZO_TERM_COMPARISON) {
		if (!azo_compiler_compile_comparison (comp, ctx, node->children, node->children->next, node, src, 11)) return 0;
	} else if (node->term.type == AZO_TERM_BINARY) {
		if (!azo_compiler_compile_arithmetic (comp, ctx, node->children, node->children->next, node, src)) return 0;
	} else if (node->term.type == AZO_TERM_TEST) {
		if (!compile_test (comp, ctx, node, src)) return 0;
	} else if (node->term.type == AZO_TERM_CAST) {
		if (!compile_cast (comp, ctx, node, src)) return 0;
	} else {
		fprintf (stderr, "compile_expression_rvalue: Invalid expression type %u\n", node->term.type);
		return 0;
	}
	return 1;
}

static unsigned int
compile_array_reference (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *array_ref, const AZONode *idx, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	azo_compiler_compile_expression (comp, ctx, array_ref, src);
	/* Array */
	azo_compiler_compile_expression (comp, ctx, idx, src);
	/* Array, Index */
	azo_code_write_ic (code, LOAD_ARRAY_ELEMENT, array_ref);
	/* Array, Value */
	azo_code_write_REMOVE (code, 1, 1, NULL);
	return 1;
}

static unsigned int
compile_reference_lookup (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZString *str)
{
	unsigned int property_not_null, not_active_obj, finished;
	AZOCode *code = &ctx->frame->code;

	/* InstanceA */
	azo_code_write_DUPLICATE (code, 0, expr);
	compile_PUSH_VALUE_const_string (comp, ctx, str, expr);
	/* InstanceA, InstanceA, String */
	azo_code_write_ic (code, AZO_TC_GET_PROPERTY, expr);
	/* InstanceA, Value|null */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, expr);
	property_not_null = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);

	/* InstanceA, null */
	azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 1, AZ_TYPE_ATTRIBUTE_DICT, expr);
	not_active_obj = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
	/* ActiveObj, null */
	azo_code_write_POP (code, 1, expr);
	compile_PUSH_VALUE_const_string (comp, ctx, str, expr);
	/* ActiveObj, String */
	azo_code_write_GET_ATTRIBUTE (code, expr);
	/* Value */
	finished = azo_code_write_JMP32 (code, JMP_32, 0, NULL);

	azo_code_update_JMP32 (code, property_not_null);
	azo_code_update_JMP32 (code, not_active_obj);
	/* InstanceA, Value */
	azo_code_write_REMOVE (code, 1, 1, NULL);
	/* Value */
	azo_code_update_JMP32 (code, finished);
	return 1;
}

static unsigned int
compile_variable_reference (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, unsigned int type, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	AZONode *left = node->children;
	AZONode *right = left->next;
	if (!azo_compiler_compile_expression (comp, ctx, left, src)) return 0;
	if (type == AZO_TERM_REFERENCE_PROPERTY) {
		compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, node);
		azo_code_write_ic (code, AZO_TC_GET_PROPERTY, node);
	} else if (type == AZO_TERM_REFERENCE_ATTRIBUTE) {
		compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, node);
		azo_code_write_GET_ATTRIBUTE (code, node);
	} else if (type == AZO_TERM_REFERENCE_PROPERTY_OR_ATTRIBUTE) {
		/* inst */
		azo_code_write_DUPLICATE (code, 0, node);
		compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, node);
		/* inst, inst, name */
		azo_code_write_ic (code, AZO_TC_GET_PROPERTY, node);
		/* inst, value|null */
		azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, node);
		unsigned int property_not_null = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, node);

		/* inst, null */
		azo_code_write_TEST_TYPE_IMMEDIATE (code, AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, 1, AZ_TYPE_ATTRIBUTE_DICT, node);
		unsigned int not_active_obj = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, node);
		/* inst, null */
		azo_code_write_POP (code, 1, node);
		compile_PUSH_VALUE_const_string (comp, ctx, right->value.v.string, node);
		/* inst, name */
		azo_code_write_GET_ATTRIBUTE (code, node);
		/* Value */
		unsigned int finished = azo_code_write_JMP32 (code, JMP_32, 0, NULL);

		azo_code_update_JMP32 (code, property_not_null);
		azo_code_update_JMP32 (code, not_active_obj);
		/* inst, value|null */
		azo_code_write_REMOVE (code, 1, 1, NULL);
		/* value */
		azo_code_update_JMP32 (code, finished);
	} else {
		fprintf (stderr, "azo_compiler_compile_expression: Unknown reference subtype %u\n", node->term.subtype);
		return 0;
	}
	return 1;
}

/* Compile lvalue expression (i.e. reference) */

static unsigned int
compile_expression_lvalue (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	if (node->term.type == AZO_TERM_REFERENCE) {
		/* LValue types */
		if (!compile_variable_reference (comp, ctx, node, node->term.subtype, src)) return 0;
	} else if (node->term.type == AZO_TERM_ARRAY_ELEMENT) {
		if (!compile_array_reference (comp, ctx, node->children, node->children->next, src)) return 0;
	} else {
		fprintf (stderr, "compile_expression_lvalue: Invalid expression type %u\n", node->term.type);
		return 0;
	}
	return 1;
}

unsigned int
azo_compiler_compile_expression (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	if (node->term.type == AZO_TERM_REFERENCE) {
		/* LValue types */
		if (!compile_expression_lvalue (comp, ctx, node, src)) return 0;
	} else if (node->term.type == AZO_TERM_ARRAY_ELEMENT) {
		if (!compile_expression_lvalue (comp, ctx, node, src)) return 0;
	} else {
		/* RValue types */
		if (!compile_expression_rvalue (comp, ctx, node, src)) return 0;
	}
	return 1;
}

static unsigned int
compile_assign (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *left, const AZONode *right, AZOSource *src)
{
	LValue lval;
	/* Push necessary components (instance+key or array+index) */
	if (!compile_lvalue (comp, ctx, left, src, &lval, 0)) return 0;
	/* Push value */
	if (!azo_compiler_compile_expression (comp, ctx, right, src)) return 0;
	/* Actual assignment */
	if (!compile_assign_to_lvalue (comp, ctx, &lval, left)) return 0;
	return 1;
}

static unsigned int
compile_silent_statement (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	switch (node->term.type) {
	case AZO_TERM_EMPTY:
		break;
	case AZO_TERM_STATEMENT_GROUP:
		// fixme: Should we be more pedantic here?
		if (!compile_sentences (comp, ctx, node->children, src)) return 0;
		break;
	case AZO_TERM_ASSIGN:
		if (!compile_assign (comp, ctx, node->children, node->children->next, src)) return 0;
		break;
	case AZO_TERM_FUNCTION_CALL:
		if (!compile_function_call (comp, ctx, node, src, 1)) return 0;
		break;
	case AZO_TERM_SUFFIX:
		if (!compile_suffix (comp, ctx, node, node->children, src, 1)) return 0;
		break;
	case AZO_TERM_PREFIX:
		if (!compile_prefix_arithmetic (comp, ctx, node, node->children, src, 1)) return 0;
		break;
	default:
		fprintf (stderr, "compile_silent_statement: invalid expression type %u\n", node->term.type);
		return 0;
	}
	return 1;
}

static unsigned int
compile_single_declaration (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src, unsigned int type)
{
	AZOCode *code = &ctx->frame->code;
	AZONode *name, *value;
	name = node->children;
	value = name->next;
	if (value) {
		azo_compiler_compile_expression (comp, ctx, value, src);
	} else {
		/* fixme: Implement runtime (or at least compile-time) type */
		azo_code_write_PUSH_EMPTY (code, type, node);
		//azo_compiler_write_PUSH_EMPTY (comp, type);
	}
	ctx->n_stack += 1;
	return 1;
}

static unsigned int
compile_declaration (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *type, *child;
	type = node->children;

	if (type->term.type != AZO_TERM_TYPE) {
		fprintf (stderr, "compile_declaration: Type is not resolved (%u/%u)\n", type->term.type, type->term.subtype);
		return 0;
	}

	for (child = type->next; child; child = child->next) {
		if (!compile_single_declaration (comp, ctx, child, src, type->term.subtype)) return 0;
	}
	return 1;
}

static unsigned int
compile_step_statement (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	if (node->term.type == AZO_TERM_DECLARATION_LIST) {
		if (!compile_declaration (comp, ctx, node, src)) return 0;
	} else {
		if (!compile_silent_statement (comp, ctx, node, src)) return 0;
	}
	return 1;
}

#define noDEBUG_STATEMENT

static unsigned int
compile_statement (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_RETURN)) {
		if (node->children) {
			// fixme: Why it was rvalue here?
			// if (!compile_expression_rvalue (comp, expr->children, src)) return 0;
			if (!azo_compiler_compile_expression(comp, ctx, node->children, src)) return 0;
			azo_code_write_ic (code, AZO_TC_RETURN_VALUE, node);
		} else {
			azo_code_write_ic (code, AZO_TC_RETURN, node);
		}
		return 1;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_DEBUG)) {
		comp->debug = 1;
		return 1;
	} else {
		if (!compile_step_statement (comp, ctx, node, src)) return 0;
	}
	return 1;
}

static unsigned int
compile_block (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZOCode *code = &ctx->frame->code;
	AZONode *child = node->children;
	unsigned int n_stack = ctx->n_stack;
	unsigned int result = compile_sentences(comp, ctx, child, src);
	/* Clear scope */
	if (ctx->n_stack > n_stack) {
		azo_code_write_POP (code, ctx->n_stack - n_stack, node);
		ctx->n_stack = n_stack;
	}
	return result;
}

#define noDEBUG_FOR

static unsigned int
compile_cycle (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node,
	const AZONode *init, const AZONode *test_at_begin, const AZONode *test_at_end, const AZONode *step, const AZONode *content,
	AZOSource *src)
{
	unsigned int cycle_begin, cycle_end;
	AZOCode *code = &ctx->frame->code;

	unsigned int n_stack = ctx->n_stack;
	/* Initialization */
	if (init) compile_step_statement (comp, ctx, init, src);
	/* Cycle start */
	cycle_begin = azo_frame_get_current_ip (ctx->frame);
	/* Test condition */
	if (test_at_begin) {
		azo_code_compile_expression_and_type_check(comp, ctx, test_at_begin, 0, AZ_TYPE_BOOLEAN, src);
		/* Jump out of cycle if condition was FALSE */
		cycle_end = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, NULL);
	}
	/* Cycle content */
	compile_sentence (comp, ctx, content, src);
	/* Step */
	if (step) compile_silent_statement(comp, ctx, step, src);
	if (test_at_end) {
		azo_code_compile_expression_and_type_check(comp, ctx, test_at_end, 0, AZ_TYPE_BOOLEAN, src);
		/* Jump back if condition is true */
		azo_code_write_JMP32 (code, JMP_32_IF, cycle_begin, NULL);
	} else {
		/* Unconditionally jump back */
		azo_code_write_JMP32 (code, JMP_32, cycle_begin, NULL);
	}
	if (test_at_begin) {
		azo_code_update_JMP32 (code, cycle_end);
	}
	if (ctx->n_stack > n_stack) {
		azo_code_write_POP (code, ctx->n_stack - n_stack, node);
		ctx->n_stack = n_stack;
	}
	return 1;
}

/*
 * FOR
 *   + init
 *   + condition
 *   + step
 *   + content
 */

static unsigned int
compile_for(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	AZONode *init, *test, *step, *content;
	init = expr->children;
	test = init->next;
	step = test->next;
	content = step->next;
	return compile_cycle(comp, ctx, expr, init, test, NULL, step, content, src);
}

/*
 * WHILE
 *   + condition
 *   + content
 */

static unsigned int
compile_while(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	AZONode *test, *content;
	test = expr->children;
	content = test->next;
	return compile_cycle(comp, ctx, expr, NULL, test, NULL, NULL, content, src);
}

/*
 * DO
 *  + condition
 *  + content
 */

static unsigned int
compile_do(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	AZONode *block = expr->children;
	AZONode *cond = block->next;
	return compile_cycle(comp, ctx, expr, NULL, NULL, cond, NULL, block, src);
}

/*
 * IF
 *   + condition
 *   + iftrue
 *   + iffalse
 */

static unsigned int
compile_if (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *cond = node->children;
	AZONode *iftrue = cond->next;
	AZONode *iffalse = iftrue->next;
	AZOCode *code = &ctx->frame->code;

	azo_code_compile_expression_and_type_check(comp, ctx, cond, 0, AZ_TYPE_BOOLEAN, src);
	/* Jump conditionally to NOT TRUE statement */
	unsigned int not_true = azo_code_write_JMP32(code, JMP_32_IF_NOT, 0, cond);
	/* TRUE sentence */
	compile_sentence (comp, ctx, iftrue, src);
	if (iffalse) {
		/* Jump to end if TRUE */
		unsigned int else_loc = azo_code_write_JMP32(code, JMP_32, 0, iftrue);
		/* Land here if FALSE */
		azo_code_update_JMP32 (code, not_true);
		compile_sentence (comp, ctx, iffalse, src);
		azo_code_update_JMP32 (code, else_loc);
	} else {
		/* Simply land here if FALSE */
		azo_code_update_JMP32 (code, not_true);
	}
	return 1;
}

static int
compile_context(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	AZONode *this_node = node->children;
	AZOCompilerContext lctx = *ctx;
	if (this_node->term.type == AZO_TERM_EMPTY) {
		lctx.this_variant = AZO_COMPILER_NO_THIS;
	} else if (this_node->term.type == AZO_TERM_VARIABLE) {
		/* Known variable types, refer directly to these */
		switch (this_node->term.subtype) {
			case AZO_TERM_VARIABLE_LOCAL:
				lctx.this_variant = AZO_COMPILER_THIS_IS_VARIABLE;
				lctx.this_var_pos = this_node->var_pos;
				break;
			case AZO_TERM_VARIABLE_SHARED:
				lctx.this_variant = AZO_COMPILER_THIS_IS_SHARED;
				lctx.this_static_pos = this_node->var_pos;
				break;
			case AZO_TERM_VARIABLE_CAPTURE:
				lctx.this_variant = AZO_COMPILER_THIS_IS_CAPTURE;
				lctx.this_capture_pos = this_node->var_pos;
				break;
			default:
				fprintf(stderr, "resolve_context: Unknown variable type\n");
				return 0;
		}
	} else {
		/* Local 'this' was not resolved, it has reserved local variable spot */
		lctx.this_variant = AZO_COMPILER_THIS_IS_VARIABLE;
		lctx.this_var_pos = node->var_pos;
		if (!azo_compiler_compile_expression(comp, ctx, node->children, src)) return 0;
		if (!compile_sentences(comp, &lctx, node->children->next, src)) return 0;
		azo_code_write_POP(&lctx.frame->code, 0, node);
		return 1;
	}
	if (!compile_sentences(comp, &lctx, node->children->next, src)) return 0;
	return 1;
}

/*
 * Sentence:
 *   Block
 *   Line
 *   for
 *   while
 *   if
 */

static unsigned int
compile_sentence (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	if (AZO_NODE_IS(node, AZO_TERM_CONTEXT, 0)) {
		if (!compile_context(comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_BLOCK, 0)) {
		if (!compile_block (comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_FOR)) {
		if (!compile_for (comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_WHILE)) {
		if (!compile_while (comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_DO)) {
		if (!compile_do (comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_IF)) {
		if (!compile_if (comp, ctx, node, src)) return 0;
	} else if (AZO_NODE_IS(node, AZO_TERM_KEYWORD, AZO_KEYWORD_DEBUG)) {
		if (node->value.impl) {
			assert(node->value.impl == AZ_IMPL_FROM_TYPE(AZ_TYPE_STRING));
			azo_compiler_write_DEBUG_STRING(comp, ctx, (const char *) node->value.v.string->str, node);
		}
	} else {
		/* Line is the same as statement because semicolon is processed by parser */
		if (!compile_statement (comp, ctx, node, src)) return 0;
	}
	return 1;
}

/* 
 * Sentences:
 *   [Sentence...]
 */

  static unsigned int
compile_sentences (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node, AZOSource *src)
{
	while (node) {
		if (!compile_sentence(comp, ctx, node, src)) return 0;
		node = node->next;
	}
	return 1;
}

/*
 * Program:
 *   Sentences
 */

static unsigned int
compile_program (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src)
{
	compile_sentences(comp, ctx, expr->children, src);
	return 1;
}

AZOProgram *
azo_compiler_compile (AZOCompiler *comp, AZOCompilerContext *ctx, AZONode *root, AZOSource *src)
{
	AZOProgram *prog;

	if (root->term.type == AZO_TERM_PROGRAM) {
		/* Programs are lists of sentences */
		if (!compile_program (comp, ctx, root, src)) return NULL;
	} else if (root->term.type == AZO_TERM_BLOCK) {
		/* Function bodies are blocks */
		if (!compile_sentence (comp, ctx, root, src)) return NULL;
	} else if (root->term.type == AZO_TERM_CONTEXT) {
		/* Function bodies are blocks */
		if (!compile_sentence (comp, ctx, root, src)) return NULL;
	} else {
		fprintf (stderr, "azo_compiler_compile: Invalid expression type %u\n", root->term.type);
		return NULL;
	}
	prog = azo_program_new(comp->globals, ctx->frame, root, src);

	return prog;
}
