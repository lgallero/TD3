;*******************************************************
;Ejercicio 2.7 TDIII
;Ejecucion en dos niveles de privilegio
;*******************************************************
;codigo 16 bits
use16

;offset del codigo en 0x8000
org 0x8000

;voy a inicio
jmp inicio

;***********************************
;	variables
;***********************************
gdtr    resb 6
idtr    resb 6

prioridad_t1     db 5
ranura           db 0

ms_t1            db 0
ms_t2            db 0
tiempo_t1_seg    dw 0
tiempo_t2_seg    dw 0

t1_ini_hora      db 0
t1_ini_min       db 0
t1_ini_seg       db 0
t2_ini_hora      db 0
t2_ini_min       db 0
t2_ini_seg       db 0

ultimo_seg_rtc   db 0xFF
serial_pendiente db 0
inicio_t1_ok     db 0
inicio_t2_ok     db 0
irq1_ds_save     dw 0
irq1_es_save     dw 0

rtc_seg          db 0
rtc_min          db 0
rtc_hora         db 0
rtc_dia          db 0
rtc_mes          db 0
rtc_anio         db 0

msg_t1           db 'TAREA 1 - HORA:', 0
msg_t2           db 'TAREA 2 - FECHA/HORA:', 0
msg_prio         db 'F2/F3 PRIORIDAD T1/T2:', 0
msg_fin          db 'F10 FINALIZA - LIMPIA Y HLT', 0
msg_stop         db 'FIN DEL PROGRAMA', 0
msg_tiempo       db 'UNA TAREA LLEGO A 3 MINUTOS', 0

;***********************************
;DECLARACION DE TSS
;***********************************
; TSS de la tarea inicial
tssInicial resb 0x68

; TSS del scheduler llamada por IRQ0
tssScheduler dd 0
        dd pila_00_Scheduler + 0xFF
        dw dataSel0
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd irq0Scheduler
        dd 0x202
        dd 0
        dd 0
        dd 0
        dd 0
        dd pila_00_Scheduler + 0xFF
        dd 0
        dd 0
        dd 0
        dw videoSel0
        dw 0
        dw codeSel0
        dw 0
        dw dataSel0
        dw 0
        dw dataSel0
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0

; TSS de la tarea 1
tssTarea1 dd 0
        dd pila_00_Tarea_1 + 0xFF
        dw dataSel0
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd tarea1
        dd 0x202
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0xFF
        dd 0
        dd 0
        dd 0
        dw dataSel3
        dw 0
        dw codeSel3
        dw 0
        dw pilaT1Sel3
        dw 0
        dw dataSel3
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0

; TSS de la tarea 2
tssTarea2 dd 0
        dd pila_00_Tarea_2 + 0xFF
        dw dataSel0
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd tarea2
        dd 0x202
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0xFF
        dd 0
        dd 0
        dd 0
        dw dataSel3
        dw 0
        dw codeSel3
        dw 0
        dw pilaT2Sel3
        dw 0
        dw dataSel3
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0

; TSS de la tarea 3
tssTarea3 dd 0
        dd pila_00_Tarea_3 + 0xFF
        dw dataSel0
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0
        dd tarea3
        dd 0x202
        dd 0
        dd 0
        dd 0
        dd 0
        dd 0xFF
        dd 0
        dd 0
        dd 0
        dw dataSel3
        dw 0
        dw codeSel3
        dw 0
        dw pilaT3Sel3
        dw 0
        dw dataSel3
        dw 0
        dd 0
        dd 0
        dd 0
        dd 0

;***********************************
;	           GDT
;***********************************
gdt:

  resb 8

; Descriptor de datos nivel 0
dataSel0 equ $-gdt
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x92
  db 0x00
  db 0x00

; Descriptor de video
videoSel0 equ $-gdt
  dw 0x1000
  dw 0x8000
  db 0x0B
  db 0x92
  db 0x00
  db 0x00

; Descriptor de codigo nivel 0
codeSel0 equ $-gdt
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x9A
  db 0x00
  db 0x00

; Call gates
cgServicioHoraT1 equ $-gdt + 3
  dw servicioHoraT1
  dw codeSel0
  db 0x00
  db 0xE4
  dw 0x0000

cgServicioFechaHoraT2 equ $-gdt + 3
  dw servicioFechaHoraT2
  dw codeSel0
  db 0x00
  db 0xE4
  dw 0x0000

cgServicioSerialHora equ $-gdt + 3
  dw servicioSerialHora
  dw codeSel0
  db 0x00
  db 0xE4
  dw 0x0000

; Descriptor de datos nivel 3
dataSel3 equ $-gdt + 3
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0xF2
  db 0x00
  db 0x00

; Descriptor de codigo nivel 3
codeSel3 equ $-gdt + 3
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0xFA
  db 0x00
  db 0x00

; Descriptores de pila nivel 3
pilaT1Sel3 equ $-gdt + 3
  dw 0x00FF
  dw pila_11_Tarea_1
  db 0x00
  db 0xF2
  db 0x00
  db 0x00

pilaT2Sel3 equ $-gdt + 3
  dw 0x00FF
  dw pila_11_Tarea_2
  db 0x00
  db 0xF2
  db 0x00
  db 0x00

pilaT3Sel3 equ $-gdt + 3
  dw 0x00FF
  dw pila_11_Tarea_3
  db 0x00
  db 0xF2
  db 0x00
  db 0x00

; Descriptores de TSS
tssInicialSel equ $-gdt
  dw 0x0067
  dw tssInicial
  db 0x00
  db 0x89
  db 0x00
  db 0x00

tssSchedulerSel equ $-gdt
  dw 0x0067
  dw tssScheduler
  db 0x00
  db 0x89
  db 0x00
  db 0x00

tssTarea1Sel equ $-gdt
  dw 0x0067
  dw tssTarea1
  db 0x00
  db 0x89
  db 0x00
  db 0x00

tssTarea2Sel equ $-gdt
  dw 0x0067
  dw tssTarea2
  db 0x00
  db 0x89
  db 0x00
  db 0x00

tssTarea3Sel equ $-gdt
  dw 0x0067
  dw tssTarea3
  db 0x00
  db 0x89
  db 0x00
  db 0x00

gdtSize equ $-gdt

;***********************************
;	             IDT
;***********************************
idt:

  resb 8*0x20

  dw 0x0000
  dw tssSchedulerSel
  db 0x00
  db 0x85
  dw 0x0000

  dw irq1Handler
  dw codeSel0
  db 0x00
  db 0x86
  dw 0x0000

idtSize equ $-idt

;***********************************
;	Codigo
;***********************************
inicio:
  cli

  ; Reprogramo PIC
  mov al, 0x11
  out 0x20, al
  mov al, 0x20
  out 0x21, al
  mov al, 0x04
  out 0x21, al
  mov al, 0x01
  out 0x21, al

  mov al, 0x11
  out 0xA0, al
  mov al, 0x28
  out 0xA1, al
  mov al, 0x02
  out 0xA1, al
  mov al, 0x01
  out 0xA1, al

  mov al, 0xFF
  out 0x21, al
  mov al, 0xFF
  out 0xA1, al

  ; Inicializo COM1
  mov dx, 0x03FB
  mov al, 0x80
  out dx, al

  mov dx, 0x03F8
  mov al, 0x0C
  out dx, al

  mov dx, 0x03F9
  mov al, 0x00
  out dx, al

  mov dx, 0x03FB
  mov al, 0x03
  out dx, al

  ; Cargo GDTR e IDTR
  mov ax, gdtSize - 1
  mov [gdtr + 0], ax

  xor eax, eax
  mov eax, gdt
  mov [gdtr + 2], eax

  mov ax, idtSize - 1
  mov [idtr + 0], ax

  xor eax, eax
  mov eax, idt
  mov [idtr + 2], eax

  lgdt [gdtr]
  lidt [idtr]

  ; Paso a modo protegido
  mov eax, cr0
  or al, 1
  mov cr0, eax
  jmp short $+2

  jmp codeSel0:modo_protegido

modo_protegido:
  mov ax, dataSel0
  mov ds, ax
  mov ss, ax

  mov ax, videoSel0
  mov es, ax

  mov eax, pila_00_Inicial + 0xFF
  mov esp, eax

  mov ax, tssInicialSel
  ltr ax

  ; Programo PIT
  mov al, 0x36
  out 0x43, al
  mov ax, 0x2E9C
  out 0x40, al
  mov al, ah
  out 0x40, al

  call limpiar_pantalla
  call imprimir_titulos
  call mostrar_prioridades

  ; Habilito IRQ0 e IRQ1
  mov al, 0xFC
  out 0x21, al
  mov al, 0xFF
  out 0xA1, al

  sti

jmp $

;***********************************
;	Scheduler
;***********************************
irq0Scheduler:
  cli

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  mov al, 0x20
  out 0x20, al

  call actualizar_tiempo_rtc

  ;tarea 3: puerto serie una vez por segundo
  cmp byte [serial_pendiente], 1
  jne scheduler_pantalla

  mov al, 0x89
  mov [gdt + tssTarea3Sel + 5], al
  sti
  jmp tssTarea3Sel:0
  jmp irq0Scheduler

scheduler_pantalla:
  mov al, [ranura]
  cmp al, [prioridad_t1]
  jb ejecutar_tarea1

ejecutar_tarea2:
  call avanzar_ranura
  call marcar_inicio_t2
  call sumar_tiempo_t2      ;corte a los 180 segundos efectivos

  mov al, 0x89
  mov [gdt + tssTarea2Sel + 5], al
  sti
  jmp tssTarea2Sel:0
  jmp irq0Scheduler

ejecutar_tarea1:
  call avanzar_ranura
  call marcar_inicio_t1
  call sumar_tiempo_t1      ;corte a los 180 segundos efectivos

  mov al, 0x89
  mov [gdt + tssTarea1Sel + 5], al
  sti
  jmp tssTarea1Sel:0
  jmp irq0Scheduler

avanzar_ranura:
  inc byte [ranura]
  cmp byte [ranura], 10
  jb fin_ranura
  mov byte [ranura], 0
fin_ranura:
  ret

;***********************************
;	Tiempo efectivo y RTC
;***********************************
actualizar_tiempo_rtc:
  push ax
  push bx

  call leer_rtc

  mov al, [rtc_seg]
  cmp byte [ultimo_seg_rtc], 0xFF
  jne comparar_segundo_rtc

  mov [ultimo_seg_rtc], al
  jmp fin_actualizar_tiempo_rtc

comparar_segundo_rtc:
  cmp al, [ultimo_seg_rtc]
  je fin_actualizar_tiempo_rtc

  mov [ultimo_seg_rtc], al
  mov byte [serial_pendiente], 1

fin_actualizar_tiempo_rtc:
  pop bx
  pop ax
  ret

marcar_inicio_t1:
  cmp byte [inicio_t1_ok], 1
  je fin_marcar_inicio_t1

  call leer_rtc
  mov al, [rtc_hora]
  mov [t1_ini_hora], al
  mov al, [rtc_min]
  mov [t1_ini_min], al
  mov al, [rtc_seg]
  mov [t1_ini_seg], al

  mov byte [inicio_t1_ok], 1
fin_marcar_inicio_t1:
  ret

marcar_inicio_t2:
  cmp byte [inicio_t2_ok], 1
  je fin_marcar_inicio_t2

  call leer_rtc
  mov al, [rtc_hora]
  mov [t2_ini_hora], al
  mov al, [rtc_min]
  mov [t2_ini_min], al
  mov al, [rtc_seg]
  mov [t2_ini_seg], al

  mov byte [inicio_t2_ok], 1
fin_marcar_inicio_t2:
  ret

sumar_tiempo_t1:
  inc byte [ms_t1]
  cmp byte [ms_t1], 100
  jb fin_sumar_t1

  mov byte [ms_t1], 0
  inc word [tiempo_t1_seg]

  cmp word [tiempo_t1_seg], 180
  jae fin_por_tiempo_efectivo

fin_sumar_t1:
  ret

sumar_tiempo_t2:
  inc byte [ms_t2]
  cmp byte [ms_t2], 100
  jb fin_sumar_t2

  mov byte [ms_t2], 0
  inc word [tiempo_t2_seg]

  cmp word [tiempo_t2_seg], 180
  jae fin_por_tiempo_efectivo

fin_sumar_t2:
  ret

fin_por_tiempo_efectivo:
  mov si, msg_tiempo
  call detener_sistema

; Conversiones
bcd_a_bin:
  push bx
  mov bl, al
  and al, 0x0F
  mov bh, al
  mov al, bl
  shr al, 4
  and al, 0x0F
  mov bl, 10
  mul bl
  add al, bh
  xor ah, ah
  pop bx
  ret

bin_a_bcd:
  push bx
  xor ah, ah
  mov bl, 10
  div bl
  shl al, 4
  or al, ah
  pop bx
  ret

; Calculo de hora de las tareas
segundos_t1:
  xor eax, eax
  mov al, [t1_ini_hora]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 3600
  mul ebx
  mov ecx, eax

  xor eax, eax
  mov al, [t1_ini_min]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 60
  mul ebx
  add ecx, eax

  xor eax, eax
  mov al, [t1_ini_seg]
  call bcd_a_bin
  and eax, 0x000000FF
  add eax, ecx

  xor ebx, ebx
  mov bx, [tiempo_t1_seg]
  add eax, ebx

  cmp eax, 86400
  jb fin_segundos_t1
  sub eax, 86400
fin_segundos_t1:
  ret

segundos_t2:
  xor eax, eax
  mov al, [t2_ini_hora]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 3600
  mul ebx
  mov ecx, eax

  xor eax, eax
  mov al, [t2_ini_min]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 60
  mul ebx
  add ecx, eax

  xor eax, eax
  mov al, [t2_ini_seg]
  call bcd_a_bin
  and eax, 0x000000FF
  add eax, ecx

  xor ebx, ebx
  mov bx, [tiempo_t2_seg]
  add eax, ebx

  cmp eax, 86400
  jb fin_segundos_t2
  sub eax, 86400
fin_segundos_t2:
  ret

calcular_hora_t1:
  push eax
  push ebx
  push ecx
  push edx
  push esi

  call segundos_t1

  xor edx, edx
  mov ebx, 3600
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [rtc_hora], al

  mov eax, esi
  xor edx, edx
  mov ebx, 60
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [rtc_min], al

  mov eax, esi
  call bin_a_bcd
  mov [rtc_seg], al

  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
  ret

calcular_hora_t2:
  push eax
  push ebx
  push ecx
  push edx
  push esi

  call segundos_t2

  xor edx, edx
  mov ebx, 3600
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [rtc_hora], al

  mov eax, esi
  xor edx, edx
  mov ebx, 60
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [rtc_min], al

  mov eax, esi
  call bin_a_bcd
  mov [rtc_seg], al

  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
  ret

;***********************************
;	Tareas
;***********************************
tarea1:
bucle_tarea1:
  call cgServicioHoraT1:ret_tarea1
ret_tarea1:
  jmp bucle_tarea1

tarea2:
bucle_tarea2:
  call cgServicioFechaHoraT2:ret_tarea2
ret_tarea2:
  jmp bucle_tarea2

tarea3:
bucle_tarea3:
  cmp byte [serial_pendiente], 1
  jne bucle_tarea3

  mov byte [serial_pendiente], 0
  call cgServicioSerialHora:ret_tarea3
ret_tarea3:
  jmp bucle_tarea3

;***********************************
;	Teclado
;***********************************
irq1Handler:
  cli

  push ax

  mov ax, ds
  mov [irq1_ds_save], ax
  mov ax, es
  mov [irq1_es_save], ax

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  in al, 0x60

  test al, 0x80
  jnz fin_irq1

  cmp al, 0x3C
  je tecla_f2

  cmp al, 0x3D
  je tecla_f3

  cmp al, 0x44
  je tecla_f10

  jmp fin_irq1

tecla_f2:
  cmp byte [prioridad_t1], 10
  jae fin_irq1
  inc byte [prioridad_t1]
  call mostrar_prioridades
  jmp fin_irq1

tecla_f3:
  cmp byte [prioridad_t1], 0
  jbe fin_irq1
  dec byte [prioridad_t1]
  call mostrar_prioridades
  jmp fin_irq1

tecla_f10:
  mov al, 0x20
  out 0x20, al
  mov si, msg_fin
  call detener_sistema

fin_irq1:
  mov al, 0x20
  out 0x20, al

  mov ax, [irq1_es_save]
  mov es, ax
  mov ax, [irq1_ds_save]
  mov ds, ax

  pop ax

  sti
  iret

;***********************************
;	Servicios por call gate
;***********************************
servicioHoraT1:
  push ax
  push bx
  push cx
  push dx
  push si
  push di

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  call leer_rtc
  call calcular_hora_t1

  mov di, ((8*80)+35)*2
  call imprimir_hora

  mov ax, dataSel3
  mov ds, ax
  mov es, ax

  pop di
  pop si
  pop dx
  pop cx
  pop bx
  pop ax
  retf

servicioFechaHoraT2:
  push ax
  push bx
  push cx
  push dx
  push si
  push di

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  call leer_rtc
  mov di, ((16*80)+35)*2
  call imprimir_fecha
  call calcular_hora_t2

  mov di, ((17*80)+35)*2
  call imprimir_hora

  mov ax, dataSel3
  mov ds, ax
  mov es, ax

  pop di
  pop si
  pop dx
  pop cx
  pop bx
  pop ax
  retf

servicioSerialHora:
  push ax
  push bx
  push cx
  push dx
  push si
  push di

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  call leer_rtc

  mov al, [rtc_hora]
  call enviar_bcd2_serie
  mov al, ':'
  call enviar_caracter_serie
  mov al, [rtc_min]
  call enviar_bcd2_serie
  mov al, ':'
  call enviar_caracter_serie
  mov al, [rtc_seg]
  call enviar_bcd2_serie

  mov al, 13
  call enviar_caracter_serie
  mov al, 10
  call enviar_caracter_serie

  mov ax, dataSel3
  mov ds, ax
  mov es, ax

  pop di
  pop si
  pop dx
  pop cx
  pop bx
  pop ax
  retf

;***********************************
;	RTC
;***********************************
leer_rtc:
esperar_rtc:
  mov al, 0x0A
  out 0x70, al
  in al, 0x71
  test al, 0x80
  jnz esperar_rtc

  mov al, 0x00
  call leer_cmos
  mov [rtc_seg], al

  mov al, 0x02
  call leer_cmos
  mov [rtc_min], al

  mov al, 0x04
  call leer_cmos
  and al, 0x7F
  mov [rtc_hora], al

  mov al, 0x07
  call leer_cmos
  mov [rtc_dia], al

  mov al, 0x08
  call leer_cmos
  mov [rtc_mes], al

  mov al, 0x09
  call leer_cmos
  mov [rtc_anio], al

  ret

leer_cmos:
  out 0x70, al
  jmp short $+2
  in al, 0x71
  ret

;***********************************
;	Pantalla
;***********************************
limpiar_pantalla:
  push ax
  push cx
  push di

  mov ax, 0x7020
  mov cx, 80*25
  xor di, di

bucle_limpiar:
  mov [es:di], ax
  add di, 2
  loop bucle_limpiar

  pop di
  pop cx
  pop ax
  ret

imprimir_titulos:
  push ax
  push bx
  push cx
  push si
  push di

  mov bl, 0x70

  mov si, msg_t1
  mov di, ((8*80)+10)*2
  call imprimir_cadena

  mov si, msg_t2
  mov di, ((16*80)+10)*2
  call imprimir_cadena

  mov si, msg_prio
  mov di, ((22*80)+10)*2
  call imprimir_cadena

  mov si, msg_fin
  mov di, ((24*80)+50)*2
  call imprimir_cadena

  pop di
  pop si
  pop cx
  pop bx
  pop ax
  ret

imprimir_cadena:
  push ax
  push si
  push di

bucle_cadena:
  mov al, [si]
  cmp al, 0
  je fin_cadena
  mov [es:di], al
  inc di
  mov al, bl
  mov [es:di], al
  inc di
  inc si
  jmp bucle_cadena

fin_cadena:
  pop di
  pop si
  pop ax
  ret

imprimir_caracter:
  mov [es:di], al
  inc di
  mov byte [es:di], 0x70
  inc di
  ret

imprimir_bcd2:
  push ax
  push bx

  mov bl, al
  shr al, 4
  and al, 0x0F
  add al, '0'
  call imprimir_caracter

  mov al, bl
  and al, 0x0F
  add al, '0'
  call imprimir_caracter

  pop bx
  pop ax
  ret

imprimir_hora:
  mov al, [rtc_hora]
  call imprimir_bcd2
  mov al, ':'
  call imprimir_caracter
  mov al, [rtc_min]
  call imprimir_bcd2
  mov al, ':'
  call imprimir_caracter
  mov al, [rtc_seg]
  call imprimir_bcd2
  ret

imprimir_fecha:
  mov al, [rtc_dia]
  call imprimir_bcd2
  mov al, ':'
  call imprimir_caracter
  mov al, [rtc_mes]
  call imprimir_bcd2
  mov al, ':'
  call imprimir_caracter
  mov al, '2'
  call imprimir_caracter
  mov al, '0'
  call imprimir_caracter
  mov al, [rtc_anio]
  call imprimir_bcd2
  ret

mostrar_prioridades:
  push ax
  push bx
  push di

  mov al, [prioridad_t1]
  mov bl, 10
  mul bl
  mov di, ((22*80)+37)*2
  call imprimir_numero_3

  mov al, '/'
  call imprimir_caracter

  mov al, 10
  sub al, [prioridad_t1]
  mov bl, 10
  mul bl
  call imprimir_numero_3

  pop di
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
  call imprimir_caracter

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
  call imprimir_caracter

  mov ax, cx

  pop dx
  pop cx
  ret

;***********************************
;	Puerto serie
;***********************************
enviar_bcd2_serie:
  push ax
  push bx

  mov bl, al
  shr al, 4
  and al, 0x0F
  add al, '0'
  call enviar_caracter_serie

  mov al, bl
  and al, 0x0F
  add al, '0'
  call enviar_caracter_serie

  pop bx
  pop ax
  ret

enviar_caracter_serie:
  push ax
  push dx

serie_esperar:
  mov dx, 0x03FD
  in al, dx
  test al, 0x20
  jz serie_esperar

  pop dx
  pop ax
  mov dx, 0x03F8
  out dx, al
  ret

;***********************************
;	Fin del programa
;***********************************
detener_sistema:
  cli

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  call limpiar_pantalla

  mov bl, 0x70
  mov di, ((10*80)+30)*2
  call imprimir_cadena

  mov si, msg_stop
  mov di, ((12*80)+30)*2
  call imprimir_cadena

  hlt
  jmp $

;***********************************
;       Pilas
;***********************************
pila_00_Inicial:
  resb 0x100

pila_00_Scheduler:
  resb 0x100

pila_00_Tarea_1:
  resb 0x100

pila_00_Tarea_2:
  resb 0x100

pila_00_Tarea_3:
  resb 0x100

pila_11_Tarea_1:
  resb 0x100

pila_11_Tarea_2:
  resb 0x100

pila_11_Tarea_3:
  resb 0x100

fin:
