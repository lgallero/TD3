# Blackjack multijugador en C para Linux

Proyecto académico de Técnicas Digitales III / Redes de Datos - UTN FRH.
Basado en la estructura de `servinet.c`, `clieinet.c` y `selectest.c`.

Se generan **dos programas**: `servidor` (master/dealer) y `cliente` (cada
jugador). Los hijos se crean automáticamente con `fork()`. No se ejecutan
manualmente y no hay un tercer programa que instalar.

## Compilar y jugar

En Debian/Ubuntu, si faltan las herramientas:

```sh
sudo apt install build-essential libncurses-dev
```

Abrir una terminal dentro de la carpeta que contiene el Makefile:

```sh
make
./servidor
```

En otras terminales de esa misma computadora:

```sh
./cliente 127.0.0.1 Lucas
./cliente 127.0.0.1 Juan
./cliente 127.0.0.1 Pedro
```

Para usar computadoras distintas, reemplazar `127.0.0.1` por la IPv4 de la
computadora del servidor. Ambas deben tener conectividad al puerto TCP 3490.
El cliente también acepta un nombre de host mediante `gethostbyname()`.

| Tecla | Acción |
|---|---|
| **I** | Iniciar una ronda cuando no hay otra activa. |
| **H** | Pedir una carta, si es tu turno. |
| **S** | Plantarte, si es tu turno. |
| **Q** | Salir de la mesa. |

Se aceptan minúsculas y mayúsculas. Se conserva H para pedir, según el
planteo original. La interfaz y los mensajes son en español. Usar una terminal
de al menos **80 columnas por 24 filas**. Si se achica, el cliente muestra un
aviso y sigue conectado. Ampliarla recupera la mesa.

Para usar otro puerto:

```sh
./servidor 5000
./cliente 127.0.0.1 Lucas 5000
```

El nombre puede tener de 1 a 31 caracteres ASCII: letras, números y, después
del primero, guiones o guiones bajos. No admite espacios ni nombres repetidos.
Si el cliente ncurses no puede registrarse, muestra la causa y termina;
volver a ejecutarlo con otro nombre.

Para terminar el servidor, presionar Ctrl+C en su terminal. Cierra los
canales IPC, los hijos cierran TCP y el padre espera su terminación. El cliente
restaura la terminal al salir o recibir Ctrl+C.

## Reglas de esta versión

- Entre **uno y cinco jugadores** contra el dealer.
- Mazo de 52 cartas, cuatro palos y trece valores. Se reconstruye y mezcla
  en cada ronda. Una carta repartida no vuelve al mazo durante esa ronda.
- A vale 11 o 1; J, Q y K valen 10.
- Dos cartas iniciales por jugador y dos para el dealer.
- La segunda carta del dealer permanece oculta hasta resolver la ronda.
  El master tampoco envía su valor ni el puntaje completo mientras está oculta.
- Los turnos siguen el orden de los lugares asignados, del 0 al 4.
- Se puede pedir varias veces hasta plantarse, llegar a 21 o pasarse.
  Al llegar a 21 se planta automáticamente; al superar 21 pierde y cede turno.
- El dealer pide con menos de 17 y se planta con 17 o más, incluido 17 blando.
- Blackjack natural (21 con dos cartas) vence a 21 con más cartas. Dos
  naturales empatan. Si el dealer tiene natural, la ronda se resuelve de inmediato.
- Si nadie queda con una mano válida, no hace falta que el dealer siga pidiendo.
- Sin apuestas, split, double down ni insurance.
- Cualquier jugador registrado puede iniciar la siguiente ronda con I.
  No hay inicio automático ni temporizador para forzar una jugada.
- Quien llega durante una ronda ve el estado pero queda **ESPERANDO**.
  Al terminar pasa a **LISTO** y recibe cartas cuando se inicia la siguiente.
- Quien se desconecta abandona esa ronda. Si era su turno, el master avanza.
  Volver a conectarse crea un registro nuevo; no recupera la mano anterior.

Los resultados se muestran en la mesa y en los mensajes finales. Se conservan
hasta iniciar otra ronda. La implementación comprueba límites de manos y mazo;
si se violaran, la ronda se marca ANULADA en lugar de acceder fuera del arreglo.

## Arquitectura

```text
                     servidor (padre / master / dealer)
                  mazo + jugadores + dealer + turno + ronda
                           select() sobre escucha e IPC
                              /          |          \
                         socketpair  socketpair  socketpair
                            /            |            \
                         hijo 1        hijo 2        hijo 3
                     select TCP/IPC  select TCP/IPC  select TCP/IPC
                            |            |            |
                           TCP          TCP          TCP
                            |            |            |
                         cliente      cliente      cliente
                         ncurses      ncurses      ncurses
```

La secuencia de aceptación mantiene el estilo de clase:

```text
socket → bind → listen → accept → socketpair → fork
```

Después de `fork()`:

| Proceso | Conserva | Cierra |
|---|---|---|
| Padre | Escucha y `canal[0]`. | TCP aceptado y `canal[1]`. |
| Hijo | TCP del cliente y `canal[1]`. | Escucha, `canal[0]` y todos los IPC heredados de otros hijos. |

El padre identifica al jugador por el canal por el que llega la acción.
El cliente no puede elegir otro ID para jugar en su nombre. El hijo envuelve
las líneas del cliente con `ACCION` y genera los avisos de desconexión por
separado, para que un texto enviado por el cliente no suplante un aviso IPC.

`fork()` hereda una copia de la memoria del padre, pero la rama del hijo
entra únicamente en `puente_cliente()` y finaliza con `_exit()`. Nunca llama
a las funciones del juego. `blackjack.c` solo se enlaza con el servidor;
el cliente no calcula puntajes ni decide resultados.

El sexto cliente también recibe un hijo y un canal: el padre manda
`SERVIDOR_LLENO` por IPC, el hijo lo transmite y cierra TCP. Los lugares se
reservan desde la conexión, incluso antes de recibir UNIRSE. Un cliente tiene
30 segundos para registrarse; después se libera el lugar.

## TCP, select y desconexiones

- `recibir_linea()` mantiene un pequeño buffer por descriptor. Devuelve
  **1** si hay una línea, **2** si falta completar, **0** si se cerró, y **-1**
  si hay error. Conserva fragmentos y no consume bytes de la línea siguiente.
- Usa `recv(..., MSG_DONTWAIT)` para no dejar al master esperando un `\n`.
- Después de `select()` se procesan hasta 32 líneas por canal y vuelta,
  permitiendo atender a otros clientes aunque uno envíe muchos comandos.
- `encolar()` guarda texto pendiente; `vaciar_salida()` llama a `send()` y
  conserva los bytes no enviados. Se vigilan los descriptores de escritura
  solamente cuando hay datos pendientes. No se presupone un envío completo.
- Cada cola está limitada a 32 KiB. Si se llena, se desconecta ese canal.
  También se cierra si los envíos pendientes no avanzan durante 10 segundos.
- El cierre normal permite hasta 3 segundos para vaciar mensajes finales.
- Se emplean `FD_ZERO`, `FD_SET`, `FD_ISSET` y `FD_CLR`. El master conserva un
  conjunto base y pasa una copia a `select()`, que lo modifica.
- Si el hijo detecta cierre TCP, avisa `DESCONECTADO id` y termina. El padre
  también trata el cierre del IPC como desconexión, incluso si el hijo murió
  antes de enviar el aviso.
- `SIGCHLD` ejecuta un manejador que recoge todos los hijos terminados con
  `waitpid(-1, NULL, WNOHANG)`, preservando `errno`. No imprime ni cambia el
  juego desde el manejador.

## Archivos y funciones principales

| Archivo | Responsabilidad |
|---|---|
| `server.c` | Red, `accept`, `fork`, IPC, `select`, registro y publicación de estados. |
| `client.c` | `gethostbyname`, TCP, `select` de teclado y red, y envío de comandos. |
| `blackjack.c` / `blackjack.h` | Estructuras, mazo, puntajes, turnos y resultados. |
| `protocol.c` / `protocol.h` | Recepción de líneas, colas de envío y validación de nombre/puerto. |
| `ui.c` / `ui.h` | Presentación con ncurses a partir de los estados recibidos. |
| `Makefile` | Compilación de los dos programas. |

Funciones para recorrer el código durante una defensa:

1. `main()` en `server.c`: inicialización y bucle `select()` del master.
2. `aceptar_cliente()`: `accept`, `socketpair`, `fork` y cierre de copias.
3. `puente_cliente()`: transporte entre TCP e IPC, sin reglas de Blackjack.
4. `procesar_accion()`: registro y validación del comando y turno en el padre.
5. `pedir_carta()`, `plantarse()` y `siguiente_turno()` en `blackjack.c`.
6. `resolver_dealer()`: puntaje final y resultado contra cada jugador.
7. `difundir()` y `publicar_estado()`: actualizaciones a todos los hijos.
8. `recibir_linea()` y `vaciar_salida()`: límites reales del flujo TCP.
9. `ui_recibir()` y `ui_dibujar()`: lectura y representación del estado.

`difundir()` cumple el papel de `broadcast_to_children()` y `recibir_linea()`
el de `recv_line()`, con los nombres propios en español.

## Relación con los ejemplos del docente

| Ejemplo | Qué se conserva |
|---|---|
| `servinet.c` | IPv4, `socket`, `bind`, `listen`, `accept`, `fork`, `send`, `close`. |
| `clieinet.c` | `gethostbyname`, `struct hostent`, `socket`, `connect`, recepción y cierre. |
| `selectest.c` | `fd_set`, `select` y comprobación de cada descriptor listo. |

Se agregan `socketpair` para el IPC, recepción de líneas parciales y colas
pequeñas para envíos parciales, reglas del juego, limpieza de procesos y
ncurses. No hay hilos, frameworks, servidores web ni bibliotecas de red externas.

## Etapas completadas

| Etapa | Cambio y archivos involucrados |
|---|---|
| 1. Arquitectura | Padre dueño del juego; hijos puente; dos ejecutables. |
| 2. TCP básico | `server.c`, `Makefile`: escucha y aceptación. |
| 3. Fork | `server.c`: hijo por cliente, `SIGCHLD`, `waitpid`. |
| 4. IPC | `server.c`, `protocol.c/h`: `socketpair` y mensajes en ambos sentidos. |
| 5. Select master | `server.c`, `protocol.c/h`: múltiples canales y líneas parciales. |
| 6. Cliente | `client.c`: resolución, conexión y comandos. |
| 7. Varios clientes | `server.c`: cinco lugares, registro y rechazo del sexto. |
| 8. Blackjack | `blackjack.c/h`, `server.c`: reglas, turnos, dealer y estados. |
| 9. Ncurses | `ui.c/h`, `client.c`, `Makefile`: mesa y teclado. |
| 10. Desconexiones | `server.c`, `client.c`: avisos, avance de turno y liberación. |
| 11. Verificación | Reglas, red e interfaz comprobadas antes de simplificar la entrega. |

La entrega contiene la versión final integrada. Los servidores de saludo de
las etapas iniciales fueron reemplazados; las conexiones ahora permanecen
abiertas hasta SALIR, desconexión o cierre del master.

## Compilación con comprobaciones estrictas

```sh
make limpiar
make CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -g'
```

Opcionalmente, una semilla fija permite repetir el reparto en el mismo entorno:

```sh
./servidor 3490 12345
```

Se usa `rand()` para la mezcla académica. La misma semilla puede producir
secuencias distintas entre implementaciones de la biblioteca C.

## Prueba manual con varios usuarios

1. Iniciar el servidor y dos clientes; ambos deben verse en la mesa.
2. Presionar I en uno. Ambos deben ver la misma ronda y el mismo turno.
3. Intentar pedir desde quien no tiene el turno: recibe el aviso de rechazo.
4. Conectar un tercer cliente: debe quedar ESPERANDO y ver la ronda actual.
5. Completar la ronda con H y S. Verificar dealer y resultados.
6. Iniciar otra ronda: el tercer jugador recibe sus cartas.
7. Salir con Q desde el jugador de turno: debe avanzar al siguiente disponible.
8. Abrir cinco clientes e intentar un sexto: debe explicar que la mesa está llena.
9. Cerrar el master: deben cerrarse todas las conexiones.

Con el PID que imprime el master también puede inspeccionarse a sus hijos:

```sh
ps -o pid,ppid,stat,comm --ppid PID_DEL_MASTER
```

Mientras los clientes están conectados habrá hijos activos. Después de cerrar
las conexiones no deben persistir hijos en estado Z.

Para quitar los ejecutables y conservar el código:

```sh
make limpiar
```

La compilación y las pruebas se verificaron en un entorno POSIX MSYS2 sobre
Windows. La ejecución nativa en Linux queda pendiente.
