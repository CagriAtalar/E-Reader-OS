; hello.asm — Minimal "Hello" boot sector for M0 verification
; Prints "Hello, cagOS!" via BIOS INT 10h teletype, then halts.

[bits 16]
[org 0x7C00]

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack below boot sector

    ; Print string via BIOS teletype
    mov si, msg
.print_loop:
    lodsb                   ; Load byte from [SI] into AL, increment SI
    test al, al             ; Null terminator?
    jz .done
    mov ah, 0x0E            ; INT 10h: teletype output
    mov bh, 0x00            ; Page number
    int 0x10
    jmp .print_loop

.done:
    cli                     ; Disable interrupts
    hlt                     ; Halt CPU
    jmp .done               ; In case of NMI

msg: db "Hello, cagOS!", 0x0D, 0x0A, 0

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
