# Ejercicio 3.3: servidor concurrente

## Procedimiento

Detener el servidor de 3.1, porque ambos usan TCP 8024. Ejecutar el servidor de esta carpeta y abrir tres clientes de 3.1 en terminales distintas, sin esperar 60 segundos entre ellos.

Desde una terminal adicional:

```bash
netstat -antp | grep ':8024 '
```

Para ver el padre y los hijos también se puede utilizar:

```bash
ps -o pid,ppid,stat,comm -C servidor
```

El filtro de `ps` corresponde al nombre del ejecutable indicado en las instrucciones de compilación.

## Resultado observado

En la prueba local se ejecutaron tres clientes como procesos separados. Los tres recibieron `Conexión aceptada` y terminaron en aproximadamente 0,003 segundos en total. Ese valor es una medición de esta máquina: lo relevante es que ninguno esperó los 60 segundos de otro cliente.

Mientras tanto quedaron tres hijos del servidor ejecutando su demora de 60 segundos. Después terminaron los tres sin dejar procesos zombie.

La [captura_netstat.txt](captura_netstat.txt) muestra la escucha del padre y las conexiones asociadas a PID de hijos diferentes. También pueden aparecer filas `TIME_WAIT` de pruebas anteriores.

Como los clientes cierran apenas reciben la cadena, los sockets de los hijos que siguen durmiendo pueden verse en `CLOSE_WAIT`, y los extremos correspondientes a los clientes en `FIN_WAIT2`. Son estados esperables en esta consigna.

## Comparación con 3.1 y 3.2

| Aspecto | No concurrente | Concurrente |
| --- | --- | --- |
| Quién atiende | El único proceso del servidor. | Un hijo diferente por cliente. |
| Quién espera los 60 segundos | El mismo proceso que debería aceptar nuevos clientes. | El hijo que ya atendió a ese cliente. |
| Padre disponible para `accept()` | Después de terminar la demora. | Inmediatamente después de crear el hijo. |
| Segundo cliente | Puede conectar, pero espera el mensaje. | Recibe el mensaje sin esperar la demora del primero. |
| Tres clientes casi simultáneos | Respuestas aproximadamente a los 0, 60 y 120 s. | Las tres respuestas llegan enseguida. |

## Explicación para el informe

La concurrencia se obtiene creando un proceso hijo después de cada conexión aceptada. El hijo envía la cadena, espera 60 segundos y cierra la conexión. El padre cierra su copia del socket conectado y vuelve a aceptar clientes.

La demora sigue existiendo, pero cada hijo realiza la suya de manera independiente. Por eso varios clientes pueden ser atendidos mientras otros hijos permanecen esperando. El padre conserva el socket de escucha; cada hijo conserva únicamente el socket de su cliente.

Todos utilizan el puerto local TCP 8024. Las conexiones se distinguen por los pares de direcciones IP y puertos de ambos extremos: no es necesario asignar un puerto TCP de escucha distinto a cada hijo.
