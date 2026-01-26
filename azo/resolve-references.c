#define __AZO_RESOLVE_REFERENCES_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <az/field.h>
#include <az/function.h>
#include <az/string.h>

#include <az/classes/attrib-dict.h>
#include <azo/compiler.h>
#include <azo/expression.h>
#include <azo/keyword.h>
#include <azo/optimizer.h>

static unsigned int
resolve_member_class (AZOCompiler *comp, AZOExpression *member, const AZClass *klass, unsigned int flags)
{
	return 0;
}

static unsigned int
resolve_member_impl (AZOCompiler *comp, AZOExpression *member, const AZClass *klass, const AZImplementation *impl, unsigned int flags)
{
	return 0;
}

#define noDEBUG_MEMBER_INST

static unsigned int
resolve_member_inst (AZOCompiler *comp, AZOExpression *expr, const AZClass *klass, const AZImplementation *impl, void *inst, AZString *str, unsigned int flags)
{
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	int idx = az_class_lookup_property (klass, impl, inst, str, &def_class, &def_impl, &def_inst);
	if (idx >= 0) {
		AZField *field = &def_class->props_self[idx];
		if (!inst && (field->spec == AZ_FIELD_INSTANCE)) return 0;
		if (!impl && (field->spec == AZ_FIELD_IMPLEMENTATION)) return 0;
		if (field->is_final && !field->is_function) {
			const AZImplementation *prop_impl;
			AZValue64 prop_val;
			if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &prop_impl, &prop_val.value, 64, NULL)) {
				fprintf (stderr, "resolve_member: Property %s is not readable\n", str->str);
				return 1;
			}
			az_packed_value_set_from_impl_value (&expr->value, prop_impl, &prop_val.value);
			expr->term.type = EXPRESSION_CONSTANT;
			if (prop_impl) {
				expr->term.subtype = AZ_IMPL_TYPE(prop_impl);
				az_value_clear (prop_impl, &prop_val.value);
			} else {
				// Final undefined value, not normal but we have to handle it
				expr->term.subtype = 0;
			}
			while (expr->children) {
				AZOExpression *child = expr->children;
				expr->children = child->next;
				azo_expression_free_tree (child);
			}
#ifdef DEBUG_MEMBER_INST
			fprintf (stderr, "resolve_member: Replaced final property %s with constant\n", str->str);
#endif
			return 0;
		}
	} else if (inst && az_type_implements(AZ_IMPL_TYPE(impl), AZ_TYPE_ATTRIBUTE_DICT)) {
		void *attrd_inst;
		const AZAttribDictImplementation *attrd_impl = (AZAttribDictImplementation *) az_instance_get_interface (impl, inst, AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
		AZValue64 attr_val;
		unsigned int attr_flags;
		const AZImplementation *attr_impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, str, &attr_val, &attr_flags);
		if (attr_flags & AZ_ATTRIB_ARRAY_IS_FINAL) {
			az_packed_value_set_from_impl_value (&expr->value, attr_impl, &attr_val.value);
			expr->term.type = EXPRESSION_CONSTANT;
			if (attr_impl) {
				expr->term.subtype = AZ_IMPL_TYPE(attr_impl);
				az_value_clear (attr_impl, &attr_val.value);
			} else {
				// Final undefined value, not normal but we have to handle it
				expr->term.subtype = 0;
			}
			while (expr->children) {
				AZOExpression *child = expr->children;
				expr->children = child->next;
				azo_expression_free_tree (child);
			}
#ifdef DEBUG_MEMBER_INST
			fprintf (stderr, "resolve_member: Replaced final attribute %s with constant\n", str->str);
#endif
			return 0;
		}
	}
	return 0;
}

/*
 * MEMBER_REFERENCE
 *   REFERENCE_VARIABLE | REFERENCE_MEMBER | CONSTANT
 *   REFERENCE_PROPERTY
 */

static unsigned int
resolve_member (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	AZOExpression *parent, *member;
	unsigned int result;
	parent = azo_compiler_resolve_expression (comp, expr->children, flags, &result);
	if (result) return result;
	member = azo_compiler_resolve_expression (comp, expr->children->next, flags, &result);
	if (result) return result;
	if (parent->term.type == EXPRESSION_CONSTANT) {
		if ((member->term.type == EXPRESSION_REFERENCE) && (member->term.subtype == REFERENCE_PROPERTY)) {
			const AZClass *klass = az_type_get_class (parent->term.subtype);
			const AZImplementation *impl = parent->value.impl;
			void *inst = az_value_get_inst(parent->value.impl, &parent->value.v);
			return resolve_member_inst (comp, expr, klass, impl, inst, member->value.v.string, flags);
		}
	}
	return 0;
}

/*
 * REFERENCE_VARIABLE
 */

static unsigned int
resolve_this_reference (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	const AZClass *klass = AZ_CLASS_FROM_IMPL(comp->current->this_impl);
	const AZImplementation *impl = comp->current->this_impl;
	void *inst = NULL;
	if (comp->current->this_val) {
		inst = az_value_get_inst(impl, comp->current->this_val);
	}
	return resolve_member_inst (comp, expr, klass, impl, inst, expr->value.v.string, flags);
}

#define noDEBUG_RESOLVE_VARIABLE

static unsigned int
resolve_variable (AZOCompiler *comp, AZOExpression *expr, unsigned int flags)
{
	/* Try global */
#ifdef DEBUG_RESOLVE_VARIABLE
	AZString *str = expr->value.v.string;
#endif
	assert (!expr->children);
	if (azo_context_lookup (comp->ctx, expr->value.v.string, &expr->value)) {
		expr->term.type = EXPRESSION_CONSTANT;
		expr->term.subtype = (expr->value.impl) ? AZ_PACKED_VALUE_TYPE(&expr->value) : 0;
#ifdef DEBUG_RESOLVE_VARIABLE
		/* If string resolved to global variable we can be sure it has at least reference left */
		fprintf (stderr, "resolve_variable: %s replaced with global object\n", str->str);
#endif
		return 0;
	}
	/* Try local */
	AZOVariable *var = azo_frame_lookup_var (comp->current, expr->value.v.string);
	if (var) {
		if (!(flags & AZO_COMPILER_VAR_IS_LVALUE) && var->const_expr) {
#ifdef DEBUG_RESOLVE_VARIABLE
			fprintf (stderr, "resolve_variable: Local %s is constant in this scope, replacing with constant expression\n", var->name->str);
#endif
			expr->term.type = EXPRESSION_CONSTANT;
			expr->term.subtype = var->const_expr->term.subtype;
			az_packed_value_copy (&expr->value, &var->const_expr->value);
			return 0;
		}
#ifdef DEBUG_RESOLVE_VARIABLE
		fprintf (stderr, "resolve_variable: Local %s at pos %u\n", expr->value.v.string->str, var->pos);
#endif
		expr->term.type = EXPRESSION_VARIABLE;
		expr->term.subtype = VARIABLE_LOCAL;
		az_packed_value_clear (&expr->value);
		expr->var_pos = var->pos;
		return 0;
	}
	/* Try if parent is already defined */
	var = azo_frame_lookup_parent_var (comp->current, expr->value.v.string);
	if (var) {
		if (!(flags & AZO_COMPILER_VAR_IS_LVALUE) && var->const_expr) {
#ifdef noDEBUG_RESOLVE_VARIABLE
			fprintf (stderr, "resolve_variable: Known parent %s is constant in this scope, replacing with constant expression\n", var->name->str);
#endif
			expr->term.type = EXPRESSION_CONSTANT;
			expr->term.subtype = var->const_expr->term.subtype;
			az_packed_value_copy (&expr->value, &var->const_expr->value);
			return 0;
		}
#ifdef DEBUG_RESOLVE_VARIABLE
		fprintf (stderr, "resolve_variable: Parent %s at pos %u\n", expr->value.v.string->str, var->pos);
#endif
		expr->term.type = EXPRESSION_VARIABLE;
		expr->term.subtype = VARIABLE_PARENT;
		az_packed_value_clear (&expr->value);
		expr->var_pos = var->pos;
		return 0;
	}
	/* Try parent frame */
	if (comp->current->parent) {
		var = azo_frame_lookup_chained (comp->current->parent, expr->value.v.string);
		if (var) {
			/* Parent */
			var = azo_frame_ensure_variable (comp->current, expr->value.v.string);
			if (!(flags & AZO_COMPILER_VAR_IS_LVALUE) && var->const_expr) {
#ifdef noDEBUG_RESOLVE_VARIABLE
				fprintf (stderr, "resolve_variable: New parent %s is constant in this scope, replacing with constant expression\n", var->name->str);
#endif
				expr->term.type = EXPRESSION_CONSTANT;
				expr->term.subtype = var->const_expr->term.subtype;
				az_packed_value_copy (&expr->value, &var->const_expr->value);
				return 0;
			}

#ifdef DEBUG_RESOLVE_VARIABLE
			fprintf (stderr, "Created parent variable %s at pos %u\n", expr->value.v.string->str, var->pos);
#endif
			expr->term.type = EXPRESSION_VARIABLE;
			expr->term.subtype = VARIABLE_PARENT;
			az_packed_value_clear (&expr->value);
			expr->var_pos = var->pos;
			return 0;
		}
	}
	/* Try this reference */
	if (comp->current->this_impl) {
		return resolve_this_reference (comp, expr, flags);
	}
	return 0;
}

AZOExpression *
azo_compiler_resolve_reference (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result)
{
	if (expr->term.subtype == REFERENCE_VARIABLE) {
		*result = resolve_variable (comp, expr, flags);
	} else if (expr->term.subtype == REFERENCE_MEMBER) {
		*result = resolve_member (comp, expr, flags);
	}
	return expr;
}

unsigned int resolve_call (AZOCompiler *comp, AZOExpression *expr, AZOExpression *ref, unsigned int n_args, unsigned int arg_types[], unsigned int flags)
{
	return 0;
}

#define noDEBUG_RESOLVE_FUNCTION_CALL

AZOExpression *
azo_compiler_resolve_function_call (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result)
{
	AZOExpression *ref, *args, *child;
	ref = expr->children;
	args = ref->next;
	assert (!args->next);
	*result = 0;
	if (args->term.type != EXPRESSION_LIST) {
		fprintf (stderr, "azo_compiler_resolve_function_call: arguments is not list\n");
		*result = 1;
		return expr;
	}

	ref = azo_compiler_resolve_reference (comp, ref, flags, result);
	if (*result) return expr;
	args = azo_compiler_resolve_expression (comp, args, flags, result);
	if (*result) return expr;

	/* If reference is already resolved to constant we have nothing to do */
	if (ref->term.type != EXPRESSION_REFERENCE) return expr;
	/* Test if arguments list is constant */
	unsigned int n_args = 0;
	unsigned int arg_types[64];
	for (child = args->children; child; child = child->next) {
		if (child->term.type != EXPRESSION_CONSTANT) return expr;
		arg_types[n_args] = child->term.subtype;
		n_args += 1;
		if (n_args >= 64) return expr;
	}
	/* All arguments are constants */
	const AZClass *klass;
	const AZImplementation *impl;
	void *inst;
	AZString *str;
	if (ref->term.subtype == REFERENCE_MEMBER) {
		AZOExpression *parent, *member;
		parent = ref->children;
		member = parent->next;
		if (parent->term.type != EXPRESSION_CONSTANT) return expr;
		if ((member->term.type != EXPRESSION_REFERENCE) || (member->term.subtype != REFERENCE_PROPERTY)) return expr;
		klass = az_type_get_class (parent->term.subtype);
		impl = parent->value.impl;
		inst = az_value_get_inst(parent->value.impl, &parent->value.v);
		str = member->value.v.string;
	} else if (ref->term.subtype == REFERENCE_VARIABLE) {
		if (!comp->current->this_impl) return expr;
		klass = AZ_CLASS_FROM_IMPL(comp->current->this_impl);
		impl = comp->current->this_impl;
		inst = (comp->current->this_val) ? az_value_get_inst(impl, comp->current->this_val) : NULL;
		str = ref->value.v.string;
	} else {
		fprintf (stderr, "azo_compiler_resolve_function_call: unknown reference subtype\n");
		*result = 1;
		return expr;
	}
	AZFunctionSignature *sig = az_function_signature_new(AZ_CLASS_TYPE(klass), AZ_TYPE_ANY, n_args, arg_types);
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	int idx = az_class_lookup_function (klass, impl, inst, str, sig, &def_class, &def_impl, &def_inst);
	az_function_signature_delete (sig);
	if (idx >= 0) {
		AZField *field = &def_class->props_self[idx];
		if (field->is_final && field->is_function) {
			const AZImplementation *prop_impl;
			AZValue64 prop_val;
			if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &prop_impl, &prop_val.value, 64, NULL)) {
				fprintf (stderr, "azo_compiler_resolve_function_call: Property %s is not readable\n", str->str);
				*result = 1;
				return expr;
			}
#if 1
			az_packed_value_set_from_impl_value (&ref->value, prop_impl, &prop_val.value);
			ref->term.type = EXPRESSION_CONSTANT;
			if (prop_impl) {
				ref->term.subtype = AZ_IMPL_TYPE(prop_impl);
				az_value_clear (prop_impl, &prop_val.value);
			} else {
				// Missing final function, this is not normal
				ref->term.subtype = 0;
			}
#ifdef DEBUG_RESOLVE_FUNCTION_CALL
			fprintf (stderr, "azo_compiler_resolve_function_call: Replaced final function %s with constant\n", str->str);
#endif
			// fixme: This is not nice but we keep parent for now as this implementation for call

			//while (ref->children) {
			//	child = ref->children;
			//	expr->children = child->next;
			//	azo_expression_free_tree (child);
			//}
#endif
			return expr;
		}
	}
	return expr;
}

#define noDEBUG_RESOLVE_NEW

AZOExpression *
azo_compiler_resolve_new (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result)
{
	static AZString *new_str = NULL;
	if (!new_str) new_str = az_string_new((const uint8_t *) "new");
	AZOExpression *ref, *args, *child;
	ref = expr->children;
	args = ref->next;
	assert (!args->next);
	*result = 0;
	ref = azo_compiler_resolve_reference (comp, ref, flags, result);
	if (*result) return expr;
	args = azo_compiler_resolve_expression (comp, args, flags, result);
	if (*result) return expr;

	if ((ref->term.type != EXPRESSION_CONSTANT) || (ref->term.subtype != AZ_TYPE_CLASS)) {
		fprintf (stderr, "azo_compiler_resolve_new: reference is not a class\n");
		*result = 1;
		return expr;
	}
	if (args->term.type != EXPRESSION_LIST) {
		fprintf (stderr, "azo_compiler_resolve_new: arguments is not list\n");
		*result = 1;
		return expr;
	}

	/* Test if arguments list is constant */
	unsigned int n_args = 0;
	unsigned int arg_types[64];
	for (child = args->children; child; child = child->next) {
		if (child->term.type != EXPRESSION_CONSTANT) return expr;
		arg_types[n_args] = child->term.subtype;
		n_args += 1;
		if (n_args >= 64) return expr;
	}
	/* All arguments are constants */
	const AZClass *klass;
	const AZImplementation *impl;
	klass = (AZClass *) ref->value.v.block;
	impl = (AZImplementation *) klass;
	AZFunctionSignature *sig = az_function_signature_new (AZ_TYPE_NONE, AZ_CLASS_TYPE(klass), n_args, arg_types);
	const AZClass *def_class;
	const AZImplementation *def_impl;
	void *def_inst;
	int idx = az_class_lookup_function (klass, impl, NULL, new_str, sig, &def_class, &def_impl, &def_inst);
	az_function_signature_delete (sig);
	if (idx >= 0) {
		AZField *field = &def_class->props_self[idx];
		if (field->is_final && field->is_function) {
			const AZImplementation *prop_impl;
			AZValue64 prop_val;
			if (!az_instance_get_property_by_id (def_class, AZ_CLASS_FROM_IMPL(def_impl), def_impl, def_inst, idx, &prop_impl, &prop_val.value, 64, NULL)) {
				fprintf (stderr, "azo_compiler_resolve_new: Property new is not readable\n");
				*result = 1;
				return expr;
			}
			az_packed_value_set_from_impl_value (&ref->value, prop_impl, &prop_val.value);
			expr->term.type = EXPRESSION_FUNCTION_CALL;
			expr->term.subtype = EXPRESSION_GENERIC;
			ref->term.type = EXPRESSION_CONSTANT;
			if (prop_impl) {
				ref->term.subtype = AZ_IMPL_TYPE(prop_impl);
				az_value_clear (prop_impl, &prop_val.value);
			} else {
				// Missing new, this is not normal
				ref->term.subtype = 0;
			}
#ifdef DEBUG_RESOLVE_NEW
			fprintf (stderr, "azo_compiler_resolve_new: Replaced new %s with constant\n", klass->name);
#endif
			return expr;
		}
	}
	return expr;
}
