.text
    .globl __fiber_clone
__fiber_clone:
    movq 0x00(%rsp), %rdi
    movq 0x08(%rsp), %rsi
    movq 0x10(%rsp), %rdx
    callq fiber_start@plt
    
spin:
    jmp spin
