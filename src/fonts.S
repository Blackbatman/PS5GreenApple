/* fonts.S - embed Liberation fonts into the binary (works with gcc & clang) */
.section .rodata

.balign 16
.global font_sans_reg_start
font_sans_reg_start:
    .incbin "assets/fonts/LiberationSans-Regular.ttf"
.global font_sans_reg_end
font_sans_reg_end:

.balign 16
.global font_sans_bold_start
font_sans_bold_start:
    .incbin "assets/fonts/LiberationSans-Bold.ttf"
.global font_sans_bold_end
font_sans_bold_end:

.balign 16
.global font_mono_start
font_mono_start:
    .incbin "assets/fonts/LiberationMono-Regular.ttf"
.global font_mono_end
font_mono_end:

.section .note.GNU-stack,"",@progbits
