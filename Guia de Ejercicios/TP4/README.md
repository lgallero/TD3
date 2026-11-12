# TP 4 · Sistemas Operativos Multitasking

Soluciones de los ejercicios 4.1 a 4.6 de la guía 2026. Los comandos se ejecutan desde esta carpeta en Linux x86. Cada ejercicio tiene su propio ejecutable y se puede compilar desde su carpeta usando solamente los archivos C de esa carpeta.

## Compilar

```sh
gcc -std=c11 -Wall -Wextra 4.1/tp41.c -o tp41
gcc -std=c11 -Wall -Wextra 4.2/tp42.c -o tp42
gcc -std=c11 -Wall -Wextra 4.3/tp43.c -o tp43
gcc -std=c11 -Wall -Wextra 4.4/tp44.c -o tp44
gcc -std=c11 -Wall -Wextra 4.5/tp45.c 4.5/rutinas.c -o tp45
gcc -std=c23 -Wall -Wextra -pedantic-errors -msse2 4.6/tp46.c 4.6/rutinas.c -o tp46
```

## 4.1 · Enviar una señal al hijo

Ejecutá `./tp41`, escribí `S` y presioná Enter. El padre manda `SIGUSR1`. El hijo muestra que la recibió y el padre lo recoge con `waitpid()`. El bloqueo inicial de la señal permite enviar la S incluso antes de que el hijo termine de instalar su manejador.

## 4.2 · Crear hijos por señal

Ejecutá `./tp42 2`. Desde otra terminal, usá `kill -USR1 PID_DEL_PADRE` para solicitar un hijo. Podés repetirlo hasta llegar al máximo de dos hijos vivos. Cada hijo muestra su PID y termina a los 60 segundos. El padre atiende `SIGCHLD` y recoge a los que terminaron. Ctrl+C deja el programa corriendo. Para terminar el padre, esperá a que salgan los hijos y usá `kill -TERM PID_DEL_PADRE`.

Enviá una solicitud por vez cuando hagas la prueba. Linux puede combinar varios `SIGUSR1` iguales si todavía están pendientes.

## 4.3 · Mostrar los datos de cada proceso

Ejecutá `./tp43 2` y solicitá los hijos con `kill -USR1 PID_DEL_PADRE`. Cada hijo informa sus datos al arrancar y cada 10 segundos. Para el campo `GroupId` se usa el grupo de usuario real que devuelve `getgid()`, la función indicada en la guía. `Nchilds` es la copia que recibió el hijo cuando se hizo `fork()`, por lo que el primero muestra 1 y el segundo, 2 si el primero sigue activo.

Para cerrar, enviá `kill -KILL PID_DEL_HIJO` a cada hijo y luego `kill -KILL PID_DEL_PADRE`. Así el padre puede recogerlos antes de que lo termines.

## 4.4 · Esperar por teclado y FIFO

Ejecutá `./tp44`. Lo que escribas en la primera terminal aparece con `[KEYB]:`. En otra terminal podés enviar un mensaje a la FIFO con este comando.

```sh
printf 'Mensaje de prueba\n' > /tmp/tdiii_pipe
```

El programa muestra `[PIPE]: Mensaje de prueba`. La FIFO acepta mensajes terminados en salto de línea o en el byte nulo que usan algunos ejemplos de clase. Podés cerrar el programa con Ctrl+C.

## 4.5 y 4.6 · Captura y procesamiento de audio

```sh
mkdir -p salida45 salida46
./tp45 entrada.wav salida45
./tp46 entrada.wav salida46
```

El WAV debe ser PCM estéreo de 8 bits a 20 000 muestras por segundo por canal. En Linux con OSS, se puede usar `/dev/dsp` en lugar de `entrada.wav` y terminar la captura con Ctrl+C. Si omitís el directorio de salida, se usa `/home/tdiii`. El directorio debe existir y ser escribible. Cada ejecución empieza desde cero los tres archivos.

Cada carpeta tiene su propio programa principal y sus propias rutinas. El 4.5 enlaza tres rutinas de paso directo para probar la captura y el reparto. El 4.6 tiene su propio programa de captura y coordinación, `4.6/tp46.c`, y enlaza las tres rutinas SIMD de `4.6/rutinas.c`. Se eligió un eco realimentado de 512 pares estéreo, un retardo de 300 ms y un FIR pasa bajos que promedia cuatro muestras consecutivas por canal.

Para el FIR se sigue el mismo cálculo que en `simd.cpp` del Campus: cargar cuatro muestras con `_mm_loadu_ps()`, multiplicarlas por un coeficiente con `_mm_mul_ps()`, sumar con `_mm_add_ps()` y guardar con `_mm_storeu_ps()`. Las cuentas se hacen en `float` y el resultado vuelve a PCM de 8 bits. Los coeficientes de 1/4 conservan el pasa bajos que pide la guía.

En el 4.6, `main()` abre el audio, crea la memoria compartida, inicializa los dos semáforos y da el primer turno de cada bloque. `trabajo_hijo()` reúne los pasos de cada hijo: consultar su turno con `semctl()`, aplicar el efecto, guardar el audio y ceder el turno con `semop()`. `esperar_hijos()` espera la confirmación y vigila el tiempo de cada turno. Las tres rutinas de audio están en `rutinas.c`.

El padre publica bloques de hasta 20 ms en memoria compartida. `TURNO` va de 3 a 0 y da paso a cada hijo. El último hijo incrementa `TERMINADO` antes de que el padre avance al bloque siguiente. El padre espera con `semtimedop()` y revisa con `waitpid(..., WNOHANG)` si terminó un hijo. Si el mismo turno sigue trabado durante 500 ms de espera, lo detiene y lo reemplaza. El reemplazo consulta el tamaño de su archivo y escribe solo la parte pendiente del bloque. Se permiten tres reinicios por hijo y por bloque.

Las salidas `echo`, `delay` y `fir_low_pass` son PCM sin cabecera. Para escuchar una como WAV, podés convertirla con `ffmpeg -f u8 -ar 20000 -ac 2 -i salida46/echo echo.wav`.

## Material de clase utilizado

| Ejercicios | Referencia |
| --- | --- |
| 4.1 a 4.3 | Presentación 09, *Linux, clase 1*, diapositivas 20 y 25 para procesos y `fork()`, y págs. 41–46 del PDF para señales y manejadores. `ejemplo1.c` muestra la creación y la espera de hijos. |
| 4.4 | Presentación 09, pág. 49 del PDF, para la FIFO. Presentación 13, pág. 35, y material del Campus `_Funcion Select.pdf` para esperar en varios descriptores. `ejemplo3_rx.c` y `ejemplo3_tx.c` muestran la FIFO nombrada y `ejemplo9.c` muestra `select()`. |
| 4.5 | Presentación 10, *Linux, clase 2*, págs. 4–5 para semáforos y 14–15 para memoria compartida. `ejemplo7.c` y `ejemplo8.c` muestran esas llamadas. Presentación 13, pág. 36, para `ioctl()`. `ejemplo5_tx.c` muestra `open()`, `ioctl()` y `read()` con `/dev/dsp`. |
| 4.6 | Presentación 07, *Procesamiento Digital de Señales*, págs. 25–26 para SSE en coma flotante y 31–33 para el eco realimentado. `simd.cpp` del Campus muestra la acumulación SIMD del FIR. |

La entrega anterior del 4.6 sirvió de referencia para turnar a los hijos y separar las rutinas de audio. En ese código el control por tiempo de espera quedó comentado y las rutinas usaban longitudes fijas. Esta versión recibe el tamaño efectivo de cada bloque, guarda el historial y recupera el turno cuando se reemplaza a un hijo.


## Verificación

Probé los seis ejercicios en Linux. En el 4.2 comprobé el límite de hijos, la respuesta a Ctrl+C y que el hijo termina y se recoge al cabo de 60 segundos. En el 4.3 comprobé el informe cada 10 segundos y el cierre con SIGKILL. En el 4.4 envié mensajes parciales por la FIFO mientras entraba una línea por teclado.

El 4.5 conservó el PCM de entrada en los tres archivos. El 4.6 dio las salidas esperadas byte por byte en x86 de 32 y 64 bits. También probé un hijo colgado y un hijo interrumpido durante o justo después de escribir, sin duplicación de audio. La captura por `/dev/dsp` se verificó con un dispositivo simulado. Queda pendiente probarla con hardware OSS real.
