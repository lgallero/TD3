;*********************************************************
; Ejercicio 2.6 - Scheduler como Rutina de Interrupcion
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

prioridad_sup db 5    
ranura        db 0    

dec_sup       dw 0    
dec_inf       dw 0

ms_sup        db 0    
ms_inf        db 0
ms_serial     dw 0    

; Variable para saber qué tarea está corriendo
; 0 = Inicial, 1 = Superior, 2 = Inferior, 3 = Serial
tarea_actual  db 0    

mensaje_excepcion db 'EXCEPCION DEL PROCESADOR', 0
msg_sup       db 'MITAD SUPERIOR: ', 0
msg_inf       db 'MITAD INFERIOR: ', 0
msg_prio      db 'F2 / F3 - PRIORIDAD: ', 0
msg_f10       db 'F10 LIMPIA Y HLT', 0

;*****************************
;            TSS
;*****************************
tssInicial resb 104

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
  resb 8

dataSel0 equ $-gdt
  dw 0ffffh
  dw 0000h
  db 00h
  db 10010010b
  db 00h
  db 00h

videoSel0 equ $-gdt
  dw 1000h
  dw 8000h
  db 0bh
  db 10010010b
  db 00h
  db 00h

codeSel0 equ $-gdt
  dw 0ffffh
  dw 0000h
  db 00h
  db 10011010b
  db 00h
  db 00h

tssInicialSel equ $-gdt
  dw 103
  dw tssInicial
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
; ¡MACRO SALVADORA! Asegura exactamente 8 bytes por entrada
%macro IDT_EXC 1
  dw %1
  dw codeSel0
  db 0
  db 86h ; 10000110b -> 16-bit Interrupt Gate
  dw 0
%endmacro

idt:
  IDT_EXC exc0
  IDT_EXC exc1
  IDT_EXC exc2
  IDT_EXC exc3
  IDT_EXC exc4
  IDT_EXC exc5
  IDT_EXC exc6
  IDT_EXC exc7
  IDT_EXC exc8
  IDT_EXC exc9
  IDT_EXC exc10
  IDT_EXC exc11
  IDT_EXC exc12
  IDT_EXC exc13
  IDT_EXC exc14
  IDT_EXC exc15
  IDT_EXC exc16
  IDT_EXC exc17
  IDT_EXC exc18
  IDT_EXC exc19
  IDT_EXC exc20
  IDT_EXC exc21
  IDT_EXC exc22
  IDT_EXC exc23
  IDT_EXC exc24
  IDT_EXC exc25
  IDT_EXC exc26
  IDT_EXC exc27
  IDT_EXC exc28
  IDT_EXC exc29
  IDT_EXC exc30
  IDT_EXC exc31

; --- NUEVO 2.6: INT 20h - IRQ0 timer configurado como INTERRUPT GATE (86h) ---
  dw irq0Scheduler
  dw codeSel0
  db 0
  db 86h
  dw 0

; INT 21h - IRQ1 teclado (Interrupt Gate normal)
  dw irq1Handler
  dw codeSel0
  db 0
  db 86h
  dw 0

idtSize equ $-idt

;*****************************
;          Codigo
;*****************************
inicio:
  cli

; --------- PIC Master ---------
  mov al, 11h
  out 20h, al
  mov al, 20h 
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
  mov al, 28h  
  out 0A1h, al
  mov al, 02h
  out 0A1h, al
  mov al, 01h
  out 0A1h, al

; ---- Inicializar COM1 (9600 8N1) ----
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
  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax
  mov ax, dataSel0
  mov ss, ax

  mov eax, fin + 100h
  mov esp, eax

  mov ax, tssInicialSel
  ltr ax

  mov al, 36h
  out 43h, al
  mov ax, 04A9h  
  out 40h, al
  mov al, ah
  out 40h, al

  call limpiar_pantalla_total ; <--- Ahora limpia TODA la pantalla sin dejar basura
  call imprimir_titulos_25
  call mostrar_prioridades

  mov al, 0FCh 
  out 21h, al
  mov al, 0FFh
  out 0A1h, al

  sti

esperar:
  jmp esperar

;*********************************************************
;        TAREA Scheduler (Interrupt Handler)
;*********************************************************
irq0Scheduler:
; Al ser una ISR, guardamos el estado de la tarea interrumpida
  pusha
  push ds
  push es

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

; EOI al PIC Master
  mov al, 20h
  out 20h, al

; Chequeo de tarea Serial (1 seg)
  inc word [ms_serial]
  cmp word [ms_serial], 1000
  jb scheduler_pantalla
  
  mov word [ms_serial], 0     
  cmp byte [tarea_actual], 3
  je fin_scheduler            ; Si ya es la serial, no cambiamos contexto
  
  mov byte [tarea_actual], 3
  mov al, 89h  
  mov [gdt + tssSerialSel + 5], al 
  jmp tssSerialSel:0             
  jmp fin_scheduler              

scheduler_pantalla:
  mov al, [ranura]
  cmp al, [prioridad_sup]
  jb ejecutar_sup

ejecutar_inf:
  call avanzar_ranura
  inc byte [ms_inf]
  cmp byte [ms_inf], 100
  jb .check_cambio
  mov byte [ms_inf], 0
  inc word [dec_inf]
.check_cambio:
  cmp byte [tarea_actual], 2
  je fin_scheduler            
  
  mov byte [tarea_actual], 2
  mov al, 89h
  mov [gdt + tssTareaInfSel + 5], al
  jmp tssTareaInfSel:0
  jmp fin_scheduler              

ejecutar_sup:
  call avanzar_ranura
  inc byte [ms_sup]
  cmp byte [ms_sup], 100
  jb .check_cambio
  mov byte [ms_sup], 0
  inc word [dec_sup]
.check_cambio:
  cmp byte [tarea_actual], 1
  je fin_scheduler            
  
  mov byte [tarea_actual], 1
  mov al, 89h
  mov [gdt + tssTareaSupSel + 5], al
  jmp tssTareaSupSel:0
  jmp fin_scheduler              

fin_scheduler:
; Restauramos el contexto cuando la tarea reanuda su ejecución
  pop es
  pop ds
  popa
  iret

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

; -----------------------------------------
;|   Handler de teclado - INT 21h - IRQ1   |
; -----------------------------------------
irq1Handler:
  cli
  push ax
  push ds
  push es

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  in al, 60h
  test al, 80h
  jnz fin_irq1

  cmp al, 24h
  je generar_excepcion

  cmp al, 3ch ; F2
  je tecla_f2
  cmp al, 3dh ; F3
  je tecla_f3
  cmp al, 44h ; F10
  je tecla_f10

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

generar_excepcion:
  db 0Fh, 0Bh       ; UD2

fin_irq1:
  mov al, 20h
  out 20h, al
  pop es
  pop ds
  pop ax
  sti
  iret

; -----------------------------------------
;|           Mostrar contador              |
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

; -----------------------------------------
;| Rutinas auxiliares                    |
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
  mov ax, 7020h      ; 70h = Fondo gris/texto negro, 20h = Espacio en blanco
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
;|        Handlers de excepciones          |
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
;         Pilas 
;*****************************
pila_sup:       resb 256
pila_inf:       resb 256
pila_serial:    resb 256

fin: