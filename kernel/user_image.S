    .section .rodata.user_image
    .balign 16
    .globl __user_image_start
    .globl __user_image_end
__user_image_start:
    .incbin "build/user.bin"
__user_image_end:
