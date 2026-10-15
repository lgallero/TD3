# Ejercicio 3.2: concepto de conexión aceptada

## Procedimiento

1. Ejecutar el servidor del ejercicio 3.1 en una terminal.
2. Ejecutar un primer cliente en otra terminal: `./ejercicio_3_1/cliente 127.0.0.1`.
3. Antes de que pasen 60 segundos, ejecutar un segundo cliente desde una tercera terminal.
4. Observar las conexiones desde otra terminal:

```bash
netstat -antp | grep ':8024 '
```

Las opciones son: `-a`, mostrar sockets de escucha y conexiones; `-n`, mostrar direcciones y puertos numéricos; `-t`, TCP; `-p`, proceso asociado cuando esté disponible. Para repetir la consulta automáticamente se puede usar:

```bash
watch -n 1 "netstat -antp | grep ':8024 '"
```

Como alternativa, si no está instalado `netstat`, se puede consultar `ss -antp | grep ':8024 '`. La experiencia pedida se hizo con `netstat`.

## Resultado observado en la prueba

La prueba se realizó en una computadora Linux, con el servidor y dos procesos cliente independientes comunicándose por `127.0.0.1`.

- El primer cliente recibió `Conexión aceptada` y terminó inmediatamente.
- El servidor continuó dentro de `sleep(60)` con su socket de esa conexión abierto.
- El segundo cliente mostró que su conexión TCP estaba establecida, pero quedó esperando el mensaje.
- El segundo cliente recibió el mensaje aproximadamente a los 60,00 segundos del inicio de la experiencia.

El archivo [captura_netstat.txt](captura_netstat.txt) contiene las filas registradas mientras el servidor dormía y el segundo cliente esperaba. Los PID y los puertos de origen cambian entre ejecuciones.

## Cómo interpretar los estados

| Estado | Interpretación en esta experiencia |
| --- | --- |
| `LISTEN` | El socket TCP del servidor está habilitado para recibir conexiones en 8024. |
| `ESTABLISHED` | Se completó el establecimiento TCP. El segundo cliente puede estar en este estado aunque todavía no haya sido atendido por `accept()` ni haya recibido la cadena. |
| `CLOSE_WAIT` | El primer cliente ya cerró su extremo. El servidor recibió ese cierre, pero mantiene abierto el suyo durante la demora. |
| `FIN_WAIT2` | El primer cliente inició el cierre y espera el cierre del extremo del servidor. El proceso cliente ya puede haber terminado. |
| `TIME_WAIT` | Puede observarse después del cierre completo. Es un estado temporal mantenido por TCP y no implica que el programa cliente siga ejecutándose. |

En Linux, el `Recv-Q` de la fila `LISTEN` puede mostrar conexiones completas pendientes de que el programa las extraiga con `accept()`. En la captura aparece 1: corresponde al segundo cliente.

## Explicación para el informe

El servidor es no concurrente porque utiliza un único proceso para aceptar conexiones, enviar la respuesta y esperar los 60 segundos. Durante esa demora no vuelve a ejecutar `accept()`.

El sistema operativo puede completar el establecimiento TCP de otros clientes y dejarlos en la cola de conexiones pendientes. Por eso un cliente puede salir de `connect()` y aparecer como `ESTABLISHED` aunque el servidor todavía no lo haya atendido. En ese caso queda bloqueado en `recv()`, esperando el mensaje del programa.

Al finalizar la demora, el servidor cierra la primera conexión y vuelve a `accept()`. Entonces extrae la siguiente conexión pendiente y envía la cadena. Si se lanzan tres clientes casi al mismo tiempo y entran en ese orden, reciben el mensaje aproximadamente a los 0, 60 y 120 segundos.

Los 60 segundos no demoran la finalización del cliente que ya recibió la cadena: demoran la atención del siguiente. Si se llena la cola, las conexiones adicionales pueden esperar, retransmitirse o fallar según la situación; no corresponde afirmar que todos los clientes siempre completarán `connect()` inmediatamente.
