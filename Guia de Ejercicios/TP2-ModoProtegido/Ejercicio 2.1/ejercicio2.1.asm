;*******************************************************
; Ejercicio 2.1 - Entrada a Modo Protegido
;
; - Carga GDT
; - Entra a modo protegido
; - Carga DS para datos
; - Carga ES para video
; - Pone la pantalla en video inverso
; - Se frena con jmp $
;*******************************************************

use16
org 8000h

jmp inicio

;***********************************
; Variables
;***********************************
gdtr resb 6

;***********************************
; GDT
;***********************************
gdt:
  ; descriptor nulo
  resb 8

; descriptor de datos, base 0, limite 64 KB, nivel 0
dataSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10010010b
  db 0x00
  db 0x00

; descriptor de video, base 0xB8000, limite 4 KB, nivel 0
videoSel0 equ $-gdt
  dw 0x1000
  dw 0x8000
  db 0x0b
  db 10010010b
  db 0x00
  db 0x00

; descriptor de codigo, base 0, limite 64 KB, nivel 0
codeSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10011010b
  db 0x00
  db 0x00

gdtSize equ $-gdt

;***********************************
; Codigo
;***********************************
inicio:
  cli

;-----------------------------------
; cargo GDTR
;-----------------------------------
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax

  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax

  lgdt [gdtr]

;-----------------------------------
; paso a modo protegido
;-----------------------------------
  mov eax, cr0
  or al, 1
  mov cr0, eax

  jmp short $+2


;-----------------------------------
; cargo CS con selector de codigo
;-----------------------------------
  jmp codeSel0:modo_protegido

modo_protegido:
;-----------------------------------
; cargo DS con datos
;-----------------------------------
  mov ax, dataSel0
  mov ds, ax

;-----------------------------------
; cargo ES con video
;-----------------------------------
  mov ax, videoSel0
  mov es, ax

;-----------------------------------
; pinto pantalla en video inverso
;-----------------------------------
  mov cx, 80*25
  mov edi, 1 ;primer atrubuto 
  mov al, 70h

bucle_pantalla:
  mov [es:edi], al
  inc edi
  inc edi
  loop bucle_pantalla ; por cada loop se resta cx

;-----------------------------------
; freno ejecucion
;-----------------------------------
  jmp $