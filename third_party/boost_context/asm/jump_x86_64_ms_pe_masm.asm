
;           Copyright Oliver Kowalke 2009.
;  Distributed under the Boost Software License, Version 1.0.
;     (See accompanying file LICENSE_1_0.txt or copy at
;           http://www.boost.org/LICENSE_1_0.txt)

;  ----------------------------------------------------------------------------------
;  |     0   |     1   |     2    |     3   |     4   |     5   |     6   |     7   |
;  ----------------------------------------------------------------------------------
;  |    0x0  |    0x4  |    0x8   |    0xc  |   0x10  |   0x14  |   0x18  |   0x1c  |
;  ----------------------------------------------------------------------------------
;  |      fbr_strg     |      fc_dealloc    |       limit       |        base       |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |     8   |    9    |    10    |    11   |    12   |    13   |    14   |    15   |
;  ----------------------------------------------------------------------------------
;  |   0xc20 |  0x24   |   0x28   |   0x2c  |   0x30  |   0x34  |   0x38  |   0x3c  |
;  ----------------------------------------------------------------------------------
;  |        R12        |         R13        |        R14        |        R15        |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    16   |    17   |    18   |    19    |    20   |    21   |    22   |    23   |
;  ----------------------------------------------------------------------------------
;  |   0xe40  |   0x44 |   0x48  |   0x4c   |   0x50  |   0x54  |   0x58  |   0x5c  |
;  ----------------------------------------------------------------------------------
;  |        RDI         |       RSI         |        RBX        |        RBP        |
;  ----------------------------------------------------------------------------------
;  ----------------------------------------------------------------------------------
;  |    24   |   25    |    26    |   27    |    28   |    29   |    30   |    31   |
;  ----------------------------------------------------------------------------------
;  |   0x60  |   0x64  |   0x68   |   0x6c  |   0x70  |   0x74  |   0x78  |   0x7c  |
;  ----------------------------------------------------------------------------------
;  |        RIP        |        EXIT        |                                       |
;  ----------------------------------------------------------------------------------

.code

jump_fcontext PROC FRAME
    .endprolog

    push  rbp  ; save RBP
    push  rbx  ; save RBX
    push  rsi  ; save RSI
    push  rdi  ; save RDI
    push  r15  ; save R15
    push  r14  ; save R14
    push  r13  ; save R13
    push  r12  ; save R12

    ; load NT_TIB
    mov  r10,  gs:[030h]
    ; save current stack base
    mov  rax,  [r10+08h]
    push  rax
    ; save current stack limit
    mov  rax, [r10+010h]
    push  rax
    ; save current deallocation stack
    mov  rax, [r10+01478h]
    push  rax
    ; save fiber local storage
    mov  rax, [r10+018h]
    push  rax

    ; store RSP (pointing to context-data) in RCX
    mov  [rcx], rsp

    ; restore RSP (pointing to context-data) from RDX
    mov  rsp, rdx

    ; load NT_TIB
    mov  r10, gs:[030h]
    ; restore fiber local storage
    pop  rax
    mov  [r10+018h], rax
    ; restore deallocation stack
    pop  rax
    mov  [r10+01478h], rax
    ; restore stack limit
    pop  rax
    mov  [r10+010h], rax
    ; restore stack base
    pop  rax
    mov  [r10+08h], rax

    pop  r12  ; restore R12
    pop  r13  ; restore R13
    pop  r14  ; restore R14
    pop  r15  ; restore R15
    pop  rdi  ; restore RDI
    pop  rsi  ; restore RSI
    pop  rbx  ; restore RBX
    pop  rbp  ; restore RBP

    ; restore return-address
    pop  r10

    ; use third arg as return-value after jump
    mov  rax, r8
    ; use third arg as first arg in context function
    mov  rcx, r8

    ; indirect jump to context
    jmp  r10
jump_fcontext ENDP
END