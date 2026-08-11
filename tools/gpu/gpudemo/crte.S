/* crte.S — bionic-compatible crt: full __libc_init startup (runs ctors,
   __system_properties_init etc.) then main. */
    .syntax unified
    .arm
    .text
    .global _start
    .type _start, %function
_start:
    mov fp, #0
    mov lr, #0
    ldr r0, =main
    ldr r1, [sp]            @ argc
    add r2, sp, #4          @ argv
    add r3, r2, r1, lsl #2
    add r3, r3, #4          @ envp
    mov r4, #0              @ ctors = NULL
    sub sp, sp, #4
    str r4, [sp]            @ 5th arg (ctors)
    ldr r4, =__libc_init
    bx  r4
    .size _start, .-_start
    .end
