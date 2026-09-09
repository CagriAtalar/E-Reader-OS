; boot.asm — cagOS readerOS BIOS bootloader
;
; Strategy: Load kernel+blob into a temporary buffer below 1MB using INT 13h,
; then switch to protected mode and copy everything to 0x100000.
;
; Since we have max ~22 sectors (~11KB), we can fit it all below 640KB easily.
; Temp buffer starts at 0x10000 (64KB mark), leaving plenty of room.

[bits 16]
[org 0x7C00]

KERNEL_LOAD_ADDR    equ 0x100000    ; Final destination: 1MB
TEMP_BUFFER         equ 0x10000     ; Temporary load address in real mode
TEMP_SEG            equ 0x1000      ; Segment for 0x10000

start:
    ; Set up segments and stack
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save boot drive number
    mov [boot_drive], dl

    ; Set VGA text mode 80x25 (mode 3)
    mov ax, 0x0003
    int 0x10

    ; Print "BOOT" to confirm we entered
    mov si, msg_boot
    call print_str

    ; --- Load sectors from disk into temp buffer ---
    mov si, msg_loading
    call print_str

    ; How many sectors to load
    mov ecx, [total_sectors]
    test ecx, ecx
    jz .load_done

    ; We'll read in chunks to handle BIOS limitations
    mov word [dap_off], 0x0000
    mov word [dap_seg], TEMP_SEG
    mov dword [dap_lba_lo], 1   ; Start from LBA 1
    mov dword [dap_lba_hi], 0

.read_loop:
    cmp ecx, 0
    je .load_done

    ; Read up to 64 sectors at a time
    mov eax, ecx
    cmp eax, 64
    jbe .count_ok
    mov eax, 64
.count_ok:
    mov [dap_count], ax

    ; Call INT 13h extended read
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc disk_error

    ; Advance counters
    movzx eax, word [dap_count]
    sub ecx, eax

    ; Advance LBA
    add [dap_lba_lo], eax

    ; Advance buffer pointer (each sector = 512 bytes = 0x200)
    movzx edx, word [dap_seg]
    shl eax, 5              ; sectors * 32 paragraphs per sector
    add edx, eax
    mov [dap_seg], dx

    ; Progress dot
    mov al, '.'
    mov ah, 0x0E
    int 0x10

    jmp .read_loop

.load_done:
    mov si, msg_ok
    call print_str

    ; --- Enable A20 ---
    call enable_a20

    ; --- Switch to protected mode ---
    cli
    lgdt [gdtr]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:pm_entry       ; Far jump to flush pipeline

; ---- Real mode helpers ----
print_str:
    lodsb
    test al, al
    jz .pret
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
    jmp print_str
.pret:
    ret

disk_error:
    mov si, msg_disk_err
    call print_str
    cli
    hlt
    jmp $

enable_a20:
    ; Try fast A20 via port 0x92
    in al, 0x92
    test al, 0x02
    jnz .a20done
    or al, 0x02
    and al, 0xFE
    out 0x92, al
.a20done:
    ret

; ---- Data ----
boot_drive:     db 0

msg_boot:       db "BOOT", 0x0D, 0x0A, 0
msg_loading:    db "Loading", 0
msg_ok:         db " OK", 0x0D, 0x0A, 0
msg_disk_err:   db "DISK ERR", 0

; Total sectors to load (set by Makefile via -DTOTAL_SECTORS=N)
total_sectors:
%ifdef TOTAL_SECTORS
    dd TOTAL_SECTORS
%else
    dd 1
%endif

; DAP (Disk Address Packet) for INT 13h extended read
align 2
dap:
    db 0x10              ; Size of DAP = 16
    db 0                 ; Reserved
dap_count:
    dw 0                 ; Sectors to read
dap_off:
    dw 0x0000            ; Buffer offset
dap_seg:
    dw TEMP_SEG          ; Buffer segment
dap_lba_lo:
    dd 0                 ; LBA low 32 bits
dap_lba_hi:
    dd 0                 ; LBA high 32 bits

; GDT
align 8
gdt:
    dq 0                            ; Null descriptor
    ; 0x08: Code segment — base=0, limit=4GB, 32-bit, execute/read
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
    ; 0x10: Data segment — base=0, limit=4GB, 32-bit, read/write
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt

; ---- 32-bit protected mode ----
[bits 32]
pm_entry:
    ; Set up 32-bit data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; Stack at 576KB

    ; Copy from temp buffer (0x10000) to final address (0x100000)
    mov esi, TEMP_BUFFER
    mov edi, KERNEL_LOAD_ADDR
    mov ecx, [total_sectors]
    shl ecx, 7              ; sectors * 128 = dwords to copy (sectors * 512 / 4)
    rep movsd

    ; Jump to kernel!
    jmp KERNEL_LOAD_ADDR

; ---- Pad to 512 bytes ----
times 510 - ($ - $$) db 0
dw 0xAA55
