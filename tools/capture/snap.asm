; SNAP.COM: a small resident DOS program that saves the screen to a file at
; a fixed interval, so the original game can be captured while it runs
; headless (DOSBox-X -silent). Our own code, for studying the original.
;
; Every INTERVAL timer ticks (about 18.2 per second), when DOS is not busy,
; it writes SNAPnnnn.BIN in the current directory:
;   'CHRS' (4)  version (1)  video mode (1)  CRTC start address (2)
;   attribute palette registers 0-15 (16)  DAC 256 x RGB, 6-bit (768)
;   then the picture:
;     mode 0Dh (EGA 320x200x16): planes 0-3, 8000 bytes each, from the
;       CRTC start address;
;     mode 13h (MCGA 320x200x256): 64000 bytes from A000:0000;
;     text modes 0-3: 4000 bytes (80x25 characters and attributes) from B800:0000;
;     other modes: nothing (the header still records the mode).
; Usage: SNAP [ticks]   (default 18, about one capture a second; at most
; 9999 files).
;
; Build: nasm -f bin -o SNAP.COM snap.asm

        org 100h
        jmp install

INTERVAL_DEFAULT equ 18

old8        dd 0
indos       dd 0
interval    dw INTERVAL_DEFAULT
ticks       dw 0
busy        db 0
count       dw 0
saved_ss    dw 0
saved_sp    dw 0
fname       db 'SNAP0000.BIN', 0
header      db 'CHRS', 1
h_mode      db 0
h_start     dw 0
h_attr      times 16 db 0
h_dac       times 768 db 0
header_end:

timer:
        pushf
        call far [cs:old8]          ; the original handler runs first (EOI)
        cli
        inc word [cs:ticks]
        push ax
        mov ax, [cs:ticks]
        cmp ax, [cs:interval]
        jb .out
        cmp byte [cs:busy], 0
        jne .out
        push ds
        push bx
        lds bx, [cs:indos]
        cmp byte [bx], 0
        pop bx
        pop ds
        jne .out                    ; DOS busy: try again next tick
        cmp word [cs:count], 9999
        jae .out
        mov word [cs:ticks], 0
        mov byte [cs:busy], 1
        mov [cs:saved_ss], ss
        mov [cs:saved_sp], sp
        mov ax, cs
        mov ss, ax
        mov sp, stack_top
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push bp
        call capture
        pop bp
        pop es
        pop ds
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        mov ss, [cs:saved_ss]
        mov sp, [cs:saved_sp]
        mov byte [cs:busy], 0
.out:
        pop ax
        iret

; Writes one capture. Saves and restores the graphics controller registers
; it touches (index, read map select, mode).
capture:
        push cs
        pop ds
        ; video mode from the BIOS data area
        push es
        mov ax, 40h
        mov es, ax
        mov al, [es:49h]
        pop es
        mov [h_mode], al
        ; CRTC start address
        mov dx, 3d4h
        in al, dx
        mov bl, al                  ; saved CRTC index
        mov al, 0ch
        out dx, al
        inc dx
        in al, dx
        mov ah, al
        dec dx
        mov al, 0dh
        out dx, al
        inc dx
        in al, dx
        mov [h_start], ax
        dec dx
        mov al, bl
        out dx, al
        ; attribute palette registers 0-15 (bit 5 set keeps the display on)
        mov dx, 3dah
        in al, dx                   ; reset the attribute flip-flop
        mov dx, 3c0h
        in al, dx
        mov bh, al                  ; saved attribute index
        xor cx, cx
        mov si, h_attr
.attr:
        mov dx, 3dah
        in al, dx
        mov dx, 3c0h
        mov al, cl
        or al, 20h
        out dx, al
        inc dx
        in al, dx
        mov [si], al
        inc si
        inc cl
        cmp cl, 16
        jb .attr
        mov dx, 3dah
        in al, dx
        mov dx, 3c0h
        mov al, bh
        out dx, al
        ; DAC
        mov dx, 3c7h
        xor al, al
        out dx, al
        mov dx, 3c9h
        mov si, h_dac
        mov cx, 768
.dac:
        in al, dx
        mov [si], al
        inc si
        loop .dac
        ; file name SNAPnnnn.BIN from count
        mov ax, [count]
        mov di, fname + 7
        mov cx, 4
        mov bx, 10
.digit:
        xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .digit
        ; create
        mov ah, 3ch
        xor cx, cx
        mov dx, fname
        int 21h
        jc .done
        mov bx, ax                  ; handle
        mov ah, 40h
        mov cx, header_end - header
        mov dx, header
        int 21h
        cmp byte [h_mode], 0dh
        je .ega
        cmp byte [h_mode], 13h
        je .mcga
        cmp byte [h_mode], 3
        jbe .text
        jmp .close
.text:
        push ds
        mov ax, 0b800h
        mov ds, ax
        xor dx, dx
        mov cx, 4000
        mov ah, 40h
        int 21h
        pop ds
        jmp .close
.ega:
        mov dx, 3ceh
        in al, dx
        mov [gc_index], al
        mov al, 4
        out dx, al
        inc dx
        in al, dx
        mov [gc_map], al
        dec dx
        mov al, 5
        out dx, al
        inc dx
        in al, dx
        mov [gc_mode], al
        and al, 0f7h                ; read mode 0
        out dx, al
        xor cl, cl
.plane:
        mov dx, 3ceh
        mov al, 4
        out dx, al
        inc dx
        mov al, cl
        out dx, al
        push cx
        push ds
        mov ax, 0a000h
        mov ds, ax
        mov dx, [cs:h_start]
        mov cx, 8000
        mov ah, 40h
        int 21h
        pop ds
        pop cx
        inc cl
        cmp cl, 4
        jb .plane
        mov dx, 3ceh
        mov al, 5
        out dx, al
        inc dx
        mov al, [gc_mode]
        out dx, al
        dec dx
        mov al, 4
        out dx, al
        inc dx
        mov al, [gc_map]
        out dx, al
        dec dx
        mov al, [gc_index]
        out dx, al
        jmp .close
.mcga:
        push ds
        mov ax, 0a000h
        mov ds, ax
        xor dx, dx
        mov cx, 64000
        mov ah, 40h
        int 21h
        pop ds
.close:
        mov ah, 3eh
        int 21h
        inc word [count]
.done:
        ret

gc_index    db 0
gc_map      db 0
gc_mode     db 0

            times 512 db 0
stack_top:

resident_end:

install:
        ; optional interval argument (decimal) in the command tail
        mov si, 81h
        xor ax, ax
        xor cx, cx
.parse:
        mov cl, [si]
        inc si
        cmp cl, ' '
        je .parse
        cmp cl, '0'
        jb .parsed
        cmp cl, '9'
        ja .parsed
        mov bx, 10
        mul bx
        sub cl, '0'
        add ax, cx
        jmp .parse
.parsed:
        or ax, ax
        jz .keep
        mov [interval], ax
.keep:
        mov ah, 34h                 ; InDOS flag address
        int 21h
        mov [indos], bx
        mov [indos+2], es
        mov ax, 3508h
        int 21h
        mov [old8], bx
        mov [old8+2], es
        mov ax, 2508h
        mov dx, timer
        int 21h
        mov ah, 9
        mov dx, msg
        int 21h
        mov dx, resident_end + 15
        mov cl, 4
        shr dx, cl
        mov ax, 3100h
        int 21h

msg     db 'SNAP installed', 13, 10, '$'
