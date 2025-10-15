    .text
    .globl context_switch
    .type context_switch, @function
context_switch:
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    ret
