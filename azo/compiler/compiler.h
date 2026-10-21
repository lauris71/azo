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

enum {
	AZO_COMPILER_NO_THIS,
	AZO_COMPILER_THIS_IS_STACK,
	AZO_COMPILER_THIS_IS_SHARED,
	AZO_COMPILER_THIS_IS_CAPTURE
};
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
