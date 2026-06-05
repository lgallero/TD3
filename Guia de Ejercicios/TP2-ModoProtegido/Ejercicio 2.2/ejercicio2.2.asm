;*******************************************************
; Ejercicio 2.2 - Manejo de Interrupciones en Modo Protegido
; - Entra a modo protegido
; - Configura IDT con handler de teclado (INT 9)
; - Por cada tecla presionada incrementa un contador
; - Muestra el contador en pantalla
;*******************************************************

use16
org 8000h

jmp inicio

;--- Variables ---
gdtr     resb 6
idtr     resb 6
contador db   0

;--- GDT ---
gdt:
  resb 8              ; descriptor nulo

dataSel0 equ $-gdt    ; selector = 0x08
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10010010b
  db 0x00
  db 0x00

videoSel0 equ $-gdt   ; selector = 0x10
  dw 0x1000
  dw 0x8000
  db 0x0b
  db 10010010b
  db 0x00
  db 0x00

codeSel0 equ $-gdt    ; selector = 0x18
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10011010b        ; código ejecutable
  db 0x00
  db 0x00

gdtSize equ $-gdt

;--- IDT ---
idt:
  resb 8*9            ; entradas 0 a 8 vacías (INT 0 a INT 8)

  ; entrada INT 9 — teclado
  dw irq1Handler      ; offset del handler
  dw codeSel0         ; selector de código
  db 0
  db 0x86             ; atributos: puerta de interrupción presente
  dw 0x00

idtSize equ $-idt

;--- Código ---
inicio:
  cli

  ; cargo GDTR
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax
  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax
  lgdt [gdtr]

  ; cargo IDTR
  mov ax, idtSize - 1
  mov [idtr + 0], ax
  xor eax, eax
  mov eax, idt
  mov [idtr + 2], eax
  lidt [idtr]

  ; entro a modo protegido
  mov eax, cr0
  or  al, 1
  mov cr0, eax
  jmp short $+2

  ; cargo CS con selector de código
  jmp codeSel0:modo_protegido

modo_protegido:
  ; cargo registros de segmento
  mov ax, dataSel0
  mov ds, ax
  mov ss, ax
  mov es, ax

  ; configuro stack
  mov eax, fin + 0x100
  mov esp, eax

  ; enmascaro todo el PIC menos IRQ1 (teclado)
  mov al, 0xfd
  out 0x21, al
  mov al, 0xff
  out 0xa1, al

  ; habilito interrupciones
  sti

  ; loop infinito esperando teclas
  jmp $

;--- Handler del teclado ---
irq1Handler:
  push ax
  push bx
  push dx
  push ds
  push es
  push edi

  cli

  ; EOI al PIC
  mov al, 0x20
  out 0x20, al

  ; leo la tecla
  in al, 0x60

  ; filtro break code
  test al, 80h
  jnz fin_irq1

  ; DS apunta a datos
  mov ax, dataSel0
  mov ds, ax

  ; incremento y reseteo si llega a 100
  inc byte [contador]
  cmp byte [contador], 100
  jne sigue
  mov byte [contador], 0

sigue:
  ; separo decenas y unidades
  mov al, [contador]
  mov ah, 0
  mov bl, 10
  div bl          ; AL = decenas, AH = unidades

  ; ES apunta a video
  mov bx, videoSel0
  mov es, bx

  ; escribo decenas en offset 0
  add al, 0x30
  mov [es:0], al
  mov byte [es:1], 0x07   ; atributo

  ; escribo unidades en offset 2
  add ah, 0x30
  mov [es:2], ah
  mov byte [es:3], 0x07   ; atributo

fin_irq1:
  sti

  pop edi
  pop es
  pop ds
  pop dx
  pop bx
  pop ax

  iret

fin: