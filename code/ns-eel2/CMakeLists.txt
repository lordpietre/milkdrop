# ns-eel2: Nullsoft Expression Evaluator Library v2
# Portable C library, used by MilkDrop3 for runtime preset evaluation
#
# NOTE: Platform-specific assembly files (asm-nseel-x86-*.c) are
# included via #include in nseel-cfunc.c, not compiled separately.

add_library(nseel2 STATIC
    nseel-caltab.c
    nseel-cfunc.c
    nseel-compiler.c
    nseel-eval.c
    nseel-lextab.c
    nseel-ram.c
    nseel-yylex.c
)

target_include_directories(nseel2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Disable some warnings for ns-eel2 code
target_compile_options(nseel2 PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-function -Wno-unused-variable -Wno-sign-compare>
)
