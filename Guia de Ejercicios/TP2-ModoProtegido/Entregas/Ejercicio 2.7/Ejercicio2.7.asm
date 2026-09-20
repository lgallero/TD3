;*******************************************************
; Ejercicio 2.7                          
; Alumno: Gallero, Lucas
; Legajo: 25632
;*******************************************************
;codigo 16 bits
use16

;offset del codigo en 0x8000
org 8000h

;voy a inicio (salteo variables)
jmp inicio

;***********************************
;	variables
;***********************************
; global descriptor register
gdtr	resb	6
idtr 	resb    6

colorPantalla       db 0 
colorPantalla2      db 0 
timer_tick	        db 0
cont_seg            db 0
max_timer_tick	    db 100
breaking_point	    db 50
ms_tarea1           db 0
ms_tarea2           db 0
tiempo_tarea1_seg   dw 0
tiempo_tarea2_seg   dw 0
inicio_tarea1_ok    db 0
inicio_tarea2_ok    db 0
tarea1_ini_horas    db 0
tarea1_ini_minutos  db 0
tarea1_ini_segundos db 0
tarea2_ini_horas    db 0
tarea2_ini_minutos  db 0
tarea2_ini_segundos db 0
msg_t1              db 'TAREA 1 - HORA:', 0
msg_t2              db 'TAREA 2 - FECHA/HORA:', 0
msg_prio            db 'F2/F3 PRIORIDAD T1/T2:', 0
msg_fin             db 'F10 FINALIZA - LIMPIA Y HLT', 0
msg_stop            db 'FIN DEL PROGRAMA', 0
msg_tiempo          db 'UNA TAREA LLEGO A 3 MINUTOS', 0
horas               db 0
minutos             db 0
segundos            db 0
dia                 db 0
mes                 db 0
anos                db 0
u_horas             db 0
d_horas             db 0
u_minutos           db 0
d_minutos           db 0
u_segundos          db 0
d_segundos          db 0
u_dia               db 0
d_dia               db 0
u_mes               db 0
d_mes               db 0
u_anos              db 0
d_anos              db 0
c_anos              db 0
m_anos              db 0

;****************************
;DECLARACION DE TSS
;****************************
tssInicial	resb 68h

tssNivel0	dd 0
		dd fin + 0x200
		dw dataSel0
		dw 0
		dd 0	;esp1
		dd 0	;ss1
		dd 0	;esp2
		dd 0	;ss2
		dd 0	;			cr3
		dd irq0Handler	;eip
		dd 0x202	;eflags
		dd 0		;eax
		dd 0		;ecx
		dd 0		;edx
		dd 0		;ebx
		dd fin + 0x200	;esp	
		dd 0		;ebp
		dd 0		;esi
		dd 0		;edi
		dw videoSel0;es
		dw 0
		dw codeSel0;cs
		dw 0
		dw dataSel0;ss
		dw 0
		dw dataSel0 ;ds
		dw 0 
		dd 0 ;fs
		dd 0;gs
		dd 0
		dd 0


;tss de la tarea 1
tssPrintPantalla	dd 0
		dd fin + 0x300
		dw dataSel0
		dw 0
		dd 0	;esp1
		dd 0	;ss1
		dd 0	;esp2
		dd 0	;ss2
		dd 0	;cr3
		dd tareaPrintPantalla	;eip
		dd 0x202	;eflags
		dd 0		;eax
		dd 0		;ecx
		dd 0		;edx
		dd 0		;ebx
		dd fin + 0x300	;esp	
		dd 0		;ebp
		dd 0		;esi
		dd 0		;edi
		dw 0    ;es
		dw 0
		dw codeSel3;cs
		dw 0
		dw dataSel3;ss
		dw 0
		dw dataSel3 ;ds
		dw 0 
		dd 0 ;fs
		dd 0;gs
		dd 0
		dd 0
		
;tss de la tarea 2
tssPrintPantalla2	dd 0
		dd fin + 0x400
		dw dataSel0
		dw 0
		dd 0	;esp1
		dd 0	;ss1
		dd 0	;esp2
		dd 0	;ss2
		dd 0	;cr3
		dd tareaPrintPantalla2	;eip
		dd 0x202	;eflags
		dd 0		;eax
		dd 0		;ecx
		dd 0		;edx
		dd 0		;ebx
		dd fin + 0x400	;esp	
		dd 0		;ebp
		dd 0		;esi
		dd 0		;edi
		dw 0    ;es
		dw 0
		dw codeSel3;cs
		dw 0
		dw dataSel3;ss
		dw 0
		dw dataSel3;ds
		dw 0 
		dd 0 ;fs
		dd 0;gs
		dd 0
		dd 0

;tss de la tarea 3
tssPuertoSerie	dd 0
		dd fin + 0x500
		dw dataSel0
		dw 0
		dd 0	;esp1
		dd 0	;ss1
		dd 0	;esp2
		dd 0	;ss2
		dd 0	;cr3
		dd tareaPuertoSerie	;eip
		dd 0x202	;eflags
		dd 0		;eax
		dd 0		;ecx
		dd 0		;edx
		dd 0		;ebx
		dd fin + 0x500	;esp	
		dd 0		;ebp
		dd 0		;esi
		dd 0		;edi
		dw 0    ;es
		dw 0
		dw codeSel0;cs
		dw 0
		dw dataSel0;ss
		dw 0
		dw dataSel0;ds
		dw 0 
		dd 0 ;fs
		dd 0;gs
		dd 0
		dd 0
	
;***********************************
;	           GDT
;***********************************
gdt:
  ;descriptor nulo
  resb 8

; descriptor de datos (Offset 0, size 64k, nivel 0)
dataSel0 equ $-gdt

  dw 0xffff
  dw 0x0000
  db 0x00
  db 10010010b
  db 0x00
  db 0x00
; descriptor de video (offset: 0xb8000, size 4k, nivel 0)
videoSel0 equ $-gdt

  dw 0x1000
  dw 0x8000
  db 0x0b
  db 10010010b
  db 0x00
  db 0x00

; descriptor de codigo (offset: 0x0000, size 64k, nivel 0)
codeSel0 equ $-gdt
  dw 0xffff
  dw 0x0000
  db 0x00
  db 10011010b
  db 0x00
  db 0x00

dataSel3 equ $-gdt+3

  dw 0xffff
  dw 0x0000
  db 0x00
  db 11110010b
  db 0x00
  db 0x00

codeSel3 equ $-gdt+3
  dw 0xffff
  dw 0x0000
  db 0x00
  db 11111010b
  db 0x00
  db 0x00

cgPrintPantalla equ $-gdt+3
 dw printPantalla1N0
 dw codeSel0
 db 0
 db 11100100b
 dw 0

cgPrintPantalla2 equ $-gdt+3
 dw printPantalla2N0
 dw codeSel0
 db 0
 db 11100100b
 dw 0

cgLeerfecha equ $-gdt+3
 dw leer_fecha
 dw codeSel0
 db 0
 db 11100100b
 dw 0

tssInicialSel equ $ -gdt
  dw 67h
  dw tssInicial
  db 0
  db 10001001b
  db 0
  db 0

tssNivel0Sel equ $ -gdt
  dw 67h
  dw tssNivel0
  db 0
  db 10001001b
  db 0
  db 0

tssPrintPantallaSel equ $ -gdt
  dw 67h
  dw tssPrintPantalla
  db 0
  db 10001001b
  db 0
  db 0

tssPrintPantalla2Sel equ $ -gdt
  dw 67h
  dw tssPrintPantalla2
  db 0
  db 10001001b
  db 0
  db 0

tssPuertoSerieSel equ $ -gdt
  dw 67h
  dw tssPuertoSerie
  db 0
  db 10001001b
  db 0
  db 0

gdtSize equ $-gdt

;***********************************
;	             IDT
;***********************************
idt:
  ;entradas no utilizadas

  resb 8*8

  ;descriptor de puerta de tarea (int 8, irq0, timer tick)
  dw 0
  dw tssNivel0Sel
  db 0
  db 10000101b
  dw 0x00

  ;descriptor de int (int 9h, irq1, teclado)
  ;descriptor de interrupcion 21(interrupcion nro: 21h, irq1)
  dw irq1Handler 
  dw codeSel0
  db 0
  db 10000110b	
  dw 0x00

idtSize equ $-idt


;***********************************
;	Codigo
;***********************************
;comienzo del codigo
inicio: 
;deshabilito interruciones
cli

;cargo gdtr
mov ax, gdtSize - 1	;longitud de gdt - 1
mov [gdtr + 0], ax		

xor eax, eax
mov ax, gdt
mov [gdtr + 2], eax	;ubicacion de la gdt en memoria	

;cargo idtr
mov ax, idtSize - 1	;longitud de idt - 1
mov [idtr + 0], ax		

xor eax, eax
mov eax, idt
mov [idtr + 2], eax	;ubicacion de la idt en memoria	

; cargo en el registro gdtr
lgdt [gdtr]

; cargo en el registro idtr
lidt [idtr]

;***********************
;PASAR A MODO PROTEGIDO
;***********************
mov eax, cr0
or al,1
mov cr0, eax
jmp short $+2

;cargo ss, ds, y es con el selector de datos
mov ax, dataSel0
mov ss, ax
mov ds, ax
mov es, ax

;cargo en cs el selector de codigo codeSel0
jmp codeSel0:modo_protegido

modo_protegido:

;apunto esp al final del codigo + 0x100
mov eax, fin + 0x100
mov esp, eax

;----------------------------------------------------------------------
;enmascaro todas las interrupciones menos la irq1 e irq0
;----------------------------------------------------------------------
mov al, 0xfc
out 0x21, al
mov al, 0xff
out 0xa1, al

cli
mov ax, videoSel0
mov ds, ax  
;ds data segment

mov cx, 80*25

;atributo del bloque de pantalla
mov al, 0x70   

;offset en el descriptor
mov edi, 0
;Limpio pantalla
bucle_pantalla0:

  mov byte [edi], 0
  inc edi
  mov [edi], al
  inc edi
  loop bucle_pantalla0

;imprimo leyendas iniciales
mov ax, dataSel0
mov ds, ax
mov ax, videoSel0
mov es, ax
call imprimir_titulos
call mostrar_prioridades

;cargo tss inicial
mov ax, tssInicialSel
ltr ax

;programo PIT a 100 Hz
mov al, 0x36
out 0x43, al
mov ax, 0x2E9C
out 0x40, al
mov al, ah
out 0x40, al

;habilito interrupciones
sti
;loop infinito
jmp $

;tarea asociada a la irq0
irq0Handler:
  ;deshabilito interrupciones
  cli
  ;bajo flag de interrupcion
  mov al, 0x20
  out 0x20, al

  ;bajo flag de busy de la tarea que pinta la pantalla
  mov al, 0xe9
  mov [gdt + tssPrintPantallaSel + 5], al
  mov [gdt + tssPrintPantalla2Sel + 5], al

  mov al, 0x89
  mov [gdt + tssPuertoSerieSel + 5], al

  ;incremento el color de la pantalla
  
  add byte[timer_tick], 1
  add byte[cont_seg], 1
  mov al, byte[timer_tick]
  mov bl, byte[max_timer_tick]
  cmp al, bl 
  jl no_reset
  mov byte[timer_tick], 0 

  no_reset:

  primeraTarea:
  ;habilito interrupciones
  mov al, byte[timer_tick]
  mov bl, byte[breaking_point]
  cmp al, bl
  jae segundaTarea

  call marcar_inicio_tarea1
  call sumar_tiempo_tarea1
  sti
  jmp tssPrintPantallaSel:0
  jmp fin_scheduler
  
  segundaTarea:
  call marcar_inicio_tarea2
  call sumar_tiempo_tarea2
  sti
  jmp tssPrintPantalla2Sel:0

 fin_scheduler:
 
  ;permanezco indefinidamente en la tarea
jmp irq0Handler

;tarea que imprime la pantalla
tareaPrintPantalla:
  
  ;cargo valor del atributo (cargo uno nuevo)
  mov byte[colorPantalla], 0x70
  

  call cgLeerfecha:0
  call calcular_hora_tarea1

  ;cargo ds con el selector de video
  ;cargo cantidad de puntos en pantalla a pintar
  xor ah, ah
  mov al, byte[horas]
  mov bl, 0x10
  div bl
  mov byte[u_horas], ah
  mov byte[d_horas], al

  xor ah, ah
  mov al, byte[minutos]
  mov bl, 0x10
  div bl
  mov byte[u_minutos], ah
  mov byte[d_minutos], al

  xor ah, ah
  mov al, byte[segundos]
  mov bl, 0x10
  div bl
  mov byte[u_segundos], ah
  mov byte[d_segundos], al

  mov bl, [colorPantalla]
  call cgPrintPantalla:0
jmp tareaPrintPantalla

printPantalla1N0:
  mov ax, videoSel0
  mov es, ax
  mov cx, 1040
  ;cargo edi = 1
  mov edi, 1
  bucle_pantalla:
    ;guardo en ds:edi el valor del atributo
    mov [es:edi], bl
    ;incremento edi en dos 
    inc edi
    inc edi
    ;ciclo hasta terminar de recorrer el buffer de video
    loop bucle_pantalla
  
  mov edi, ((8*80)+35)*2

  mov al, byte[d_horas]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_horas]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi

  mov byte [es:edi], 0x3A
  inc edi
  inc edi

  mov al, byte[d_minutos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_minutos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi

  mov byte [es:edi], 0x3A
  inc edi
  inc edi
  
  mov al, byte[d_segundos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_segundos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi

retf
;tarea que imprime la pantalla
tareaPrintPantalla2:


  mov byte[colorPantalla2], 0x70
  
  call cgLeerfecha:0
  call calcular_hora_tarea2

  xor ah, ah
  mov al, byte[horas]
  mov bl, 0x10
  div bl
  mov byte[u_horas], ah
  mov byte[d_horas], al

  xor ah, ah
  mov al, byte[minutos]
  mov bl, 0x10
  div bl
  mov byte[u_minutos], ah
  mov byte[d_minutos], al

  xor ah, ah
  mov al, byte[segundos]
  mov bl, 0x10
  div bl
  mov byte[u_segundos], ah
  mov byte[d_segundos], al


  xor ah, ah
  mov al, byte[dia]
  mov bl, 0x10
  div bl
  mov byte[u_dia], ah
  mov byte[d_dia], al

  xor ah, ah
  xor al, al
  mov al, byte[mes]
  mov bl, 0x10
  div bl
  mov byte[u_mes], ah
  mov byte[d_mes], al

  xor ah, ah
  mov al, byte[anos]
  mov bl, 0x10
  div bl
  mov byte[u_anos], ah
  xor ah, ah
  div bl
  mov byte[d_anos], ah
  xor ah, ah
  div bl
  mov byte[c_anos], ah
  xor ah, ah
  div bl
  mov byte[m_anos], ah

  ;cargo ds con el selector de video
  ;cargo cantidad de puntos en pantalla a pintar
  mov bl, [colorPantalla2]
  call cgPrintPantalla2:0

jmp tareaPrintPantalla2

printPantalla2N0:

  mov cx, 960
  ;cargo edi = 1
  mov edi, 2081
  mov ax, videoSel0
  mov es, ax
  bucle_pantalla2:
    ;guardo en ds:edi el valor del atributo
    mov [es:edi], bl
    ;incremento edi en dos 
    inc edi
    inc edi
    ;ciclo hasta terminar de recorrer el buffer de video
    loop bucle_pantalla2

  mov edi, ((16*80)+35)*2
  mov al, byte[d_dia]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_dia]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi


  mov byte [es:edi], 0x3A
  inc edi
  inc edi

  mov al, byte[d_mes]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_mes]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi 

  mov byte [es:edi], 0x3A
  inc edi
  inc edi

  mov al, byte[m_anos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[c_anos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi 
  mov al, byte[d_anos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi 
  mov al, byte[u_anos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi 

  mov edi, ((17*80)+35)*2

  mov al, byte[d_horas]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_horas]
  add al, 0x30
  mov byte [es:edi], al
  inc edi
  inc edi

  mov byte [es:edi], 0x3A
  inc edi
  inc edi

  mov al, byte[d_minutos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_minutos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi

  mov byte [es:edi], 0x3A
  inc edi
  inc edi
  
  mov al, byte[d_segundos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi
  mov al, byte[u_segundos]
  add al, 0x30
  mov byte [es:edi], al
  inc edi 
  inc edi


retf
;vuelvo al inicio de la tarea

leer_fecha:

 ;segundos
 mov al, 0x00
 out 70h, al
 in  al, 71h
 mov byte[segundos], al

 ;minutos
 mov al, 0x02
 out 70h, al
 in  al, 71h
 mov byte[minutos], al

 ;horas
 mov al, 0x04
 out 70h ,al
 in  al, 71h
 mov byte[horas], al

 ;dias
 mov al, 0x07
 out 70h , al
 in  al, 71h
 mov byte[dia], al

 ;mes
 mov al, 0x08
 out 70h, al
 in  al, 71h
 mov byte[mes], al

 ;año
 mov al, 0x09
 out 70h, al
 in  al, 71h
 mov byte[anos], al

retf

tareaPuertoSerie:
  mov al, byte[horas]
  out 0x2E8, al
  mov al, byte[minutos]
  out 0x2E8, al
  mov al, byte[segundos]
  out 0x2E8, al

jmp tareaPuertoSerie

irq1Handler:
  cli
  mov al, 0x20
  out 0x20, al
  in al, 0x60
  mov bl, al
  cmp al, 0x3C        ;F2
  je incrementar
  cmp al, 0X3D        ;F3
  je decrementar
  cmp bl, 0x44        ;F10
  je fin_teclado
  jmp fin_irq1
 incrementar:
  mov al, byte[breaking_point]
  cmp al, 100
  jae fin_irq1
  add byte [breaking_point], 10
  push es
  mov ax, videoSel0
  mov es, ax
  call mostrar_prioridades
  pop es
  jmp fin_irq1
 decrementar:
  mov al, byte[breaking_point]
  cmp al, 0
  jbe fin_irq1
  sub byte[breaking_point], 10
  push es
  mov ax, videoSel0
  mov es, ax
  call mostrar_prioridades
  pop es
  jmp fin_irq1
 fin_teclado:
  mov si, msg_fin
  call detener_sistema

fin_irq1:
  sti 
  iret

;***********************************
;	Inicio y hora de tareas
;***********************************
marcar_inicio_tarea1:
  cmp byte [inicio_tarea1_ok], 1
  je fin_marcar_inicio_tarea1

  call cgLeerfecha:0
  mov al, [horas]
  mov [tarea1_ini_horas], al
  mov al, [minutos]
  mov [tarea1_ini_minutos], al
  mov al, [segundos]
  mov [tarea1_ini_segundos], al
  mov byte [inicio_tarea1_ok], 1

fin_marcar_inicio_tarea1:
  ret

marcar_inicio_tarea2:
  cmp byte [inicio_tarea2_ok], 1
  je fin_marcar_inicio_tarea2

  call cgLeerfecha:0
  mov al, [horas]
  mov [tarea2_ini_horas], al
  mov al, [minutos]
  mov [tarea2_ini_minutos], al
  mov al, [segundos]
  mov [tarea2_ini_segundos], al
  mov byte [inicio_tarea2_ok], 1

fin_marcar_inicio_tarea2:
  ret

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

segundos_tarea1:
  xor eax, eax
  mov al, [tarea1_ini_horas]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 3600
  mul ebx
  mov ecx, eax

  xor eax, eax
  mov al, [tarea1_ini_minutos]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 60
  mul ebx
  add ecx, eax

  xor eax, eax
  mov al, [tarea1_ini_segundos]
  call bcd_a_bin
  and eax, 0x000000FF
  add eax, ecx

  xor ebx, ebx
  mov bx, [tiempo_tarea1_seg]
  add eax, ebx

  cmp eax, 86400
  jb fin_segundos_tarea1
  sub eax, 86400
fin_segundos_tarea1:
  ret

segundos_tarea2:
  xor eax, eax
  mov al, [tarea2_ini_horas]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 3600
  mul ebx
  mov ecx, eax

  xor eax, eax
  mov al, [tarea2_ini_minutos]
  call bcd_a_bin
  and eax, 0x000000FF
  mov ebx, 60
  mul ebx
  add ecx, eax

  xor eax, eax
  mov al, [tarea2_ini_segundos]
  call bcd_a_bin
  and eax, 0x000000FF
  add eax, ecx

  xor ebx, ebx
  mov bx, [tiempo_tarea2_seg]
  add eax, ebx

  cmp eax, 86400
  jb fin_segundos_tarea2
  sub eax, 86400
fin_segundos_tarea2:
  ret

calcular_hora_tarea1:
  push eax
  push ebx
  push ecx
  push edx
  push esi

  call segundos_tarea1

  xor edx, edx
  mov ebx, 3600
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [horas], al

  mov eax, esi
  xor edx, edx
  mov ebx, 60
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [minutos], al

  mov eax, esi
  call bin_a_bcd
  mov [segundos], al

  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
  ret

calcular_hora_tarea2:
  push eax
  push ebx
  push ecx
  push edx
  push esi

  call segundos_tarea2

  xor edx, edx
  mov ebx, 3600
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [horas], al

  mov eax, esi
  xor edx, edx
  mov ebx, 60
  div ebx
  mov esi, edx
  call bin_a_bcd
  mov [minutos], al

  mov eax, esi
  call bin_a_bcd
  mov [segundos], al

  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
  ret

;***********************************
;	Tiempo de tareas
;***********************************
sumar_tiempo_tarea1:
  inc byte [ms_tarea1]
  cmp byte [ms_tarea1], 100
  jb fin_sumar_tarea1

  mov byte [ms_tarea1], 0
  inc word [tiempo_tarea1_seg]

  cmp word [tiempo_tarea1_seg], 180
  jae fin_por_tiempo

fin_sumar_tarea1:
  ret

sumar_tiempo_tarea2:
  inc byte [ms_tarea2]
  cmp byte [ms_tarea2], 100
  jb fin_sumar_tarea2

  mov byte [ms_tarea2], 0
  inc word [tiempo_tarea2_seg]

  cmp word [tiempo_tarea2_seg], 180
  jae fin_por_tiempo

fin_sumar_tarea2:
  ret

fin_por_tiempo:
  mov si, msg_tiempo
  call detener_sistema

;***********************************
;	Pantalla inicial y fin
;***********************************
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

mostrar_prioridades:
  push ax
  push bx
  push di

  mov al, [breaking_point]
  mov di, ((22*80)+37)*2
  call imprimir_numero_3

  mov al, '/'
  call imprimir_caracter

  mov al, 100
  sub al, [breaking_point]
  call imprimir_numero_3

  pop di
  pop bx
  pop ax
  ret

imprimir_numero_3:
  push ax
  push bx

  xor ah, ah
  mov bl, 100
  div bl
  add al, '0'
  call imprimir_caracter

  mov al, ah
  xor ah, ah
  mov bl, 10
  div bl
  add al, '0'
  call imprimir_caracter

  mov al, ah
  add al, '0'
  call imprimir_caracter

  mov al, '%'
  call imprimir_caracter

  pop bx
  pop ax
  ret

detener_sistema:
  cli

  mov ax, dataSel0
  mov ds, ax
  mov ax, videoSel0
  mov es, ax

  mov cx, 80*25
  mov ax, 0x7020
  mov di, 0
limpiar_fin:
  mov [es:di], ax
  add di, 2
  loop limpiar_fin

  mov bl, 0x70
  mov di, ((10*80)+25)*2
  call imprimir_cadena

  mov si, msg_stop
  mov di, ((12*80)+30)*2
  call imprimir_cadena

  hlt
  jmp $

fin:
  mov si, msg_fin
  call detener_sistema


