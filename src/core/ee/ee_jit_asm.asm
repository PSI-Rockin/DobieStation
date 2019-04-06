PUBLIC run_ee_jit
EXTERN exec_block:PROC

.data

.code

run_ee_jit PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    push rsi
    push rbx
    push rcx
    push rdx
    push r11
    push r12
    push r13
    push r14
    push r15
    push rdi

    sub rsp, 32
    call exec_block
    add rsp, 32
    sub rsp, 32
    call rax
    add rsp, 32

    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop rdx
    pop rcx
    pop rbx
    pop rsi
    pop rbp
    ret
run_ee_jit ENDP

END