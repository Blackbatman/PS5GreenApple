/* fonts.S - embed Liberation fonts into the binary (works with gcc & clang)
 *
 * Portable across ELF (PS5 payload, Linux host) and Mach-O (macOS sim):
 *  - .S is preprocessed, so we can key off __MACH__ / __ELF__.
 *  - Mach-O needs segment-style sections (__TEXT,__const) and prefixes every
 *    C-visible symbol with an underscore; ELF does not.
 *  - The ELF branch stays byte-identical to the pre-port file, so the PS5
 *    payload hash is unaffected by this change.
 */

#if defined(__MACH__)
#define SYM(name) _##name
#else
#define SYM(name) name
#endif

#if defined(__MACH__)
.section __TEXT,__const
#else
.section .rodata
#endif

.balign 16
.global SYM(font_sans_reg_start)
SYM(font_sans_reg_start):
    .incbin "assets/fonts/LiberationSans-Regular.ttf"
.global SYM(font_sans_reg_end)
SYM(font_sans_reg_end):

.balign 16
.global SYM(font_sans_bold_start)
SYM(font_sans_bold_start):
    .incbin "assets/fonts/LiberationSans-Bold.ttf"
.global SYM(font_sans_bold_end)
SYM(font_sans_bold_end):

.balign 16
.global SYM(font_mono_start)
SYM(font_mono_start):
    .incbin "assets/fonts/LiberationMono-Regular.ttf"
.global SYM(font_mono_end)
SYM(font_mono_end):

/* ELF-only: mark the stack as non-executable (no-op on Mach-O) */
#if !defined(__MACH__)
.section .note.GNU-stack,"",@progbits
#endif
