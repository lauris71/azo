set(AZO_HEADERS
	bytecode.h
	compare.h
	compiled-function.h
	context.h
	debug.h
	exception.h
	expression.h
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
	bytecode.c
	compare.c
	compiled-function.c
	context.c
	debug.c
	exception.c
	expression.c
	interpreter.c
	keyword.c
	namespace.c
	number.c
	operator.c
	optimizer.c
	parser.c
	private.c
	program.c
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

add_subdirectory(compiler)
