    .section .rodata.user_image
    .balign 16
    .globl __user_image_start
    .globl __user_image_end
__user_image_start:
    .incbin "build/user.bin"
__user_image_end:
    .balign 16
    .globl __app_image_start
    .globl __app_image_end
__app_image_start:
    .incbin "build/app.bin"
__app_image_end:
