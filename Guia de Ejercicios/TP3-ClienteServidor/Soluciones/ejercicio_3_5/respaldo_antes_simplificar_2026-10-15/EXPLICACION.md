# Explicación de las soluciones

## Cómo se usó el material del docente

Se tomaron las consignas de [TP3.pdf](../TP3.pdf), páginas impresas 11 y 12, y se revisaron las cuatro presentaciones y los seis programas proporcionados.

| Material | Qué aporta a la como resolución |
| --- | --- |
| `ejemplo10 - copia.c` | Servidor TCP con `socket`, `bind`, `listen`, `accept`, `send` y `close`. Es la base de 3.1. |
| `ejemplo11 - copia.c` y `clieinet.c - copia.txt` | Cliente TCP: argumento con el host, `gethostbyname`, `connect` y `recv`. |
| `servinet.c - copia.txt` | Un `fork()` por conexión. El hijo atiende y el padre cierra su copia del socket conectado. Base de 3.3, 3.4 y 3.5. |
| `udpser.c - copia.txt` | Socket `SOCK_DGRAM`, asociación del puerto con `bind()` y recepción con `recvfrom()`. |
| `udpcli.c - copia.txt` | Dirección del servidor y envío de datagramas con `sendto()`. |
| `09 - Sistemas Operativos - Linux - Clase 1.pdf` | Procesos, retorno de `fork()`, señales, archivos heredados y estado zombie. Páginas PDF 20, 25 y 39 a 46. |
| `10 - Sistemas Operativos - Linux - Clase 2.pdf` | Colas, semáforos y memoria compartida. Se revisaron también las páginas con imágenes. Estos mecanismos no hacen falta en estos ejercicios: el padre puede llevar su propio contador de hijos. |
| `11 - Redes de datos - Clase 1.pdf` | Puertos, establecimiento y cierre TCP y diferencia con UDP. Páginas PDF 59 a 71. |
| `12 - Redes de datos - Clase 2.pdf` | Estructura `sockaddr_in` y secuencia de llamadas del cliente y del servidor, para TCP y UDP. Páginas PDF 2 a 16. |

Los ejemplos son la base del estilo, y las consignas del TP3 definen los requisitos. Se agregaron `select()` y el control de hijos para cumplir las partes que no están resueltas en los ejemplos. Las presentaciones entregadas no contienen un desarrollo de `select()`.

No se usan hilos, clases, memoria dinámica, bibliotecas externas ni un sistema de funciones genéricas. En 3.4 y 3.5 solamente se separó la atención del hijo en `atender_cliente()` para que el recorrido del padre sea fácil de seguir.

Se hicieron ajustes pequeños para que los programas sean correctos al compilarlos y ejecutarlos: declarar los encabezados necesarios, inicializar las estructuras, cargar el tamaño antes de `accept()`, reservar los límites de los buffers y cerrar los descriptores que cada proceso no necesita.

## Elementos que se repiten

### La dirección del socket

```c
struct sockaddr_in my_addr;

memset(&my_addr, 0, sizeof(my_addr));
my_addr.sin_family = AF_INET;
my_addr.sin_port = htons(MYPORT);
my_addr.sin_addr.s_addr = INADDR_ANY;
```

- `AF_INET`: se utiliza IPv4.
- `sin_port`: contiene el puerto. `htons()` lo convierte al orden de bytes de la red.
- `INADDR_ANY`: el servidor acepta tráfico destinado a sus direcciones IPv4 locales.
- `memset()`: inicializa los campos de la estructura en cero.

En el cliente se coloca la dirección del servidor en lugar de `INADDR_ANY`. `gethostbyname()` se conserva porque aparece en los ejemplos. Se usa `h_addr_list[0]` para tomar la primera dirección IPv4 devuelta.

### Los dos sockets TCP del servidor

`sockfd` es el socket que escucha conexiones. Se crea una vez y se usa en `bind()`, `listen()` y `accept()`.

`new_fd` es el socket de una conexión particular. Lo devuelve `accept()` y sirve para enviar o recibir datos de ese cliente.

`listen(sockfd, 10)` define la cola de conexiones pendientes. No significa que existan diez procesos hijos ni limita por sí mismo cuántos clientes puede atender un servidor concurrente. En 3.5, esa cantidad se controla aparte con `MAX_HIJOS` y `hijos`.

### Por qué hay algunas repeticiones de envío y recepción

TCP entrega un flujo de bytes: una llamada a `recv()` puede recibir solo una parte de lo enviado. Por eso el cliente 3.1 lee hasta el salto de línea y el servidor 3.4 acumula los tres bytes de `FIN`.

El puerto UDP tiene un formato fijo de dos bytes. El cliente lo recibe con `MSG_WAITALL`, comprueba que recibió los dos y aplica `ntohs()`.

Cuando se envían cadenas por TCP se repite `send()` si faltó una parte. `MSG_NOSIGNAL` evita que una escritura a un cliente que se desconectó termine al servidor por `SIGPIPE`; en ese caso se puede detectar el error de `send()`.

## Ejercicio 3.1: un cliente por vez

El servidor realiza esta secuencia:

1. Crea el socket TCP y lo asocia al puerto 8024.
2. Habilita la escucha y espera en `accept()`.
3. Envía `Conexión aceptada` y un salto de línea.
4. Espera 60 segundos con `sleep(60)`.
5. Cierra esa conexión y vuelve a `accept()`.

El salto de línea permite al cliente reconocer el final de la cadena sin esperar al cierre del servidor. El cliente imprime el mensaje y cierra inmediatamente: no espera los 60 segundos.

Mientras el servidor duerme, no vuelve a ejecutar `accept()`. Un segundo cliente puede completar su `connect()` porque el sistema operativo tiene una cola de conexiones. Sin embargo, su `recv()` queda esperando el mensaje hasta que el servidor vuelva a atenderlo. Esto es lo que se estudia en 3.2.

No se instala un manejador para Ctrl+C: la acción habitual de `SIGINT` termina el programa, y el sistema libera sus sockets.

## Ejercicio 3.2: conexión TCP y atención del programa

La secuencia de terminales y el informe de la experiencia están en [observaciones.md](ejercicio_3_2/observaciones.md). La idea principal es que completar la conexión TCP y ser atendido por el código del servidor son momentos distintos.

## Ejercicio 3.3: un hijo por cliente

La preparación del socket es igual a 3.1. La diferencia aparece después de `accept()`:

```c
pid = fork();
if (pid == 0)
{
    close(sockfd);
    /* Aqui el hijo envia el mensaje y espera 60 segundos. */
    close(new_fd);
    return 0;
}
close(new_fd);
```

Este fragmento es un esquema: en el archivo real están escritas las instrucciones de envío y demora dentro del hijo.

`fork()` devuelve 0 en el hijo, el PID del hijo en el padre y -1 si falla. Cada proceso continúa desde esa llamada con sus propias variables.

El hijo cierra el socket de escucha, envía el mensaje, espera 60 segundos, cierra su conexión y termina. El padre cierra su copia de `new_fd` y vuelve enseguida a `accept()`. Por eso el siguiente cliente recibe su mensaje aunque el primer hijo esté durmiendo.

Para este ejercicio basta `signal(SIGCHLD, SIG_IGN)`: en Linux permite que no queden hijos zombie y no necesitamos contarlos. Las observaciones están en [el informe de 3.3](ejercicio_3_3/observaciones.md).

## Ejercicio 3.4: TCP para controlar, UDP para recibir datos

El padre escucha en TCP 3456. Cada `accept()` correcto se sigue de un `fork()`. El padre vuelve a esperar; el hijo ejecuta `atender_cliente(new_fd)`.

### Buscar un puerto libre

El hijo crea un socket UDP y prueba `bind()` desde 10000. Si devuelve `EADDRINUSE`, ese puerto está ocupado y se prueba el siguiente. Si `bind()` funciona, ese socket ya tiene reservado el puerto. No se hace una comprobación separada que pudiera dejar un hueco entre consultar y reservar.

No se activa `SO_REUSEADDR` en los sockets UDP: cada hijo debe tener un puerto distinto. En cambio, sí se utiliza en el socket TCP de escucha para facilitar el reinicio del servidor.

El hijo convierte el número con `htons()` y lo envía por el TCP heredado. El cliente conoce así el destino de sus próximos datagramas.

### Esperar TCP y UDP al mismo tiempo

Si el hijo se quedara bloqueado solamente en `recvfrom()`, no podría enterarse de que llegó `FIN` por TCP. `select()` permite esperar ambos sockets:

```c
FD_ZERO(&lectura);
FD_SET(udpfd, &lectura);
FD_SET(new_fd, &lectura);
select(mayor_fd + 1, &lectura, NULL, NULL, NULL);
```

`FD_ZERO()` vacía el conjunto y `FD_SET()` incorpora cada descriptor. El primer argumento es el descriptor más grande más uno. Aquí el último argumento es `NULL`: no hay tiempo máximo de espera.

Después de `select()`, `FD_ISSET()` indica cuál tiene algo para leer. Si es UDP, se recibe y se escribe en el archivo del hijo. Si es TCP, se lee la orden. No se presupone que `FIN` llegue completo en una única lectura.

### El archivo y el fin del hijo

Se crea `hijo_PID.txt` en la carpeta desde la que se ejecutó el servidor. El PID diferencia los archivos de los hijos de esa ejecución. `fwrite(buf, 1, numbytes, archivo)` guarda exactamente los bytes recibidos; no necesita que el datagrama termine con `\0`.

Al recibir `FIN`, el hijo cierra el archivo y sus dos sockets, y termina. También libera los recursos si el cliente cierra TCP sin enviar la orden, para evitar que un cliente desaparecido deje un hijo esperando para siempre.

La consigna solo pide el servidor. Se agregó un cliente de teclado para probarlo: manda cada línea por UDP y la palabra `FIN` por TCP.

UDP y TCP son canales distintos. El envío de `FIN` no confirma que todos los datagramas anteriores hayan llegado: UDP puede perderse o demorarse y no existe un orden global entre ambos protocolos. Para la prueba manual se pueden observar los avisos de escritura antes de terminar. Este ejercicio no implementa confirmaciones ni garantiza una transferencia íntegra de archivos.

## Ejercicio 3.5: logs y tiempos de espera

Se conserva la estructura de 3.4, con estos cambios:

| Requisito | Resolución |
| --- | --- |
| Escucha inicial | TCP 5024. |
| Máximo de hijos | Contador del padre y constante `MAX_HIJOS`, inicialmente 10. |
| Puerto UDP de cada hijo | Primer puerto libre desde 10000, reservado con `bind()`. |
| Uso de TCP | Informar el puerto; luego se cierra. |
| Destino de los datos | Pantalla del servidor, identificando hijo y puerto. |
| Espera del hijo | `select()` con intervalos de 30 segundos. |
| Fin del hijo | 120 segundos continuos sin recibir datos. |
| Fin del padre | 60 segundos sin hijos y sin conexiones entrantes. |
| Cliente | `sender archivo.txt`; bloques de hasta 50 bytes, uno cada 30 segundos. |

### La espera del hijo

El hijo mantiene `segundos_sin_datos`, inicialmente cero. Antes de cada `select()` vuelve a preparar el conjunto y el timeout: esas estructuras pueden ser modificadas por la llamada.

Si recibe un datagrama, lo muestra y vuelve a poner el contador en cero. Si pasan 30 segundos sin datos, suma 30. Cuatro esperas completas sin ningún datagrama producen 120 segundos y el hijo termina.

No se suman silencios separados por mensajes. Por ejemplo, si un mensaje llega tras 90 segundos de silencio, se reinicia la espera: el hijo no debe morir 30 segundos después.

### El contador de hijos del padre

Solo el padre modifica `hijos`: lo incrementa cuando crea un hijo y lo decrementa por cada hijo terminado que recoge con:

```c
while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    hijos--;
```

`-1` permite recoger cualquier hijo. `WNOHANG` evita bloquearse si todavía están ejecutándose. El bucle recoge todos los que terminaron, aunque hayan finalizado casi al mismo tiempo.

En este ejercicio no se ignora `SIGCHLD`: necesitamos recuperar el estado de salida con `waitpid()` para actualizar el contador. El hijo tampoco modifica ese contador: después de `fork()` su variable sería una copia, no la del padre.

Si ya hay diez hijos activos, se informa puerto 0 y se cierra esa conexión. No se crea un hijo número once. Los lugares se recuperan cuando terminan los hijos anteriores.

### El minuto sin actividad del padre

El padre usa `select()` con un timeout de un segundo para poder revisar periódicamente los hijos terminados.

- Mientras quedan hijos, el contador de inactividad del padre se mantiene en cero.
- Sin hijos, cada segundo de espera sin conexiones incrementa el contador.
- Una conexión entrante vuelve a ponerlo en cero.
- Al llegar a 60, el padre cierra el socket y termina.

El minuto no empieza cuando se fue el último cliente TCP: empieza cuando ya no queda ningún hijo. Un cliente puede cerrar TCP mientras su hijo sigue esperando datos UDP.

Por la revisión periódica, el padre puede detectar la salida del último hijo hasta aproximadamente un segundo más tarde. Se priorizó que el código sea fácil de seguir y no necesite un manejador de señales ni comunicación adicional entre procesos.

### El cliente sender

La dirección del servidor se define en `HOST` para respetar la ejecución con un solo argumento, el archivo.

El cliente comprueba que puede abrir el archivo, establece TCP y recibe los dos bytes del puerto UDP. Después cierra TCP, abre el archivo para recorrerlo y crea los datagramas:

```c
cantidad = fread(buf, 1, BLOQUE, archivo);
```

`cantidad` es el número real de bytes leídos, entre 0 y 50. Se envían esos bytes, de modo que el último bloque puede tener menos de 50.

El primer bloque sale inmediatamente. Antes de cada envío siguiente se espera con `sleep(30)`. Si ya no hay más bloques, el cliente termina sin agregar una espera final. Un archivo vacío no produce datagramas.

### Ejemplo de tiempos

Para el archivo de ejemplo de 131 bytes, suponiendo que llegan los tres datagramas y no hay otros clientes:

| Tiempo aproximado | Suceso |
| --- | --- |
| 0 s | Se asigna el puerto y se envían los primeros 50 bytes. |
| 30 s | Se envían otros 50 bytes. |
| 60 s | Se envían los últimos 31 bytes. Termina `sender`. |
| 90, 120 y 150 s | El hijo informa 30, 60 y 90 segundos sin datos. |
| 180 s | El hijo completa 120 segundos sin datos y termina. |
| 240 s | El padre completa el minuto sin hijos ni conexiones y termina. |

Son tiempos aproximados: dependen de la planificación de los procesos y, para el padre, de la revisión cada segundo.
