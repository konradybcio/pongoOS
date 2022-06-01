.align 4
barcode_space_narrow:
    ldr x26, =0x600*4*8
    adds x25, x25, x26
    ret
barcode_space_wide:
    ldr x26, =0x600*4*16
    adds x25, x25, x26
    ret
barcode_stripe_narrow:
    mov x26, #0x600*8      // Count
    1:
        str w27, [x25], #4
        subs x26, x26, #1
        bne 1b
    ret
barcode_stripe_wide:
    mov x26, #0x600*16      // Count
    1:
        str w27, [x25], #4
        subs x26, x26, #1
        bne 1b
    ret
barcode_print_a_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_1_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_2_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_4_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_6_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_7_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_wide
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_8_codabar:
    mov x0, lr
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_wide
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret
barcode_print_9_codabar:
    mov x0, lr
    bl barcode_stripe_wide
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_wide
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    bl barcode_stripe_narrow
    bl barcode_space_narrow
    mov lr, x0
    ret