;*******************************************************
; Ejercicio 2.4 - Manejo de Excepciones en Modo Protegido
;
; - Parte del Ejercicio 2.3
; - Reprograma PIC1 y PIC2 a INT 20h..INT 2Fh
; - Carga handlers para excepciones INT 00h..INT 1Fh
; - IRQ1 teclado queda en INT 21h
; - Si se presiona J/j genera una excepcion
; - Cualquier otra tecla incrementa contador
; - Si ocurre excepcion, imprime mensaje y hace jmp $
;*******************************************************

use16
org 8000h

jmp inicio

;***********************************
; Variables
;***********************************
gdtr      resb 6
idtr      resb 6
contador  db 0

mensaje_excepcion db 'EXCEPCION DEL PROCESADOR', 0

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
; IDT
;***********************************
idt:

; INT 00h - Division por cero
  dw exc0
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 01h - Debug
  dw exc1
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 02h - NMI
  dw exc2
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 03h - Breakpoint
  dw exc3
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 04h - Overflow
  dw exc4
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 05h - Bound range
  dw exc5
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 06h - Invalid Opcode
  dw exc6
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 07h - Device not available
  dw exc7
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 08h - Double fault
  dw exc8
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 09h - Coprocessor Segment
  dw exc9
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Ah - Invalid TSS
  dw exc10
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Bh - Segment Not Present
  dw exc11
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Ch - Stack Segment Fault
  dw exc12
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Dh - General Protection Fault
  dw exc13
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Eh - Page Fault
  dw exc14
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 0Fh - Reservado por Intel
  dw exc15
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 10h - x87 Floating Point Error
  dw exc16
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 11h - Alignment Check
  dw exc17
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 12h - Machine Check
  dw exc18
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 13h - SIMD Floating Point
  dw exc19
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 14h - Virtualization Exception
  dw exc20
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 15h - Control Protection
  dw exc21
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 16h - Reservado
  dw exc22
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 17h - Reservado
  dw exc23
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 18h - Reservado
  dw exc24
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 19h - Reservado
  dw exc25
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Ah - Reservado
  dw exc26
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Bh - Reservado
  dw exc27
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Ch - Reservado
  dw exc28
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Dh - Reservado
  dw exc29
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Eh - Security Exception
  dw exc30
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 1Fh - Reservado
  dw exc31
  dw codeSel0
  db 0
  db 10000110b
  dw 0

; INT 20h - IRQ0 timer, no usado
  resb 8

; INT 21h - IRQ1 teclado
  dw irq1Handler
  dw codeSel0
  db 0
  db 10000110b
  dw 0

idtSize equ $-idt


;***********************************
; Codigo
;***********************************
inicio:
  cli

;-------------------------------------------------------
; Reprogramacion de PIC1 y PIC2
;-------------------------------------------------------

; ICW1
  mov al, 11h
  out 20h, al
  out 0A0h, al

; ICW2 - PIC Master empieza en INT 20h
  mov al, 20h
  out 21h, al

; ICW2 - PIC Slave empieza en INT 28h
  mov al, 28h
  out 0A1h, al

; ICW3 - slave conectado en IRQ2 del master
  mov al, 04h
  out 21h, al

; ICW3 - slave identificado como IRQ2
  mov al, 02h
  out 0A1h, al

; ICW4 - modo 8086
  mov al, 01h
  out 21h, al
  out 0A1h, al

;-------------------------------------------------------
; Cargo GDTR
;-------------------------------------------------------
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax

  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax

;-------------------------------------------------------
; Cargo IDTR
;-------------------------------------------------------
  mov ax, idtSize - 1
  mov [idtr + 0], ax

  xor eax, eax
  mov eax, idt
  mov [idtr + 2], eax

  lgdt [gdtr]
  lidt [idtr]

;-------------------------------------------------------
; Paso a modo protegido
;-------------------------------------------------------
  mov eax, cr0
  or al, 1
  mov cr0, eax

  jmp short $+2

; cargo CS
  jmp codeSel0:modo_protegido


modo_protegido:

; DS para datos
  mov ax, dataSel0
  mov ds, ax

; ES para video
  mov ax, videoSel0
  mov es, ax

; SS para pila
  mov ax, dataSel0
  mov ss, ax

; inicializo pila
  mov eax, fin + 100h
  mov esp, eax

; pantalla en video inverso
  call pintar_video_inverso

; habilito solamente IRQ1 teclado
  mov al, 0FDh ; 1111 1101 b
  out 21h, al

; enmascaro todo el PIC Slave
  mov al, 0FFh
  out 0A1h, al

; habilito interrupciones
  sti

; espero interrupciones
  jmp $


;***********************************
; Pintar pantalla en video inverso
;***********************************
pintar_video_inverso:

  push ax
  push cx
  push edi

  mov cx, 80*25
  mov edi, 1
  mov al, 70h

bucle_pantalla:
  mov [es:edi], al
  inc edi
  inc edi
  loop bucle_pantalla

  pop edi
  pop cx
  pop ax

  ret


;***********************************
; Handler de teclado - INT 21h - IRQ1
;***********************************

;------------------------------------
irq1Handler:
  cli

; leo scan code
  in al, 60h

; si es break code, no cuento
  test al, 80h
  jnz fin_irq1

; tecla J tiene scan code 24h
  cmp al, 24h
  je generar_excepcion

; cualquier otra tecla incrementa contador
  inc byte [contador]
  call mostrar_contador
  jmp fin_irq1
;------------------------------------

;------------------------------------
generar_excepcion:

; Genero una excepcion de operacion invalida.
; INT 06h - Invalid Opcode
  db 0Fh, 0Bh       ; UD2
;------------------------------------

;------------------------------------
fin_irq1:

; EOI al PIC Master
  mov al, 20h
  out 20h, al

  sti
  iret
;------------------------------------

;***********************************
; Mostrar contador
;***********************************
mostrar_contador:

  push ax
  push bx
  push edi

; contador esta en DS:dataSel0
  mov al, [contador]

; separo decenas y unidades
  xor ah, ah
  mov bl, 10
  div bl

; AL = decenas
; AH = unidades
  add al, '0'
  add ah, '0'

  mov bl, al
  mov bh, ah

; escribo usando ES:videoSel0
  mov edi, 0

; decenas
  mov al, bl
  mov [es:edi], al
  inc edi

  mov al, 70h
  mov [es:edi], al
  inc edi

; unidades
  mov al, bh
  mov [es:edi], al
  inc edi

  mov al, 70h
  mov [es:edi], al

  pop edi
  pop bx
  pop ax

  ret


;***********************************
; Handlers de excepciones
;***********************************
; Se imprime el mismo mensaje pero se deja generico para poner mensaje para a cada una

exc0:
  call imprimir_excepcion
  jmp $

exc1:
  call imprimir_excepcion
  jmp $

exc2:
  call imprimir_excepcion
  jmp $

exc3:
  call imprimir_excepcion
  jmp $

exc4:
  call imprimir_excepcion
  jmp $

exc5:
  call imprimir_excepcion
  jmp $

exc6:
  call imprimir_excepcion
  jmp $

exc7:
  call imprimir_excepcion
  jmp $

exc8:
  call imprimir_excepcion
  jmp $

exc9:
  call imprimir_excepcion
  jmp $

exc10:
  call imprimir_excepcion
  jmp $

exc11:
  call imprimir_excepcion
  jmp $

exc12:
  call imprimir_excepcion
  jmp $

exc13:
  call imprimir_excepcion
  jmp $

exc14:
  call imprimir_excepcion
  jmp $

exc15:
  call imprimir_excepcion
  jmp $

exc16:
  call imprimir_excepcion
  jmp $

exc17:
  call imprimir_excepcion
  jmp $

exc18:
  call imprimir_excepcion
  jmp $

exc19:
  call imprimir_excepcion
  jmp $

exc20:
  call imprimir_excepcion
  jmp $

exc21:
  call imprimir_excepcion
  jmp $

exc22:
  call imprimir_excepcion
  jmp $

exc23:
  call imprimir_excepcion
  jmp $

exc24:
  call imprimir_excepcion
  jmp $

exc25:
  call imprimir_excepcion
  jmp $

exc26:
  call imprimir_excepcion
  jmp $

exc27:
  call imprimir_excepcion
  jmp $

exc28:
  call imprimir_excepcion
  jmp $

exc29:
  call imprimir_excepcion
  jmp $

exc30:
  call imprimir_excepcion
  jmp $

exc31:
  call imprimir_excepcion
  jmp $


;***********************************
; Imprimir mensaje de excepcion
;***********************************
imprimir_excepcion:

  push ax
  push bx
  push esi
  push edi

; DS ya apunta a dataSel0
; ES ya apunta a videoSel0

; posicion en pantalla:
; fila 10, columna 20
; offset = ((fila * 80) + columna) * 2
  mov edi, ((10*80)+20)*2

  mov esi, mensaje_excepcion

imprimir_loop:

  mov al, [esi]
  cmp al, 0
  je fin_imprimir

; escribo caracter (lettra)
  mov [es:edi], al
  inc edi

; escribo atributo (fondo [0100b] + color [1111b])
  mov al, 4Fh
  mov [es:edi], al
  inc edi

  inc esi
  jmp imprimir_loop

fin_imprimir:

  pop edi
  pop esi
  pop bx
  pop ax

  ret


fin: