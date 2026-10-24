#ifndef __AZO_COMPILER_H__
#define __AZO_COMPILER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;
typedef struct _AZOCompilerContext AZOCompilerContext;

#include <stdint.h>

#include <azo/context.h>
#include <azo/compiler/frame.h>
#include <azo/compiler/resolver.h>
#include <azo/node.h>
#include <azo/interpreter.h>
#include <azo/source.h>
#include <azo/calculate.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of compile-time assignment type classification
 * 
 * Given target type Ta and inferred source type Ib:
 * - ACCEPT: the assignment always succeeds, no runtime check needed
 * - REJECT: the assignment can never succeed, compile-time error
 * - CHECK:  a runtime check is needed (emit EXCEPTION_IF_NOT_ASSIGNABLE_IMMEDIATE)
 */
enum {
	AZO_ASSIGN_ACCEPT,
	AZO_ASSIGN_REJECT,
	AZO_ASSIGN_CHECK
};

static inline unsigned int
azo_type_assignment_classify (unsigned int Ta, unsigned int Ib)
{
	/* Unknown target type - nothing to check */
	if (!Ta) return AZO_ASSIGN_ACCEPT;
	/* Ib assignable to Ta (exact, subtype, interface, or null to block/any) */
	if (az_type_is_assignable_to (Ib, Ta)) return AZO_ASSIGN_ACCEPT;
	/* Arithmetic types are mutually convertible: the assignment is a conversion
	 * (a = (TYPE_OF(a)) b), so rounding/clamping conditions are checked (and
	 * masked per computation mode) at runtime, not rejected here */
	if (AZ_TYPE_IS_ARITHMETIC (Ta) && AZ_TYPE_IS_ARITHMETIC (Ib)) return AZO_ASSIGN_ACCEPT;
	/* Leaf type - the actual type is exactly Ib, which is not assignable */
	if (AZ_TYPE_IS_FINAL (Ib)) return AZO_ASSIGN_REJECT;
	/* A subtype of Ib may be assignable to Ta */
	if (az_type_is_a (Ta, Ib) || AZ_TYPE_IS_INTERFACE (Ta)) return AZO_ASSIGN_CHECK;
	return AZO_ASSIGN_REJECT;
}

/**
 * @brief Classify an assignment source node against a target type
 * 
 * A source with NONE type is either a null literal (CONSTANT) or an unknown
 * (not-yet-inferred) expression. Unknown sources need a runtime check unless
 * the target is unrestricted.
 */
static inline unsigned int
azo_type_assignment_classify_source (const AZONode *source, unsigned int Ta)
{
	unsigned int Ib = source->type;
	if (!Ib && (source->term.type != AZO_TERM_CONSTANT)) {
		if (Ta == AZ_TYPE_ANY) return AZO_ASSIGN_ACCEPT;
		return AZO_ASSIGN_CHECK;
	}
	return azo_type_assignment_classify (Ta, Ib);
}

/* Bit width of an integer type, 0 for non-integers */
static inline unsigned int
azo_type_int_width (unsigned int type)
{
	switch (type) {
	case AZ_TYPE_INT8: case AZ_TYPE_UINT8: return 8;
	case AZ_TYPE_INT16: case AZ_TYPE_UINT16: return 16;
	case AZ_TYPE_INT32: case AZ_TYPE_UINT32: return 32;
	case AZ_TYPE_INT64: case AZ_TYPE_UINT64: return 64;
	default: return 0;
	}
}

/* Whether every value of the source type Ib is representable in the target type
 * Ta without magnitude loss (clamping). Precision loss (rounding) is allowed. */
static inline unsigned int
azo_type_is_magnitude_safe (unsigned int Ta, unsigned int Ib)
{
	if (!AZ_TYPE_IS_ARITHMETIC (Ta) || !AZ_TYPE_IS_ARITHMETIC (Ib)) return 0;
	if (Ta == Ib) return 1;
	if (AZ_TYPE_IS_INTEGRAL (Ta) && AZ_TYPE_IS_INTEGRAL (Ib)) {
		unsigned int tw = azo_type_int_width (Ta);
		unsigned int iw = azo_type_int_width (Ib);
		if (!AZ_TYPE_IS_SIGNED (Ib)) {
			/* unsigned source: unsigned target at least as wide, or signed target strictly wider */
			return AZ_TYPE_IS_SIGNED (Ta) ? (tw > iw) : (tw >= iw);
		}
		/* signed source: needs a signed target at least as wide */
		return AZ_TYPE_IS_SIGNED (Ta) && (tw >= iw);
	}
	/* Integer source: any floating/complex target preserves magnitude (rounds at most) */
	if (AZ_TYPE_IS_INTEGRAL (Ib)) return 1;
	/* Floating-point widening */
	if ((Ib == AZ_TYPE_FLOAT) && ((Ta == AZ_TYPE_DOUBLE) || (Ta == AZ_TYPE_COMPLEX_FLOAT) || (Ta == AZ_TYPE_COMPLEX_DOUBLE))) return 1;
	if ((Ib == AZ_TYPE_DOUBLE) && (Ta == AZ_TYPE_COMPLEX_DOUBLE)) return 1;
	if ((Ib == AZ_TYPE_COMPLEX_FLOAT) && (Ta == AZ_TYPE_COMPLEX_DOUBLE)) return 1;
	return 0;
}

enum {
	AZO_COMPILER_NO_THIS,
	AZO_COMPILER_THIS_IS_STACK,
	AZO_COMPILER_THIS_IS_SHARED,
	AZO_COMPILER_THIS_IS_CAPTURE
};

/* Default computation-mode exception mask: rounding and non-finite results are
 * masked (suppressed); magnitude change and integer overflow throw by default */
#define AZO_COMPILER_DEFAULT_EXCEPTION_MASK (AZO_INTR_CHANGE_OF_PRECISION_MASK | AZO_INTR_NOT_FINITE_MASK)

/* Adjust an exception mask for a computation mode (AZO_TERM_FLAG_* mode bits) */
static inline unsigned int
azo_compiler_mode_mask (unsigned int mask, unsigned int mode_flags)
{
	if (mode_flags & AZO_TERM_FLAG_FINITE) {
		/* finite: non-finite results throw */
		mask &= ~AZO_INTR_NOT_FINITE_MASK;
	}
	if (mode_flags & (AZO_TERM_FLAG_SATURATE | AZO_TERM_FLAG_WRAP)) {
		/* saturate/wrap: magnitude change and integer overflow are suppressed */
		mask |= (AZO_INTR_CHANGE_OF_MAGNITUDE_MASK | AZO_INTR_INTEGER_OVERFLOW_MASK);
	}
	return mask;
}

/* Calculation mode for a computation mode (AZO_TERM_FLAG_* mode bits) */
static inline unsigned int
azo_compiler_mode_calc (unsigned int mode_flags)
{
	return (mode_flags & AZO_TERM_FLAG_WRAP) ? AZO_CALCULATE_MODE_WRAP : AZO_CALCULATE_MODE_SATURATE;
}
/**
 * @brief Compiler context
 * 
 * Defines this object, argument types and return type
 * 
 */
struct _AZOCompilerContext {
	/**
	 * @brief General context
	 * 
	 */
	unsigned int ret_type;
	/* The current frame */
	AZOFrame *frame;

	/**
	 * @brief Computation-mode state
	 * 
	 * Exception mask of currently masked (suppressed) exceptions, using the
	 * same bit layout as the interpreter (1U << AZO_EXCEPTION_*).
	 */
	unsigned int exception_mask;
	/* Integer overflow/clamp handling: AZO_CALCULATE_MODE_SATURATE or _WRAP */
	unsigned int calc_mode;

	/**
	 * @brief This handling
	 * 
	 * This may be resolved in 3 different ways
	 * - As a stack variable (this argument or member block variable)
	 * - As a captured variable (happens in lambda)
	 * - As a static constant (if program is compiled as a member of specific instance)
	 *
	 * During resolve explicit or implicit 'this' is replaced by specific access variant (VARIABLE or CONSTANT)
	 */
	unsigned int this_variant;
	union {
		/* For stack */
		unsigned int this_var_pos;
		/* For capture */
		unsigned int this_capture_pos;
		/* For static constant */
		unsigned int this_static_pos;
	};

	/* Compiler */
	/* The number of variables pushed into stack */
	unsigned int n_stack;
	/* debug */
	unsigned int print_tree;
};

struct _AZOCompiler {
	/**
	 * @brief Global definitions
	 * 
	 */
	AZOContext *globals;
	/**
	 * @brief Link to source
	 * 
	 */
	AZOSource *src;
	/**
	 * @brief Force typecode argument checking
	 * 
	 * This is only needed to debug compiler/interpreter
	 * 
	 */
	unsigned int check_args : 1;
	/**
	 * @brief Write debug symbols
	 * 
	 */
	unsigned int debug : 1;

	unsigned int n_frames_allocated;
	unsigned int n_frames;
	AZOFrame **frames;
};

void azo_compiler_setup (AZOCompiler *compiler, AZOContext *globals, AZOSource *src);
void azo_compiler_release (AZOCompiler *compiler);

/**
 * @brief Resolves references and types in parsed tree
 * 
 * Argumets must be already declared as variables.
 * 
 * References are replaced with either VARIABLE or CONSTANT nodes
 * All type expressions must resolve to constants and are replaced by TYPE nodes
 * 
 * @param comp A compiler
 * @param rctx Resolve context
 * @param root The root node of the parsed tree
 * @return 0 if successful, non-zero otherwise
 */
int azo_compiler_resolve_program(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node);

AZOProgram *azo_compiler_compile (AZOCompiler *comp, AZOCompilerContext *ctx, AZONode *root, AZOSource *src);

/**
 * @brief Create a new function frame
 * 
 * The argument are exactly as in function signature, e.g. for member functions this has to be
 * explicitly added as the first argument.
 * this_impl/this_inst are only for specifying constant this
 *  
 * @param comp The compiler instance 
 * @param parent The parent frame (or NULL for root).
 * @param capture_this Whether to capture 'this' in this frame.
 * @param n_args The number of arguments.
 * @param ret_type The return type.
 * @return AZOFrame* The new function frame.
 */
AZOFrame *azo_compiler_new_frame(AZOCompiler *comp, AZOFrame *parent, unsigned int capture_this, unsigned int n_args, unsigned int ret_type);

unsigned int azo_compiler_compile_expression (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, AZOSource *src);

#ifdef __cplusplus
}
#endif

#endif
