/* crte.S — minimal bionic crt: call main, then exit(r0). */
    .syntax unified
    .arm
    .text
    .global _start
    .type _start, %function
_start:
    mov fp, #0
    mov lr, #0
    bl main
    bl exit
    .size _start, .-_start
    .end
