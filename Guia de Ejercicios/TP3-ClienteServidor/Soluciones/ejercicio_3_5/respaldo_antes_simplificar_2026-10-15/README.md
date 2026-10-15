# Guía 3: soluciones en C

Resolución de los ejercicios 3.1 a 3.5 para Linux, basada en los ejemplos y las presentaciones de la carpeta del TP3. Cada programa se puede leer y compilar por separado. Los originales se conservaron sin cambios.

El código sigue la secuencia del docente: crear el socket, completar `sockaddr_in`, asociar el puerto, esperar o iniciar la conexión, intercambiar datos y cerrar. Los servidores concurrentes usan `fork()`. Los comentarios y el manejo de errores están escritos de manera explícita.

## Qué contiene cada carpeta

| Ejercicio | Archivos | Qué resuelve |
| --- | --- | --- |
| 3.1 | `ejercicio_3_1/servidor.c`, `cliente.c` | TCP no concurrente, puerto 8024 y demora de 60 s. |
| 3.2 | `ejercicio_3_2/observaciones.md` | Experiencia con varios clientes y explicación de `netstat`. |
| 3.3 | `ejercicio_3_3/servidor.c`, `observaciones.md` | Versión concurrente con `fork()`. Usa el cliente de 3.1. |
| 3.4 | `ejercicio_3_4/servidor.c`, `cliente.c` | TCP 3456, puerto UDP por hijo, archivos separados y orden `FIN`. El cliente es auxiliar para probar. |
| 3.5 | `ejercicio_3_5/servidor.c`, `sender.c`, `logs_ejemplo.txt` | Entrega obligatoria: hasta 10 hijos, bloques de 50 bytes, intervalos y cierres por inactividad. |

La explicación del código y la relación con los ejemplos están en [EXPLICACION.md](EXPLICACION.md). Los resultados de las pruebas están en [VERIFICACION.md](VERIFICACION.md).

## Compilar y ejecutar

Abrir una terminal en esta carpeta `Soluciones`. Se necesita Linux y GCC. Compilar una vez y ejecutar servidor y cliente en terminales distintas.

### Ejercicios 3.1 y 3.2

Compilar:

```bash
gcc -Wall -Wextra ejercicio_3_1/servidor.c -o ejercicio_3_1/servidor
gcc -Wall -Wextra ejercicio_3_1/cliente.c -o ejercicio_3_1/cliente
```

Terminal del servidor:

```bash
./ejercicio_3_1/servidor
```

Terminal de cada cliente:

```bash
./ejercicio_3_1/cliente 127.0.0.1
```

Para observar las conexiones desde otra terminal:

```bash
netstat -antp | grep ':8024 '
```

El servidor se detiene con Ctrl+C. El cliente termina apenas recibe el mensaje completo. Para ver la espera del segundo cliente, ejecutarlo mientras el servidor está en los 60 segundos de demora del primero.

### Ejercicio 3.3

Detener primero el servidor de 3.1: ambos usan el mismo puerto TCP.

```bash
gcc -Wall -Wextra ejercicio_3_3/servidor.c -o ejercicio_3_3/servidor
./ejercicio_3_3/servidor
```

En otras terminales, usar el mismo cliente:

```bash
./ejercicio_3_1/cliente 127.0.0.1
```

Consultar nuevamente `netstat -antp | grep ':8024 '`. Los mensajes se reciben sin esperar la demora de los otros clientes. Ctrl+C en la terminal del servidor detiene el padre y sus hijos que estén en el mismo grupo de terminal.

### Ejercicio 3.4

```bash
gcc -Wall -Wextra ejercicio_3_4/servidor.c -o ejercicio_3_4/servidor
gcc -Wall -Wextra ejercicio_3_4/cliente.c -o ejercicio_3_4/cliente
```

En la terminal del servidor, entrar en su carpeta para guardar allí los archivos:

```bash
cd ejercicio_3_4
./servidor
```

En otra terminal abierta en `Soluciones`:

```bash
./ejercicio_3_4/cliente 127.0.0.1
```

Escribir un mensaje y pulsar Enter. Se envía por UDP y se guarda en `hijo_PID.txt`. Repetir con otros mensajes. Escribir `FIN` en una línea para terminar ese hijo. El padre sigue aceptando conexiones hasta Ctrl+C.

Cada cliente tiene un puerto UDP y un archivo propios. El primer puerto libre se busca desde 10000. Si otro proceso ya lo usa, se prueba 10001 y así sucesivamente.

### Ejercicio 3.5: entrega obligatoria

```bash
gcc -Wall -Wextra ejercicio_3_5/servidor.c -o ejercicio_3_5/servidor
gcc -Wall -Wextra ejercicio_3_5/sender.c -o ejercicio_3_5/sender
```

Terminal del servidor:

```bash
./ejercicio_3_5/servidor
```

Terminal del cliente:

```bash
./ejercicio_3_5/sender ejercicio_3_5/logs_ejemplo.txt
```

El archivo de ejemplo tiene 131 bytes. Se envían 50 bytes inmediatamente, otros 50 aproximadamente a los 30 segundos y los últimos 31 aproximadamente a los 60 segundos. El cliente termina después del último envío.

El hijo sigue esperando mensajes: termina a los 120 segundos del último datagrama recibido. Cuando ya no quedan hijos, el padre espera 60 segundos sin conexiones nuevas y termina. En esta prueba completa, el servidor dura aproximadamente 240 segundos desde el primer envío.

Para ver TCP y UDP:

```bash
netstat -antp | grep ':5024 '
netstat -anup | grep -E ':100[0-9][0-9] '
```

Ese filtro UDP muestra solo el rango 10000 a 10099; el programa puede usar puertos superiores si esos están ocupados. TCP puede desaparecer enseguida del listado porque aquí solo se utiliza para comunicar el puerto UDP.

## Probar entre dos computadoras

En 3.1, 3.3 y 3.4, reemplazar `127.0.0.1` en el comando del cliente por la dirección IPv4 del servidor.

En 3.5 se mantiene la forma pedida `sender archivo.txt`: cambiar `HOST` en `sender.c` por la dirección IPv4 del servidor y recompilar. El servidor escucha en sus direcciones IPv4 locales mediante `INADDR_ANY`. La red debe permitir el puerto TCP del ejercicio y los puertos UDP asignados desde 10000.

## Decisiones sencillas de esta solución

- El puerto UDP se informa como un entero de 2 bytes, con `htons()` y `ntohs()`. El cliente y el servidor usan el mismo formato.
- En 3.5, el puerto informado vale 0 cuando ya hay 10 hijos. El cliente muestra que el servidor está ocupado y termina. La consigna no fija cómo avisar este caso.
- El padre de 3.5 revisa los hijos terminados cada segundo con `waitpid()`. Esto evita un manejador de señales y permite contar los hijos en el mismo proceso padre. Puede detectar la salida del último hijo hasta aproximadamente un segundo después.
- No se agregan reintentos ni confirmaciones a UDP. Los ejercicios muestran el envío de datagramas; UDP no garantiza su recepción ni su orden.
