.export _screen_setup, _bank_count, _modifiers, _draw_row, _draw_color
.import popa
.importzp ptr1

.segment "CODE"

_screen_setup:
    lda #1
    clc
    jsr $ff5f
    lda #$0f
    jmp $ffd2

_bank_count:
    sec
    jsr $ff99
    ldx #0
    cmp #0
    bne done
    inx
done:
    rts

_modifiers:
    jmp $fec0

; __fastcall__ draw_row(row, buffer)
_draw_row:
    sta ptr1
    stx ptr1+1
    jsr popa
    clc
    adc #$b0
    tax
    stz $9f25
    stz $9f20
    stx $9f21
    lda #$11
    sta $9f22
    ldy #0
copy:
    lda (ptr1),y
    sta $9f23
    iny
    cpy #160
    bne copy
    rts

; __fastcall__ draw_color(row, column, color)
_draw_color:
    pha
    jsr popa
    asl
    inc a
    pha
    jsr popa
    clc
    adc #$b0
    stz $9f25
    sta $9f21
    pla
    sta $9f20
    lda #1
    sta $9f22
    pla
    sta $9f23
    rts

.export _read_block, _write_block

; __fastcall__ block(count, buffer)
; Return -1 if the active channel has no block transfer support.
_read_block:
    sta ptr1
    stx ptr1+1
    jsr popa
    ldx ptr1
    ldy ptr1+1
    clc
    jsr $ff44
    bra block_result

_write_block:
    sta ptr1
    stx ptr1+1
    jsr popa
    ldx ptr1
    ldy ptr1+1
    clc
    jsr $feb1
block_result:
    bcc block_ok
    lda #$ff
    tax
    rts
block_ok:
    txa
    phy
    plx
    rts

.export _ticks
_ticks:
    jmp $ffde
