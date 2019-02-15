PUBLIC run_vu_jit
EXTERN exec_block:PROC

.data

.code

run_vu_jit PROC
    push rbp
    mov rbp, rsp
    push rsi
	push rax
    push rbx
    push rcx
    push rdx
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
    pop rdx
    pop rcx
    pop rbx
	pop rax
    pop rsi
    pop rbp
    ret
run_vu_jit ENDP

END