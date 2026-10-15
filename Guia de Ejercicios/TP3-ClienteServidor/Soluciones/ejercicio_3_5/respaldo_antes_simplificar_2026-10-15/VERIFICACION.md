# Verificación de las soluciones

Pruebas realizadas en Linux el 14 de octubre de 2026. Se ejecutaron los programas como procesos independientes sobre IPv4 local (`127.0.0.1`). No se realizó una prueba entre computadoras físicas distintas.

## Compilación

Los siete archivos C compilaron sin errores ni advertencias con GCC y estas opciones:

```text
-std=gnu11 -Wall -Wextra -Wpedantic
```

## Pruebas con el código y los tiempos originales

| Ejercicio | Qué se comprobó | Resultado |
| --- | --- | --- |
| 3.1 y 3.2 | Primer cliente inmediato; segundo cliente esperando mientras el servidor duerme. | El segundo recibió la cadena aproximadamente a los 60,00 s. |
| 3.2 | Consulta real de las conexiones con `netstat -antp`. | Se registraron `LISTEN`, el segundo cliente en `ESTABLISHED` y el primer extremo del servidor en `CLOSE_WAIT`. |
| 3.3 | Tres clientes casi simultáneos. | Los tres recibieron la cadena en aproximadamente 0,003 s; quedaron tres hijos realizando su demora. |
| 3.3 | Espera completa de los hijos. | Terminaron tras sus 60 s y no quedaron hijos zombie. |
| 3.4 | Puerto 10000 ocupado por otro socket UDP antes de conectar dos clientes. | Los hijos eligieron 10001 y 10002. |
| 3.4 | Envíos por UDP desde clientes diferentes. | Los archivos separados coincidieron exactamente con los datos enviados en esta prueba. |
| 3.4 | `FIN` dividido en tres envíos TCP, primero `F`, luego `I`, luego `N`. | Se reconoció la orden y terminó el hijo. |
| 3.4 | Cliente auxiliar escrito en C. | Envió una línea, se verificó su archivo y terminó con `FIN`. |
| 3.4 | Cliente que cierra TCP sin enviar `FIN`. | El hijo cerró sus recursos y terminó. |
| 3.5 | Archivo de ejemplo de 131 bytes. | Se enviaron y recibieron bloques de 50, 50 y 31 bytes; el cliente tardó aproximadamente 60,01 s. |

Las capturas de `netstat` se conservan en las carpetas de 3.2 y 3.3. No son salidas inventadas. Los valores medidos dependen de esta máquina y no son tiempos garantizados para otras ejecuciones.

La prueba completa de 3.5 terminó correctamente a los **240,32 segundos**: el hijo cerró tras 120 segundos sin datos desde el último datagrama, y el padre cerró después del minuto sin hijos ni conexiones. La salida real está en [captura_prueba.txt](ejercicio_3_5/captura_prueba.txt).

## Casos adicionales con copias de prueba aceleradas

Para comprobar más situaciones sin esperar varios minutos en cada una, se compilaron copias temporales fuera de la carpeta de soluciones. En esas copias se cambiaron únicamente los puertos de prueba y las constantes de tiempo:

- Intervalo de recepción/envío: de 30 s a 1 s.
- Inactividad del hijo: de 120 s a 4 s.
- Inactividad del padre: de 60 s a 3 s.

Los archivos entregados conservan los valores exigidos por la guía.

| Caso | Resultado |
| --- | --- |
| Diez conexiones activas | Se asignaron diez puertos UDP diferentes y se crearon diez hijos. |
| Cliente número once | Recibió el puerto 0; no se creó otro hijo. |
| Nuevo dato antes del vencimiento | Reinició la inactividad del hijo. |
| Un hijo sigue activo | El padre continuó funcionando aunque ya hubiera transcurrido su tiempo de espera sin conexiones. |
| Terminan hijos anteriores | Se recuperó capacidad para aceptar otro cliente. |
| Conexión antes de vencer la espera del padre | Se aceptó y el servidor continuó; no terminó con el contador anterior. |
| Padre sin ningún cliente desde el inicio | Terminó tras aproximadamente 3,03 s en la copia configurada con 3 s. |
| Puerto TCP fragmentado en dos entregas de un byte | `sender` recibió correctamente el número completo. |

También se verificó el contenido exacto emitido por `sender` con estos archivos:

| Tamaño del archivo | Tamaños de los datagramas |
| --- | --- |
| 0 bytes | Ninguno. |
| 1 byte | 1. |
| 50 bytes | 50. |
| 51 bytes | 50 y 1. |
| 100 bytes | 50 y 50. |
| 131 bytes | 50, 50 y 31. |

En las pruebas aceleradas, los datagramas consecutivos estuvieron separados por al menos aproximadamente un segundo, y no hubo una espera adicional después del último bloque. Con el ejecutable original se comprobó además que faltar el argumento o indicar un archivo inexistente produce un error antes de conectar.

## Alcance

Las comprobaciones de UDP se hicieron sobre la comunicación local y validan los casos observados. No convierten UDP en un protocolo de entrega garantizada. La solución no agrega confirmaciones ni reenvíos que la guía no solicita.
