;*******************************************************
; Ejercicio 2.1 - Entrada a Modo Protegido
; Entra a modo protegido, pone pantalla en video
; inverso y frena con jmp $.
;*******************************************************
use16
org 8000h

jmp inicio

;--- Variables ---
gdtr   resb 6

;--- GDT ---
gdt:
  resb 8              ; descriptor nulo (obligatorio) 0-7

dataSel0 equ $-gdt    ; selector = 0x08               8-15
  dw 0xffff           ; límite 64k
  dw 0x0000           ; base = 0
  db 0x00
  db 10010010b        ; P=1, DPL=0, S=1, Datos R/W
  db 0x00
  db 0x00

videoSel0 equ $-gdt   ; selector = 0x10               16-23
  dw 0x1000           ; límite 4k
  dw 0x8000           ; base[15:0]
  db 0x0b             ; base[23:16] → base total = 0xB8000
  db 10010010b        ; P=1, DPL=0, S=1, Datos R/W
  db 0x00
  db 0x00

gdtSize equ $-gdt

;--- Código ---
inicio:
  cli

  mov ax, gdtSize - 1
  mov [gdtr + 0], ax
  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax
  lgdt [gdtr]

  mov eax, cr0
  or  al, 1
  mov cr0, eax
  jmp short $+2           ; flush del pipeline

  mov ax, videoSel0
  mov ds, ax

  mov cx, 80*25
  mov al, 0x70
  mov edi, 1

bucle_pantalla:
  mov [ds:edi], al
  inc edi
  inc edi
  loop bucle_pantalla

  jmp $