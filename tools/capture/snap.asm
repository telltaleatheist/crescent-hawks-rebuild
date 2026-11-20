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
;   In mode 0Dh with the key script, the picture is followed by the memory:
;   'MEMS' (4), the game's DS when it last asked DOS whether a key was
;   waiting (2), then the
;   first 640K of memory (segment 0 on), so tools can read the game's state.
; Usage: SNAP [ticks] [K]
;   ticks: capture every so many ticks (default 18, about one a second; 0
;   turns the timed captures off). At most 9999 files.
;   K: play the key script in KEYS.BIN (see below).
;
; Key script (tools/reference/run.py writes KEYS.BIN): a word count, then
; that many words: 0FFFFh = capture the screen now; 0FFFEh N = let N more
; empty polls go by before the next item (the game idles meanwhile; a
; blocking read skips it); 0FFFCh N = the same count, set at once, even in
; a blocking read, and the script goes on (before a key: the Nth poll after
; it is fed); 0FFFDh W O N b1..bN = a test poke (below);
; otherwise a key as the BIOS gives it (scan code << 8 | character).
;
; A test poke writes N bytes (one per word, low byte) at segment:O in the
; game's memory, where the segment is the word at DS:W of the game's own
; data segment (W = 0FFFFh: DS itself), as the game reaches its data. It
; stands in for play the rebuild cannot do yet (docs/reference.md). It
; writes data only: a segment below the game's first data segment (the
; game's code and the run-time library) or above DS is refused, nothing is
; written, and SNAP writes POKEFAIL.TXT, which run.py reports. It needs the
; game's DS, so it only works where the game asks DOS for keys (the town). Keys are fed only when the
; game is waiting for one, never by the clock, so a run does not depend on
; how fast the machine is:
;   - the game calls int 16h function 0 or 10h (wait for a key) and the
;     BIOS keyboard buffer is empty; or
;   - it asks DOS (int 21h function 0Bh) whether a key is waiting, the
;     buffer is empty, and it has asked POLLS times in a row since it last
;     read a key. One question right after a key (a flush loop) never gets a
;     key; a loop that waits for a key by asking does.
; At such a moment the script's captures are written, then its next key is
; put in the buffer (int 16h function 5). When the script is used up SNAP
; writes SNAPEND.TXT and stops feeding.
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
old16       dd 0
old21       dd 0
POLLS       equ 4
MAX_KEYS    equ 16384
feeding     db 0
blocking    db 0                    ; feed was called from int 16h
polls       dw 0                    ; signed: an idle step makes it negative
arm         dw 0                    ; 0FFFCh's count, for when the key is read
armed       db 0
game_ds     dw 0                    ; DS of the game's last "key waiting?" call
                                    ; (a blocking read may come with another DS)
key_pos     dw 0
key_count   dw 0
end_name    db 'SNAPEND.TXT', 0
fail_name   db 'POKEFAIL.TXT', 0
; DS minus the first data segment, image 36db - 1c6c (docs/original.md).
DATA_BELOW_DS equ 36dbh - 1c6ch
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

; Is the BIOS keyboard buffer empty? ZF set if so. Keeps every register.
buffer_empty:
        push ds
        push ax
        mov ax, 40h
        mov ds, ax
        mov ax, [1ah]
        cmp ax, [1ch]
        pop ax
        pop ds
        ret

int16:
        cmp byte [cs:feeding], 0
        je .chain
        cmp ah, 0
        je .read
        cmp ah, 10h
        jne .chain
.read:
        mov word [cs:polls], 0
        call apply_arm
        call buffer_empty
        jne .chain
        mov byte [cs:blocking], 1
        call feed
        mov byte [cs:blocking], 0
        call apply_arm
.chain:
        jmp far [cs:old16]

; A key is being read: the polls counted from now on start at 0FFFCh's
; count, if one came before the key. Keeps every register and the flags.
apply_arm:
        pushf
        cmp byte [cs:armed], 0
        je .out
        push ax
        mov ax, [cs:arm]
        mov [cs:polls], ax
        mov byte [cs:armed], 0
        pop ax
.out:
        popf
        ret

int21:
        cmp ah, 0bh
        jne .chain
        cmp byte [cs:feeding], 0
        je .chain
        call buffer_empty
        jne .chain
        inc word [cs:polls]
        cmp word [cs:polls], POLLS
        jl .chain
        mov word [cs:polls], 0
        mov [cs:game_ds], ds
        call feed
.chain:
        jmp far [cs:old21]

; The game is waiting for a key: write the script's captures up to its next
; key, then put that key in the buffer. Runs on SNAP's own stack. Keeps
; every register and the flags.
feed:
        pushf
        cmp byte [cs:busy], 0
        jne .busy
        cli
        mov byte [cs:busy], 1
        mov [cs:saved_ss], ss
        mov [cs:saved_sp], sp
        push cs
        pop ss
        mov sp, stack_top
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push bp
        push cs
        pop ds
.next:
        mov bx, [key_pos]
        cmp bx, [key_count]
        jae .finished
        inc word [key_pos]
        shl bx, 1
        mov cx, [keys + bx]
        cmp cx, 0ffffh
        jne .poke
        call capture
        jmp .next
.poke:
        cmp cx, 0fffdh
        jne .arm
        call poke
        jmp .next
.arm:
        cmp cx, 0fffch
        jne .idle
        mov bx, [key_pos]               ; the count follows
        inc word [key_pos]
        shl bx, 1
        mov ax, POLLS
        sub ax, [keys + bx]
        mov [arm], ax
        mov byte [armed], 1
        jmp .next
.idle:
        cmp cx, 0fffeh
        jne .key
        mov bx, [key_pos]               ; the count follows
        inc word [key_pos]
        shl bx, 1
        cmp byte [blocking], 0
        jne .next                       ; nothing idles in a blocking read
        mov ax, POLLS
        sub ax, [keys + bx]
        mov [polls], ax
        jmp .out
.key:
        mov ah, 5
        pushf
        call far [old16]
        jmp .out
.finished:
        mov byte [feeding], 0
        mov ah, 3ch
        xor cx, cx
        mov dx, end_name
        int 21h
        jc .out
        mov bx, ax
        mov ah, 3eh
        int 21h
.out:
        pop bp
        pop es
        pop ds
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        mov ss, [cs:saved_ss]
        mov sp, [cs:saved_sp]
        mov byte [cs:busy], 0
.busy:
        popf
        ret

; A test poke: the item after 0FFFDh is W O N b1..bN (see the top).
; DS is SNAP's here; key_pos is on W.
poke:
        mov bx, [key_pos]
        shl bx, 1
        lea si, [keys + bx]
        mov ax, [si]                    ; W
        mov di, [si + 2]                ; O
        mov cx, [si + 4]                ; N
        mov bx, cx
        add bx, 3
        add [key_pos], bx               ; past the whole item
        add si, 6                       ; the bytes
        mov dx, [game_ds]
        or dx, dx
        jz .refuse                      ; no DS seen yet
        cmp ax, 0ffffh
        je .ds
        push ds
        mov ds, dx
        mov bx, ax
        mov ax, [bx]                    ; the segment, as the game keeps it
        pop ds
        jmp .check
.ds:
        mov ax, dx
.check:
        mov bx, dx
        sub bx, DATA_BELOW_DS           ; the first data segment
        cmp ax, bx
        jb .refuse
        cmp ax, dx
        ja .refuse
        mov es, ax
        jcxz .done
        cld
.copy:
        mov al, [si]
        stosb
        add si, 2
        loop .copy
.done:
        ret
.refuse:
        mov ah, 3ch
        xor cx, cx
        mov dx, fail_name
        int 21h
        jc .done
        mov bx, ax
        mov ah, 3eh
        int 21h
        ret

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
        cmp byte [feeding], 0
        je .close
        mov ax, [game_ds]
        mov [mem_ds], ax
        mov cx, 6
        mov dx, mem_tag
        mov ah, 40h
        int 21h
        xor si, si                  ; segment
.mem:
        push ds
        mov ds, si
        xor dx, dx
        mov cx, 8000h
        mov ah, 40h
        int 21h
        pop ds
        add si, 800h
        cmp si, 0a000h
        jb .mem
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

mem_tag     db 'MEMS'
mem_ds      dw 0
gc_index    db 0
gc_map      db 0
gc_mode     db 0

            times 512 db 0
stack_top:

keys        times MAX_KEYS dw 0

resident_end:

install:
        ; command tail: an optional interval (decimal) and an optional K
        mov si, 81h
        xor ax, ax
        xor cx, cx
        xor di, di                  ; di = 1 once a digit is seen
.parse:
        mov cl, [si]
        inc si
        cmp cl, 13
        je .parsed
        cmp cl, 'K'
        je .flag
        cmp cl, 'k'
        je .flag
        cmp cl, '0'
        jb .parse
        cmp cl, '9'
        ja .parse
        mov di, 1
        mov bx, 10
        mul bx
        sub cl, '0'
        add ax, cx
        jmp .parse
.flag:
        mov byte [want_keys], 1
        jmp .parse
.parsed:
        or di, di
        jz .keep
        mov [interval], ax
.keep:
        cmp byte [want_keys], 0
        je .no_keys
        mov ax, 3d00h               ; open KEYS.BIN
        mov dx, keys_name
        int 21h
        jc .bad_keys
        mov bx, ax
        mov ah, 3fh
        mov cx, 2
        mov dx, key_count
        int 21h
        jc .bad_keys
        mov cx, [key_count]
        cmp cx, MAX_KEYS
        ja .bad_keys
        shl cx, 1
        mov ah, 3fh
        mov dx, keys
        int 21h
        jc .bad_keys
        mov ah, 3eh
        int 21h
        mov ax, 3516h
        int 21h
        mov [old16], bx
        mov [old16+2], es
        mov ax, 2516h
        mov dx, int16
        int 21h
        mov ax, 3521h
        int 21h
        mov [old21], bx
        mov [old21+2], es
        mov ax, 2521h
        mov dx, int21
        int 21h
        mov byte [feeding], 1
.no_keys:
        mov ah, 34h                 ; InDOS flag address
        int 21h
        mov [indos], bx
        mov [indos+2], es
        cmp word [interval], 0
        je .resident
        mov ax, 3508h
        int 21h
        mov [old8], bx
        mov [old8+2], es
        mov ax, 2508h
        mov dx, timer
        int 21h
.resident:
        mov ah, 9
        mov dx, msg
        int 21h
        mov dx, resident_end + 15
        mov cl, 4
        shr dx, cl
        mov ax, 3100h
        int 21h
.bad_keys:
        mov ah, 9
        mov dx, bad_msg
        int 21h
        mov ax, 4c01h
        int 21h

want_keys db 0
keys_name db 'KEYS.BIN', 0
msg     db 'SNAP installed', 13, 10, '$'
bad_msg db 'SNAP: cannot read KEYS.BIN', 13, 10, '$'
