;*******************************************************
; TECNICAS DIGITALES III
; EJERCICIO 2.5
; ALUMNO: GALLERO LUCAS
; LEGAJO: 25632
;*******************************************************
; Codigo 16 bits
use16

; Offset en 0x8000
org 8000h

; Voy a inicio
jmp inicio

;***********************************
;       Variables
;***********************************
gdtr    resb 6
idtr    resb 6

tecla   db 0

prioridad_sup db 5       ; 5 = 50% superior / 50% inferior
ranura        db 0       ; ranura del scheduler: 0 a 9, lo hago para recorrer un vector en 10 posiciones

contador_sup  dw 0
contador_inf  dw 0

ms_sup        db 0
ms_inf        db 0
ms_serial     dw 0

mensaje_excepcion db 'EXCEPCION DEL PROCESADOR', 0
msg_sup           db 'MITAD SUPERIOR: ', 0
msg_inf           db 'MITAD INFERIOR: ', 0
msg_prio          db 'F2 / F3 - PRIORIDAD: ', 0
msg_f10           db 'F10 LIMPIA Y HLT', 0

;***********************************
;       TSS
;***********************************
; TSS de la tarea inicial
tssInicial resb 68h

; TSS de la tarea scheduler llamada por IRQ0
tssScheduler dd 0
        dd fin + 0x100
        dw dataSel0
        dw 0
        dd 0        ; esp1
        dd 0        ; ss1
        dd 0        ; esp2
        dd 0        ; ss2
        dd 0        ; cr3
        dd irq0Scheduler ; eip
        dd 202h     ; eflags
        dd 0        ; eax
        dd 0        ; ecx
        dd 0        ; edx
        dd 0        ; ebx
        dd fin + 0x100 ; esp
        dd 0        ; ebp
        dd 0        ; esi
        dd 0        ; edi
        dw videoSel0 ; es
        dw 0
        dw codeSel0 ; cs
        dw 0
        dw dataSel0 ; ss
        dw 0
        dw dataSel0 ; ds
        dw 0
        dd 0        ; fs
        dd 0        ; gs
        dd 0
        dd 0

; TSS de la tarea de mitad superior
tssSup dd 0
        dd fin + 0x200
        dw dataSel0
        dw 0
        dd 0        ; esp1
        dd 0        ; ss1
        dd 0        ; esp2
        dd 0        ; ss2
        dd 0        ; cr3
        dd tareaSuperior ; eip
        dd 202h     ; eflags
        dd 0        ; eax
        dd 0        ; ecx
        dd 0        ; edx
        dd 0        ; ebx
        dd fin + 0x200 ; esp
        dd 0        ; ebp
        dd 0        ; esi
        dd 0        ; edi
        dw videoSel0 ; es
        dw 0
        dw codeSel0 ; cs
        dw 0
        dw dataSel0 ; ss
        dw 0
        dw dataSel0 ; ds
        dw 0
        dd 0        ; fs
        dd 0        ; gs
        dd 0
        dd 0

; TSS de la tarea de mitad inferior
tssInf dd 0
        dd fin + 0x300
        dw dataSel0
        dw 0
        dd 0        ; esp1
        dd 0        ; ss1
        dd 0        ; esp2
        dd 0        ; ss2
        dd 0        ; cr3
        dd tareaInferior ; eip
        dd 202h     ; eflags
        dd 0        ; eax
        dd 0        ; ecx
        dd 0        ; edx
        dd 0        ; ebx
        dd fin + 0x300 ; esp
        dd 0        ; ebp
        dd 0        ; esi
        dd 0        ; edi
        dw videoSel0 ; es
        dw 0
        dw codeSel0 ; cs
        dw 0
        dw dataSel0 ; ss
        dw 0
        dw dataSel0 ; ds
        dw 0
        dd 0        ; fs
        dd 0        ; gs
        dd 0
        dd 0

; TSS de la tarea de puerto serie
tssSerial dd 0
        dd fin + 0x400
        dw dataSel0
        dw 0
        dd 0        ; esp1
        dd 0        ; ss1
        dd 0        ; esp2
        dd 0        ; ss2
        dd 0        ; cr3
        dd tareaSerial ; eip
        dd 202h     ; eflags
        dd 0        ; eax
        dd 0        ; ecx
        dd 0        ; edx
        dd 0        ; ebx
        dd fin + 0x400 ; esp
        dd 0        ; ebp
        dd 0        ; esi
        dd 0        ; edi
        dw videoSel0 ; es
        dw 0
        dw codeSel0 ; cs
        dw 0
        dw dataSel0 ; ss
        dw 0
        dw dataSel0 ; ds
        dw 0
        dd 0        ; fs
        dd 0        ; gs
        dd 0
        dd 0

;***********************************
;       GDT
;***********************************
gdt:
  ; Descriptor nulo
  resb 8

; Descriptor de datos (Offset 0, size 64k, nivel 0)
dataSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10010010b
  db 0x00
  db 0x00

; Descriptor de video (Offset 0xb8000, size 4k, nivel 0)
videoSel0 equ $-gdt
  dw 0x1000
  dw 0x8000
  db 0x0b
  db 10010010b
  db 0x00
  db 0x00

; Descriptor de codigo (Offset 0, size 64k, nivel 0)
codeSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10011010b
  db 0x00
  db 0x00

; Descriptor de TSS inicial
tssInicialSel equ $-gdt
  dw 67h
  dw tssInicial
  db 0
  db 10001001b
  db 0
  db 0

; Descriptor de TSS scheduler
tssSchedulerSel equ $-gdt
  dw 67h
  dw tssScheduler
  db 0
  db 10001001b
  db 0
  db 0

; Descriptor de TSS superior
tssSupSel equ $-gdt
  dw 67h
  dw tssSup
  db 0
  db 10001001b
  db 0
  db 0

; Descriptor de TSS inferior
tssInfSel equ $-gdt
  dw 67h
  dw tssInf
  db 0
  db 10001001b
  db 0
  db 0

; Descriptor de TSS puerto serie
tssSerialSel equ $-gdt
  dw 67h
  dw tssSerial
  db 0
  db 10001001b
  db 0
  db 0

; Tamano de la gdt
gdtSize equ $-gdt

;***********************************
;       IDT
;***********************************
idt:
; Excepciones del procesador, INT 00h a INT 1Fh
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

; INT 20h - IRQ0 Timer tick - Task gate
  dw 0
  dw tssSchedulerSel
  db 0
  db 10000101b
  dw 0

; INT 21h - IRQ1 Teclado
  dw irq1Handler
  dw codeSel0
  db 0
  db 10000110b
  dw 0

idtSize equ $-idt

;***********************************
;       Codigo
;***********************************
inicio:
; Deshabilito interrupciones
  cli

;***********************************
; Reprogramo PIC
;***********************************
; Master
  mov al, 11h
  out 20h, al
  mov al, 20h
  out 21h, al
  mov al, 04h
  out 21h, al
  mov al, 01h
  out 21h, al

; Slave
  mov al, 11h
  out 0a0h, al
  mov al, 28h
  out 0a1h, al
  mov al, 02h
  out 0a1h, al
  mov al, 01h
  out 0a1h, al

; Enmascaro todo
  mov al, 0ffh
  out 21h, al
  mov al, 0ffh
  out 0a1h, al

;***********************************
; Inicializo COM1
;***********************************
  mov dx, 3fbh
  mov al, 80h
  out dx, al

  mov dx, 3f8h
  mov al, 0ch
  out dx, al

  mov dx, 3f9h
  mov al, 00h
  out dx, al

  mov dx, 3fbh
  mov al, 03h
  out dx, al

;***********************************
; Cargo GDTR
;***********************************
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax

  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax

;***********************************
; Cargo IDTR
;***********************************
  mov ax, idtSize - 1
  mov [idtr + 0], ax

  xor eax, eax
  mov eax, idt
  mov [idtr + 2], eax

; Cargo gdtr e idtr
  lgdt [gdtr]
  lidt [idtr]

; Paso a modo protegido
  mov eax, cr0
  or al, 1
  mov cr0, eax
  jmp short $+2

; Cargo cs con el selector de codigo
  jmp codeSel0:modo_protegido

modo_protegido:
; Cargo ds, es y ss
  mov ax, dataSel0
  mov ds, ax
  mov ss, ax

  mov ax, videoSel0
  mov es, ax

; Inicializo pila
  mov eax, fin + 0x500
  mov esp, eax

; Cargo TSS inicial
  mov ax, tssInicialSel
  ltr ax

; Programo timer cada 1 ms
  mov al, 36h
  out 43h, al
  mov ax, 04a9h
  out 40h, al
  mov al, ah
  out 40h, al

; Inicializo pantalla
  call limpiar_pantalla
  call imprimir_titulos
  call mostrar_prioridades

; Habilito IRQ0 e IRQ1
  mov al, 0fch ; 1111 1100
  out 21h, al
  mov al, 0ffh
  out 0a1h, al

; Habilito interrupciones
  sti

jmp $

;-------------------------------------------------------------
; Tarea asociada a la IRQ0
;-------------------------------------------------------------
irq0Scheduler:
  cli

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

; Bajo flag de interrupcion
  mov al, 20h
  out 20h, al

; Cada 1000 ticks llamo a la tarea serie
  inc word [ms_serial]
  cmp word [ms_serial], 1000
  jb scheduler_pantalla

  mov word [ms_serial], 0
  mov al, 89h
  mov [gdt + tssSerialSel + 5], al
  sti
  jmp tssSerialSel:0
  jmp irq0Scheduler

scheduler_pantalla:
  mov al, [ranura]        ; 
  cmp al, [prioridad_sup]
  jb ejecutar_sup

;-------------------------------------------------------------
; Ejecuto mitad inferior
;-------------------------------------------------------------
ejecutar_inf:
  call avanzar_ranura

  inc byte [ms_inf]
  cmp byte [ms_inf], 100
  jb fin_inf
  mov byte [ms_inf], 0
  inc word [contador_inf]

fin_inf:
  mov al, 89h
  mov [gdt + tssInfSel + 5], al
  sti
  jmp tssInfSel:0
  jmp irq0Scheduler

;-------------------------------------------------------------
; Ejecuto mitad superior
;-------------------------------------------------------------
ejecutar_sup:
  call avanzar_ranura

  inc byte [ms_sup]
  cmp byte [ms_sup], 100
  jb fin_sup
  mov byte [ms_sup], 0
  inc word [contador_sup]

fin_sup:
  mov al, 89h
  mov [gdt + tssSupSel + 5], al
  sti
  jmp tssSupSel:0
  jmp irq0Scheduler

;-------------------------------------------------------------
avanzar_ranura:
  inc byte [ranura]
  cmp byte [ranura], 10
  jb fin_ranura
  mov byte [ranura], 0
fin_ranura:
  ret

;-------------------------------------------------------------
; Tarea mitad superior
;-------------------------------------------------------------
tareaSuperior:
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

bucle_sup:
  mov ax, [contador_sup]
  mov edi, ((5*80)+35)*2
  call imprimir_numero_5
  jmp bucle_sup

;-------------------------------------------------------------
; Tarea mitad inferior
;-------------------------------------------------------------
tareaInferior:
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

bucle_inf:
  mov ax, [contador_inf]
  mov edi, ((15*80)+35)*2
  call imprimir_numero_5
  jmp bucle_inf

;-------------------------------------------------------------
; Tarea puerto serie
;-------------------------------------------------------------
tareaSerial:
  mov ax, dataSel0
  mov ds, ax

bucle_serial:
  cli

  mov ax, [contador_sup]
  call enviar_numero_serie

  mov al, ' '
  call enviar_caracter_serie

  mov ax, [contador_inf]
  call enviar_numero_serie

  mov al, 13
  call enviar_caracter_serie
  mov al, 10
  call enviar_caracter_serie

  sti
  jmp bucle_serial

;-------------------------------------------------------------
; Handler de teclado - IRQ1
;-------------------------------------------------------------
irq1Handler:
  cli

; Leo scancode
  in al, 60h

; Si es break code, no hago nada
  test al, 80h
  jnz fin_irq1

; Tecla J - genero excepcion
  cmp al, 24h
  je generar_excepcion

; F2 - aumenta prioridad superior
  cmp al, 3ch
  je tecla_f2

; F3 - disminuye prioridad superior
  cmp al, 3dh
  je tecla_f3

; F10 - limpia pantalla y detiene CPU
  cmp al, 44h
  je tecla_f10

; Cualquier otra tecla incrementa contador
  inc byte [tecla]
  call mostrar_teclas
  jmp fin_irq1

tecla_f2:
  cmp byte [prioridad_sup], 10
  jae fin_irq1
  inc byte [prioridad_sup]
  call mostrar_prioridades
  jmp fin_irq1

tecla_f3:
  cmp byte [prioridad_sup], 0
  jbe fin_irq1
  dec byte [prioridad_sup]
  call mostrar_prioridades
  jmp fin_irq1

tecla_f10:
  call limpiar_pantalla
  mov al, 20h
  out 20h, al
  cli
  hlt
  jmp $

generar_excepcion:
; Genero una excepcion de operacion invalida
  db 0fh, 0bh

fin_irq1:
  mov al, 20h
  out 20h, al
  sti
  iret

;-------------------------------------------------------------
; Funciones de pantalla
;-------------------------------------------------------------
limpiar_pantalla:
  push ax
  push cx
  push edi

  mov ax, 7020h
  mov cx, 80*25
  xor edi, edi

bucle_limpiar:
  mov [es:edi], ax
  add edi, 2
  loop bucle_limpiar

  pop edi
  pop cx
  pop ax
  ret

imprimir_titulos:
  push ax
  push bx
  push cx
  push esi
  push edi

  mov bl, 70h

  mov esi, msg_sup
  mov edi, ((5*80)+10)*2
  call imprimir_cadena

  mov esi, msg_inf
  mov edi, ((15*80)+10)*2
  call imprimir_cadena

  mov esi, msg_prio
  mov edi, ((22*80)+10)*2
  call imprimir_cadena

  mov esi, msg_f10
  mov edi, ((24*80)+60)*2
  call imprimir_cadena

  mov cx, 80
  mov edi, (10*80)*2
linea:
  mov al, '-'
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi
  loop linea

  pop edi
  pop esi
  pop cx
  pop bx
  pop ax
  ret

imprimir_cadena:
  push ax
  push esi
  push edi

bucle_cadena:
  mov al, [esi]
  cmp al, 0
  je fin_cadena
  mov [es:edi], al
  inc edi
  mov al, bl
  mov [es:edi], al
  inc edi
  inc esi
  jmp bucle_cadena

fin_cadena:
  pop edi
  pop esi
  pop ax
  ret

mostrar_prioridades:
  push ax
  push bx
  push edi

  mov al, [prioridad_sup]
  mov bl, 10
  mul bl
  mov edi, ((22*80)+35)*2
  call imprimir_numero_3

  mov al, '/'
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi

  mov al, 10
  sub al, [prioridad_sup]
  mov bl, 10
  mul bl
  call imprimir_numero_3

  pop edi
  pop bx
  pop ax
  ret

mostrar_teclas:
  push ax
  push bx
  push edi

  mov al, [tecla]
  xor ah, ah
  mov bl, 10
  div bl

  add al, '0'
  add ah, '0'
  mov bl, al
  mov bh, ah

  mov edi, 0

  mov al, bl
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi

  mov al, bh
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al

  pop edi
  pop bx
  pop ax
  ret

imprimir_numero_5:
  push ax
  push bx

  mov bx, 10000
  call imprimir_digito
  mov bx, 1000
  call imprimir_digito
  mov bx, 100
  call imprimir_digito
  mov bx, 10
  call imprimir_digito
  mov bx, 1
  call imprimir_digito

  pop bx
  pop ax
  ret

imprimir_numero_3:
  push ax
  push bx

  mov bx, 100
  call imprimir_digito
  mov bx, 10
  call imprimir_digito
  mov bx, 1
  call imprimir_digito

  mov al, '%'
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi

  pop bx
  pop ax
  ret

imprimir_digito:
  push cx
  push dx

  xor dx, dx
  div bx
  mov cx, dx

  add al, '0'
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi

  mov ax, cx

  pop dx
  pop cx
  ret

;-------------------------------------------------------------
; Funciones puerto serie
;-------------------------------------------------------------
enviar_numero_serie:
  push ax
  push bx
  push cx
  push dx

  mov bx, 10
  mov cx, 0

serie_dividir:
  xor dx, dx
  div bx
  push dx
  inc cx
  cmp ax, 0
  jne serie_dividir

serie_imprimir:
  pop dx
  add dl, '0'
  mov al, dl
  call enviar_caracter_serie
  loop serie_imprimir

  pop dx
  pop cx
  pop bx
  pop ax
  ret

enviar_caracter_serie:
  push dx
  push ax

serie_esperar:
  mov dx, 3fdh
  in al, dx
  test al, 20h
  jz serie_esperar

  pop ax
  mov dx, 3f8h
  out dx, al

  pop dx
  ret

;-------------------------------------------------------------
; Excepciones
;-------------------------------------------------------------
exc0: call imprimir_excepcion
  jmp $
exc1: call imprimir_excepcion
  jmp $
exc2: call imprimir_excepcion
  jmp $
exc3: call imprimir_excepcion
  jmp $
exc4: call imprimir_excepcion
  jmp $
exc5: call imprimir_excepcion
  jmp $
exc6: call imprimir_excepcion
  jmp $
exc7: call imprimir_excepcion
  jmp $
exc8: call imprimir_excepcion
  jmp $
exc9: call imprimir_excepcion
  jmp $
exc10: call imprimir_excepcion
  jmp $
exc11: call imprimir_excepcion
  jmp $
exc12: call imprimir_excepcion
  jmp $
exc13: call imprimir_excepcion
  jmp $
exc14: call imprimir_excepcion
  jmp $
exc15: call imprimir_excepcion
  jmp $
exc16: call imprimir_excepcion
  jmp $
exc17: call imprimir_excepcion
  jmp $
exc18: call imprimir_excepcion
  jmp $
exc19: call imprimir_excepcion
  jmp $
exc20: call imprimir_excepcion
  jmp $
exc21: call imprimir_excepcion
  jmp $
exc22: call imprimir_excepcion
  jmp $
exc23: call imprimir_excepcion
  jmp $
exc24: call imprimir_excepcion
  jmp $
exc25: call imprimir_excepcion
  jmp $
exc26: call imprimir_excepcion
  jmp $
exc27: call imprimir_excepcion
  jmp $
exc28: call imprimir_excepcion
  jmp $
exc29: call imprimir_excepcion
  jmp $
exc30: call imprimir_excepcion
  jmp $
exc31: call imprimir_excepcion
  jmp $

imprimir_excepcion:
  push ax
  push bx
  push esi
  push edi

  mov bl, 4fh
  mov edi, ((10*80)+20)*2
  mov esi, mensaje_excepcion
  call imprimir_cadena

  pop edi
  pop esi
  pop bx
  pop ax
  ret

fin:

; scheduler -> fin + 0x100
; superior  -> fin + 0x200
; inferior  -> fin + 0x300
; serie     -> fin + 0x400
; inicial   -> fin + 0x500
