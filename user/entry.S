    .section .text.entry
    .globl _user_start
_user_start:
    call user_main
    # user_main returns its exit status in a0.
    call exit
1:
    j 1b
