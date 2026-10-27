/* fonts.S - embed Liberation fonts into the binary (works with gcc & clang) */
#if defined(__APPLE__)
#define ASM_NAME(sym) _##sym
#else
#define ASM_NAME(sym) sym
#endif

#if defined(__APPLE__)
    .section __TEXT,__rodata,regular,pure_instructions
    .align 4
#else
    .section .rodata
    .balign 16
#endif

.global ASM_NAME(font_sans_reg_start)
ASM_NAME(font_sans_reg_start):
    .incbin "assets/fonts/LiberationSans-Regular.ttf"
.global ASM_NAME(font_sans_reg_end)
ASM_NAME(font_sans_reg_end):

#if defined(__APPLE__)
    .align 4
#else
    .balign 16
#endif

.global ASM_NAME(font_sans_bold_start)
ASM_NAME(font_sans_bold_start):
    .incbin "assets/fonts/LiberationSans-Bold.ttf"
.global ASM_NAME(font_sans_bold_end)
ASM_NAME(font_sans_bold_end):

#if defined(__APPLE__)
    .align 4
#else
    .balign 16
#endif

.global ASM_NAME(font_mono_start)
ASM_NAME(font_mono_start):
    .incbin "assets/fonts/LiberationMono-Regular.ttf"
.global ASM_NAME(font_mono_end)
ASM_NAME(font_mono_end):

#if !defined(__APPLE__)
    .section .note.GNU-stack,"",@progbits
#endif
