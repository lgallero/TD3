;*******************************************************
; Ejercicio 2.3 - Ordenamiento de las Interrupciones
;
; - Reprograma PIC1 y PIC2
; - IRQ0..IRQ15 pasan a INT 20h..INT 2Fh
; - Carga GDT
; - Carga IDT
; - Entra a modo protegido
; - Carga DS para datos
; - Carga ES para video
; - Pone pantalla en video inverso
; - Habilita IRQ1, teclado
; - Cada tecla incrementa contador
; - Muestra contador en pantalla
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
  ; ahora el teclado no esta en INT 9h
  ; porque reprogramamos el PIC:
  ;
  ; IRQ0 -> INT 20h
  ; IRQ1 -> INT 21h
  ;
  ; Entonces dejamos vacias las entradas 0 a 20h
  resb 8*21h

  ; INT 21h - IRQ1 teclado
  dw irq1Handler
  dw codeSel0
  db 0
  db 10000110b
  dw 0x00

idtSize equ $-idt

;***********************************
; Codigo
;***********************************
inicio:
  cli

;-------------------------------------------------------
; Reprogramacion de PIC1 y PIC2
;-------------------------------------------------------
; PIC Master:
;   command port = 20h
;   data port    = 21h
;
; PIC Slave:
;   command port = A0h
;   data port    = A1h
;
; Resultado:
;   PIC1: IRQ0..IRQ7  -> INT 20h..27h
;   PIC2: IRQ8..IRQ15 -> INT 28h..2Fh
;-------------------------------------------------------

; ICW1: inicializacion
  mov al, 11h
  out 20h, al
  out 0A0h, al

; ICW2: base de interrupciones del PIC Master
  mov al, 20h
  out 21h, al

; ICW2: base de interrupciones del PIC Slave
  mov al, 28h
  out 0A1h, al

; ICW3: el slave esta conectado en IRQ2 del master
  mov al, 04h
  out 21h, al

; ICW3: el slave se identifica como IRQ2
  mov al, 02h
  out 0A1h, al

; ICW4: modo 8086
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

; cargo CS con selector de codigo
  jmp codeSel0:modo_protegido

modo_protegido:

; cargo DS con datos
  mov ax, dataSel0
  mov ds, ax

; cargo ES con video
  mov ax, videoSel0
  mov es, ax

; cargo SS con datos
  mov ax, dataSel0
  mov ss, ax

; inicializo pila
  mov eax, fin + 100h
  mov esp, eax

; pongo pantalla en video inverso
  call pintar_video_inverso

; habilito solamente IRQ1, teclado
; aunque el teclado ahora entra por INT 21h,
; sigue siendo IRQ1 fisicamente.
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
; Rutina: pintar pantalla en video inverso
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
irq1Handler:
  cli

; leo scan code del teclado
  in al, 60h

; incremento contador
  inc byte [contador]

; muestro contador en pantalla
  call mostrar_contador

; envio EOI al PIC Master
  mov al, 20h
  out 20h, al

  sti
  iret

;***********************************
; Mostrar contador en pantalla
;***********************************
mostrar_contador:

  push ax
  push bx
  push edi

; leo contador desde DS:dataSel0
  mov al, [contador]

; convierto a decimal de 2 digitos
  xor ah, ah
  mov bl, 10
  div bl

; AL = decenas
; AH = unidades
  add al, '0'
  add ah, '0'

  mov bl, al
  mov bh, ah

; escribo contador en pantalla usando ES:videoSel0
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

fin: