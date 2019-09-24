PUBLIC jit_draw_pixel_asm
PUBLIC jit_tex_lookup_asm

.data

.code

jit_draw_pixel_asm PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 40h
    .setframe rbp, 40h
    .endprolog
    mov [rbp - 8h], r12
    mov [rbp - 10h], r13
    mov [rbp - 18h], r14
    mov [rbp - 20h], r15
    mov rax, rcx
    mov r12, rdx
    mov r13, r8
    mov r14, r9
    mov r15, [rbp + 30h]
    mov r15, [r15]
    call rax
    mov r15, [rbp - 20h]
    mov r14, [rbp - 18h]
    mov r13, [rbp - 10h]
    mov r12, [rbp - 8h]
    add rsp, 40h
    pop rbp
    ret
jit_draw_pixel_asm ENDP

jit_tex_lookup_asm PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 40h
    .setframe rbp, 40h
    .endprolog
    mov [rbp - 8h], r12
    mov [rbp - 10h], r13
    mov [rbp - 18h], r14
    mov [rbp - 20h], r15
    xor r12, r12
    xor r13, r13
    mov rax, rcx
    mov r12w, dx
    mov r13w, r8w
    mov r14, r9
    call rax
    mov r15, [rbp - 20h]
    mov r14, [rbp - 18h]
    mov r13, [rbp - 10h]
    mov r12, [rbp - 8h]
    add rsp, 40h
    pop rbp
    ret
jit_tex_lookup_asm ENDP

END