SET(SOURCES
    arithmetic.c arithmetic.h
    compiler.c compiler.h
    frame.c frame.h
    resolve-constants.c
    resolve-frames.c
    resolve-references.c
)

list(TRANSFORM SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

TARGET_SOURCES(azo PRIVATE ${SOURCES})
