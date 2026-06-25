;*********************************************************
; Ejercicio 2.5 
; Alumno: Gallero, Lucas
; Legajo: 25632
;*********************************************************

use16
org 8000h

jmp inicio

;*****************************
;         Variables
;*****************************
gdtr      resb 6
idtr      resb 6
contador  db 0

; --- Variables nuevas para el Ejercicio 2.5 ---
prioridad_sup db 5    ; Porcentaje de prioridad superior (50% por defecto)
ranura        db 0    ; Ruleta del scheduler (0 a 9)

dec_sup       dw 0    ; Décimas de segundo acumuladas
dec_inf       dw 0

ms_sup        db 0    ; Contadores internos de milisegundos
ms_inf        db 0
ms_serial     dw 0    ; Contador para disparar la tarea del puerto serie

mensaje_excepcion db 'EXCEPCION DEL PROCESADOR', 0

; Cadenas de texto para la pantalla
msg_sup       db 'MITAD SUPERIOR: ', 0
msg_inf       db 'MITAD INFERIOR: ', 0
msg_prio      db 'F2 / F3 - PRIORIDAD: ', 0
msg_f10       db 'F10 LIMPIA Y HLT', 0

;*****************************
;            TSS
;*****************************
; Agregamos 5 Task State Segments (TSS) de 32 bits (104 bytes cada una)

tssInicial resb 104

tssScheduler:
  dd 0
  dd pila_scheduler + 0FFh ; ESP0
  dw dataSel0, 0           ; SS0
  dd 0                     ; ESP1
  dw 0, 0                  ; SS1
  dd 0                     ; ESP2
  dw 0, 0                  ; SS2
  dd 0                     ; CR3
  dd irq0Scheduler         ; EIP
  dd 202h                  ; EFLAGS (IF=1)
  dd 0, 0, 0, 0            ; EAX, ECX, EDX, EBX
  dd pila_scheduler + 0FFh ; ESP
  dd 0, 0, 0               ; EBP, ESI, EDI
  dw videoSel0, 0          ; ES
  dw codeSel0, 0           ; CS
  dw dataSel0, 0           ; SS
  dw dataSel0, 0           ; DS
  dw dataSel0, 0           ; FS
  dw dataSel0, 0           ; GS
  dw 0, 0                  ; LDT
  dw 0, 104                ; I/O map base deshabilitado para no generar GPF

tssTareaSup:
  dd 0, pila_sup + 0FFh
  dw dataSel0, 0
  dd 0, 0, 0, 0, 0
  dd tareaSuperior
  dd 202h
  dd 0, 0, 0, 0
  dd pila_sup + 0FFh
  dd 0, 0, 0
  dw videoSel0, 0, codeSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0
  dw 0, 0, 0, 104

tssTareaInf:
  dd 0, pila_inf + 0FFh
  dw dataSel0, 0
  dd 0, 0, 0, 0, 0
  dd tareaInferior
  dd 202h
  dd 0, 0, 0, 0
  dd pila_inf + 0FFh
  dd 0, 0, 0
  dw videoSel0, 0, codeSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0
  dw 0, 0, 0, 104

tssSerial:
  dd 0, pila_serial + 0FFh
  dw dataSel0, 0
  dd 0, 0, 0, 0, 0
  dd tareaSerial
  dd 202h
  dd 0, 0, 0, 0
  dd pila_serial + 0FFh
  dd 0, 0, 0
  dw videoSel0, 0, codeSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0, dataSel0, 0
  dw 0, 0, 0, 104

;*****************************
;            GDT
;*****************************
gdt:
  ; Descriptor nulo
  resb 8

; Descriptor de datos, base 0, limite 64 KB, nivel 0
dataSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10010010b
  db 0x00
  db 0x00

; Descriptor de video, base 0xB8000, limite 4 KB, nivel 0
videoSel0 equ $-gdt
  dw 0x1000
  dw 0x8000
  db 0x0b
  db 10010010b
  db 0x00
  db 0x00

; Descriptor de codigo, base 0, limite 64 KB, nivel 0
codeSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10011010b
  db 0x00
  db 0x00

; --- NUEVO: Descriptores de las TSS ---
; Limite: 103 (0x67) bytes. Atributo: 89h (Available 32-bit TSS)
tssInicialSel equ $-gdt
  dw 103
  dw tssInicial
  db 0
  db 89h
  db 0
  db 0

tssSchedulerSel equ $-gdt
  dw 103
  dw tssScheduler
  db 0
  db 89h
  db 0
  db 0

tssTareaSupSel equ $-gdt
  dw 103
  dw tssTareaSup
  db 0
  db 89h
  db 0
  db 0

tssTareaInfSel equ $-gdt
  dw 103
  dw tssTareaInf
  db 0
  db 89h
  db 0
  db 0

tssSerialSel equ $-gdt
  dw 103
  dw tssSerial
  db 0
  db 89h
  db 0
  db 0

gdtSize equ $-gdt

;*****************************
;            IDT
;*****************************
idt:
; Mantuve tu IDT manual exacta para no romper las alineaciones de NASM.

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

; --- NUEVO: INT 20h - IRQ0 timer - USADO COMO TASK GATE ---
; El atributo 85h (10000101b) define que esto es una Puerta de Tarea.
  dw 0
  dw tssSchedulerSel
  db 0
  db 85h
  dw 0

; INT 21h - IRQ1 teclado
  dw irq1Handler
  dw codeSel0
  db 0
  db 10000110b
  dw 0

idtSize equ $-idt

;*****************************
;          Codigo
;*****************************
inicio:
  cli

; ======= REPROGRAMACION PIC MASTER Y SLAVE ==============
; --------- PIC Master ---------
  mov al, 11h
  out 20h, al
  mov al, 20h ;vector base
  out 21h, al
  mov al, 04h
  out 21h, al
  mov al, 01h
  out 21h, al
  mov al, 0FFh
  out 21h, al

; ---------  PIC Slave --------- 
  mov al, 11h
  out 0A0h, al
  mov al, 28h  ;vector base
  out 0A1h, al
  mov al, 02h
  out 0A1h, al
  mov al, 01h
  out 0A1h, al
; =======================================================

; --- NUEVO: Inicializar Puerto Serie COM1 a 9600 baud ---
  mov dx, 3fbh    
  mov al, 80h     
  out dx, al
  mov dx, 3f8h    
  mov al, 0ch     ; Divisor para 9600
  out dx, al
  mov dx, 3f9h    
  mov al, 00h
  out dx, al
  mov dx, 3fbh    
  mov al, 03h     ; 8N1
  out dx, al
; =======================================================

; ====================== GDTR ===========================
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax
  xor eax, eax
  mov ax, gdt
  mov [gdtr + 2], eax
  lgdt [gdtr]

; ====================== IDTR ===========================
  mov ax, idtSize - 1
  mov [idtr + 0], ax
  xor eax, eax
  mov eax, idt
  mov [idtr + 2], eax
  lidt [idtr]

; =============== Activo MODO PROTEGIDO  ================
  mov eax, cr0
  or al, 1
  mov cr0, eax
  jmp short $+2
  jmp codeSel0:modo_protegido

modo_protegido:
; --- DS para datos --- 
  mov ax, dataSel0
  mov ds, ax

; ---  ES para video --- 
  mov ax, videoSel0
  mov es, ax

; ---  SS para pila --- 
  mov ax, dataSel0
  mov ss, ax

; ---  inicializo pila --- 
  mov eax, fin + 100h
  mov esp, eax

; --- NUEVO: Cargar la TSS inicial del procesador ---
  mov ax, tssInicialSel
  ltr ax

; --- NUEVO: Reprogramar el Timer a 1 mseg exacto ---
  mov al, 36h
  out 43h, al
  mov ax, 04A9h  ; 1193 en Hexadecimal
  out 40h, al
  mov al, ah
  out 40h, al

; --- Cambio de color de pantalla --- 
  call pintar_video_inverso
  call imprimir_titulos_25
  call mostrar_prioridades

; === PIC MASTER: habilito IRQ0 (Timer) e IRQ1 (Teclado) [1111 1100 b] ===
  mov al, 0FCh 
  out 21h, al

; === PIC SLAVE: enmascaro todo el PIC Slave ===
  mov al, 0FFh
  out 0A1h, al

; --- habilito interrupciones ---
  sti

; --- espero interrupciones ---
esperar:
  jmp esperar


;*********************************************************
;        TAREA Scheduler llamada por IRQ0
;*********************************************************
irq0Scheduler:
  cli
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

; EOI al PIC Master
  mov al, 20h
  out 20h, al

; Chequeo de tiempo para el puerto serie (1000 ticks = 1 seg)
  inc word [ms_serial]
  cmp word [ms_serial], 1000
  jb scheduler_pantalla
  
  mov word [ms_serial], 0     
  mov al, 89h  ; Limpia flag de "ocupado" del TSS
  mov [gdt + tssSerialSel + 5], al 
  jmp tssSerialSel:0             
  jmp irq0Scheduler              

scheduler_pantalla:
  mov al, [ranura]
  cmp al, [prioridad_sup]
  jb ejecutar_sup

ejecutar_inf:
  call avanzar_ranura
  inc byte [ms_inf]
  cmp byte [ms_inf], 100
  jb .fin_inf
  mov byte [ms_inf], 0
  inc word [dec_inf]
.fin_inf:
  mov al, 89h
  mov [gdt + tssTareaInfSel + 5], al
  jmp tssTareaInfSel:0
  jmp irq0Scheduler              

ejecutar_sup:
  call avanzar_ranura
  inc byte [ms_sup]
  cmp byte [ms_sup], 100
  jb .fin_sup
  mov byte [ms_sup], 0
  inc word [dec_sup]
.fin_sup:
  mov al, 89h
  mov [gdt + tssTareaSupSel + 5], al
  jmp tssTareaSupSel:0
  jmp irq0Scheduler              

avanzar_ranura:
  inc byte [ranura]
  cmp byte [ranura], 10
  jb .fin
  mov byte [ranura], 0
.fin:
  ret

;*********************************************************
;              TAREAS DE PANTALLA Y SERIE
;*********************************************************
tareaSuperior:
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax
.bucle:
  mov ax, [dec_sup]
  mov edi, ((5*80)+35)*2
  call imprimir_numero_5
  jmp .bucle

tareaInferior:
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax
.bucle:
  mov ax, [dec_inf]
  mov edi, ((15*80)+35)*2
  call imprimir_numero_5
  jmp .bucle

tareaSerial:
  mov ax, dataSel0
  mov ds, ax
.bucle:
  cli
  mov ax, [dec_sup]
  call enviar_numero_serie
  mov al, ' '
  call enviar_char_serie

  mov ax, [dec_inf]
  call enviar_numero_serie
  mov al, 13
  call enviar_char_serie
  mov al, 10
  call enviar_char_serie
  sti
  jmp .bucle 

enviar_numero_serie:
  push ax
  push bx
  push cx
  push dx
  mov bx, 10
  mov cx, 0
.div_loop:
  xor dx, dx
  div bx
  push dx
  inc cx
  cmp ax, 0
  jne .div_loop
.print_loop:
  pop dx
  add dl, '0'
  mov al, dl
  call enviar_char_serie
  loop .print_loop
  pop dx
  pop cx
  pop bx
  pop ax
  ret

enviar_char_serie:
  push dx
  push ax
.wait_tx:
  mov dx, 3fdh       
  in al, dx
  test al, 20h       
  jz .wait_tx
  pop ax
  mov dx, 3f8h       
  out dx, al
  pop dx
  ret

;*********************************************************
;                       FUNCIONES
;*********************************************************

; --------------------------------------
;|   Pintar pantalla en video inverso   |
; --------------------------------------
pintar_video_inverso:
  push ax
  push cx
  push edi

  mov cx, 80*25
  xor edi, edi      ; Empezar desde el offset 0 de la memoria de video
  mov ax, 7020h     ; AH = 70h (Atributo inverso), AL = 20h (Carácter de espacio ' ')

bucle_pantalla:
  mov [es:edi], ax  ; Escribo el espacio y el color al mismo tiempo (2 bytes)
  add edi, 2        ; Avanzo a la siguiente celda de la pantalla
  loop bucle_pantalla

  pop edi
  pop cx
  pop ax

  ret

; -----------------------------------------
;|   Handler de teclado - INT 21h - IRQ1   |
; -----------------------------------------
irq1Handler:
  cli

; leo scan code
  in al, 60h

; si es break code, no cuento
  test al, 80h
  jnz fin_irq1

; --- MANTENIDO DE TU 2.4: tecla J dispara excepcion ---
  cmp al, 24h
  je generar_excepcion

; --- NUEVO 2.5: Teclas F2, F3, F10 ---
  cmp al, 3ch ; F2
  je tecla_f2
  cmp al, 3dh ; F3
  je tecla_f3
  cmp al, 44h ; F10
  je tecla_f10

; cualquier otra tecla incrementa contador (Original 2.4)
  inc byte [contador]
  call mostrar_contador
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
  call limpiar_pantalla_total
  mov al, 20h
  out 20h, al
  cli
  hlt
  jmp $

; -----------------------------------------
;|               EXCEPCION                 | -> Mantenido del 2.4
; -----------------------------------------
generar_excepcion:
; Genero una excepcion de operacion invalida.
  db 0Fh, 0Bh       ; UD2

; -----------------------------------------
;|         FIN DE INTERRUPCION             | -> Mantenido del 2.4
; -----------------------------------------
fin_irq1:
; EOI al PIC Master
  mov al, 20h
  out 20h, al
  sti
  iret

; -----------------------------------------
;|           Mostrar contador              | -> Mantenido exacto del 2.4
; -----------------------------------------
mostrar_contador:
  push ax
  push bx
  push edi
  mov al, [contador]
  xor ah, ah
  mov bl, 10
  div bl
  add al, '0'
  add ah, '0'
  mov bl, al
  mov bh, ah
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

; -----------------------------------------
;| Rutinas auxiliares para el Ejercicio 2.5|
; -----------------------------------------
imprimir_titulos_25:
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
  
  ; Línea divisoria al medio
  mov cx, 80
  mov edi, (10*80)*2
.linea:
  mov al, '-'
  mov [es:edi], al
  inc edi
  mov al, 70h
  mov [es:edi], al
  inc edi
  loop .linea
  pop edi
  pop esi
  pop cx
  pop bx
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

imprimir_cadena:
  push ax
  push esi
  push edi
.loop:
  mov al, [esi]
  cmp al, 0
  je .fin
  mov [es:edi], al
  inc edi
  mov al, bl
  mov [es:edi], al
  inc edi
  inc esi
  jmp .loop
.fin:
  pop edi
  pop esi
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

limpiar_pantalla_total:
  push ax
  push cx
  push edi
  mov ax, 0720h
  mov cx, 80*25
  xor edi, edi
.loop:
  mov [es:edi], ax
  add edi, 2
  loop .loop
  pop edi
  pop cx
  pop ax
  ret


; -----------------------------------------
;|        Handlers de excepciones          | -> Mantenido exacto del 2.4
; -----------------------------------------
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


; -----------------------------------------
;|    Imprimir mensaje de excepcion        | -> Mantenido exacto del 2.4
; -----------------------------------------
imprimir_excepcion:
  push ax
  push bx
  push esi
  push edi
  mov edi, ((10*80)+20)*2
  mov esi, mensaje_excepcion
imprimir_loop:
  mov al, [esi]
  cmp al, 0
  je fin_imprimir
  mov [es:edi], al
  inc edi
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

;*****************************
;         Pilas (NUEVAS)
;*****************************
pila_scheduler: resb 256
pila_sup:       resb 256
pila_inf:       resb 256
pila_serial:    resb 256

fin: