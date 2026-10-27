/* fonts.S - embed Liberation fonts into the binary (works with gcc & clang) */
#if defined(__APPLE__)
#define ASM_NAME(sym) _##sym
#define ASM_SECTION   .section __TEXT,__rodata,regular,pure_instructions
#define ASM_ALIGN     .align 4
#else
#define ASM_NAME(sym) sym
#define ASM_SECTION   .section .rodata
#define ASM_ALIGN     .balign 16
#endif

ASM_SECTION
ASM_ALIGN

.global ASM_NAME(font_sans_reg_start)
ASM_NAME(font_sans_reg_start):
    .incbin "assets/fonts/LiberationSans-Regular.ttf"
.global ASM_NAME(font_sans_reg_end)
ASM_NAME(font_sans_reg_end):

ASM_ALIGN

.global ASM_NAME(font_sans_bold_start)
ASM_NAME(font_sans_bold_start):
    .incbin "assets/fonts/LiberationSans-Bold.ttf"
.global ASM_NAME(font_sans_bold_end)
ASM_NAME(font_sans_bold_end):

ASM_ALIGN

.global ASM_NAME(font_mono_start)
ASM_NAME(font_mono_start):
    .incbin "assets/fonts/LiberationMono-Regular.ttf"
.global ASM_NAME(font_mono_end)
ASM_NAME(font_mono_end):

#if !defined(__APPLE__)
    .section .note.GNU-stack,"",@progbits
#endif
