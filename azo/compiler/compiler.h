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

enum {
	AZO_COMPILER_NO_THIS,
	// fixme: Join argument and variable -> stack
	AZO_COMPILER_THIS_IS_ARGUMENT,
	AZO_COMPILER_THIS_IS_VARIABLE,
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
	 * This may be resolved in 4 different ways
	 * - As just one variable (happens in member block)
	 * - As a captured variable (happens in lambda)
	 * - As the first program argument (if program is compiled as a member of class)
	 * - As a static constant (if program is compiled as a member of specific instance)
	 *
	 * During resolve explici or implici 'this' is replaced by specific access variant (VARIABLE or CONSTANT)
	 */
	unsigned int this_variant;
	union {
		/* For variable */
		unsigned int this_var_pos;
		/* For capture */
		unsigned int this_capture_pos;
		/* Argument is always 0 */
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
