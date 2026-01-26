set(AZO_HEADERS
	arithmetic.h
	bytecode.h
	compare.h
	compiled-function.h
	compiler.h
	context.h
	exception.h
	expression.h
	frame.h
	interpreter.h
	keyword.h
	namespace.h
	operator.h
	optimizer.h
	parser.h
	private.h
	program.h
	source.h
	stack.h
	tokenizer.h
)

set(AZO_SOURCES
	arithmetic.c
	bytecode.c
	compare.c
	compiled-function.c
	compiler.c
	context.c
	exception.c
	expression.c
	frame.c
	interpreter.c
	keyword.c
	namespace.c
	number.c
	operator.c
	optimizer.c
	parser.c
	private.c
	program.c
	resolve-constants.c
	resolve-frames.c
	resolve-references.c
	source.c
	stack.c
	tokenizer.c
)

add_library(azo STATIC
	${AZO_HEADERS}
	${AZO_SOURCES}
)

set_property(TARGET azo PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(azo PUBLIC
	${PROJECT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/arikkei
    ${CMAKE_SOURCE_DIR}/az
)
