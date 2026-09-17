SET(SOURCES
    arithmetic.c arithmetic.h
    compiler.c compiler.h
    frame.c frame.h
    calculate-constants.c
    resolve-frames.c
    resolve-references.c
    scope.c scope.h
    optimizer.c optimizer.h
    variable.c variable.h
    resolver.h
)

list(TRANSFORM SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

TARGET_SOURCES(azo PRIVATE ${SOURCES})
