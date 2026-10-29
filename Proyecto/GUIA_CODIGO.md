# Cómo recorrer y explicar el Blackjack

La prioridad de esta versión es que cada paso pueda leerse y explicarse. Se separaron operaciones, se escribieron condiciones con `if` y `else`, se agregaron llaves y se mantuvo una instrucción por línea. El programa conserva las reglas, los mensajes y la arquitectura de la versión anterior.

## 1. Qué programa hace cada cosa

Se compilan dos programas: `servidor` y `cliente`.

- El padre del servidor guarda la única partida: jugadores, mazo, dealer, ronda y turno.
- Cada hijo del servidor comunica a un cliente con el padre. No reparte ni decide resultados.
- El cliente lee el teclado, envía acciones y muestra lo que informa el servidor.

Los hijos se crean con `fork()`. No se ejecutan a mano y no hay un tercer ejecutable.

## 2. Orden sugerido de lectura

| Orden | Archivo | Qué entender primero |
|---|---|---|
| 1 | `blackjack.h` | Cómo se representan una carta, un jugador y el dealer; estados y resultados. |
| 2 | `blackjack.c` | Cómo se crea el mazo, se cuentan puntos y se desarrolla una ronda. |
| 3 | `server.c` | Cómo el padre recibe una acción y llama a las reglas. |
| 4 | `client.c` | Cómo una tecla se convierte en una acción y cómo llega una respuesta. |
| 5 | `protocol.c` | Cómo se conserva un mensaje que TCP entrega en partes. |
| 6 | `ui.c` | Cómo los datos recibidos se dibujan en la terminal. |

En cada archivo, la ejecución del programa empieza en `main()` cuando ese archivo define el programa principal. Las funciones escritas arriba solo se ejecutan cuando se las llama.

## 3. Seguir una ronda sin pensar todavía en la red

### Preparar el juego

`inicializar_juego()` deja vacíos los jugadores y la mano del dealer. No hay ronda activa y el turno vale -1.

`registrar_jugador()` guarda el nombre y marca la conexión. Si ya hay una ronda activa, el nuevo jugador espera la siguiente.

### Iniciar una ronda

`iniciar_ronda()` realiza los pasos en este orden:

1. Verifica que no haya otra ronda activa y que haya jugadores.
2. Crea las 52 cartas y mezcla el mazo.
3. Limpia las manos y los resultados anteriores.
4. Marca quiénes participan.
5. Reparte una carta a cada jugador y una al dealer, durante dos vueltas.
6. Marca como plantado a quien ya tiene 21.
7. Busca el primer jugador que debe actuar con `siguiente_turno()`.

La primera carta del dealer permanece oculta hasta resolver la ronda.

### Pedir una carta

`pedir_carta(id)` valida el jugador, la ronda, el turno y la mano. Si corresponde, reparte una carta y vuelve a calcular los puntos.

- Menos de 21: el jugador puede seguir actuando.
- Exactamente 21: queda plantado y se avanza el turno.
- Más de 21: queda pasado y se avanza el turno.

No se modifica la mano de los otros jugadores al avanzar de turno.

### Plantarse

`plantarse(id)` hace las mismas comprobaciones de jugador y turno, cambia su estado a plantado y llama a `siguiente_turno()`.

### Resolver el dealer

Cuando no queda un jugador por actuar, `siguiente_turno()` llama a `resolver_dealer()`.

El dealer pide con menos de 17 y se planta con 17 o más. Después se compara su mano con la de cada participante, teniendo en cuenta si alguien se pasó y si tiene blackjack natural.

## 4. Dos funciones que conviene poder explicar

### `calcular_puntaje()`

Recorre la mano. Las figuras suman 10 y los ases empiezan sumando 11. Si el total supera 21, cambia los ases necesarios de 11 a 1: por cada cambio resta 10.

### `repartir()`

Primero comprueba que queden cartas y lugar en la mano. La operación está separada en tres instrucciones:

```c
mano[*cantidad] = mazo[proxima_carta];
(*cantidad)++;
proxima_carta++;
```

1. Copia la próxima carta del mazo a la primera posición libre de la mano.
2. Incrementa la cantidad de cartas de esa mano.
3. Avanza la posición de la próxima carta del mazo.

`cantidad` es un puntero porque la función necesita modificar el contador original, no una copia. Al llamarla se pasa su dirección con `&`.

## 5. Recorrido de una acción completa

Si el jugador presiona H:

1. El cliente reconoce la tecla y prepara el texto `PEDIR` seguido de un salto de línea.
2. Lo envía por TCP a su hijo del servidor.
3. `puente_cliente()` lo identifica como una acción y lo transmite al padre por el canal interno.
4. El padre lo recibe en `atender_canal()` y llama a `procesar_accion()`.
5. `procesar_accion()` comprueba si hay ronda y si es el turno correcto.
6. `pedir_carta()` aplica las reglas sobre la única partida.
7. El padre publica el estado actualizado para los jugadores registrados.
8. Cada hijo transmite ese estado a su cliente.
9. El cliente actualiza la pantalla.

El registro se atiende por separado en `registrar_cliente()`, para no mezclar el ingreso a la mesa con las jugadas.

## 6. Conexión basada en los ejemplos del docente

El servidor conserva esta secuencia:

```text
socket -> preparar sockaddr_in -> bind -> listen -> accept -> fork
```

La llamada a `socketpair()` antes de `fork()` agrega el canal interno entre padre e hijo. El padre conserva un extremo y el hijo conserva el otro.

El cliente conserva esta secuencia:

```text
gethostbyname -> socket -> preparar sockaddr_in -> connect
```

En `server.c` y `client.c` hay comentarios numerados junto a estos pasos. La interfaz de terminal no cambia cómo se establece TCP.

## 7. Por qué se conserva el archivo de comunicación

TCP puede entregar una línea en varias partes o varias líneas juntas. `recibir_linea()` guarda los caracteres hasta encontrar el salto de línea.

| Resultado | Significado |
|---|---|
| 1 | Se recibió una línea completa. |
| 2 | Todavía falta completar la línea; se podrá continuar después. |
| 0 | Se cerró la conexión sin una línea pendiente. |
| -1 | Hubo un error. |

`encolar()` guarda texto por enviar. `vaciar_salida()` envía lo que el socket admite y conserva el resto. Este mecanismo permite que un cliente lento no detenga a toda la mesa.

Estas funciones se mantienen separadas de las reglas del Blackjack para poder explicar primero la partida y después el transporte.

## 8. Comparación y comprobaciones de esta revisión

Se guardó una copia del código anterior en la carpeta hermana `Proyecto_respaldo_antes_legibilidad_2026-10-28`.

La revisión elimina operadores ternarios, incrementos mezclados con accesos a arreglos, comparaciones de cadenas abreviadas y las instrucciones `continue` y `goto` de las fuentes del programa. Se conserva `break` para salir de un ciclo y `return` para finalizar una función.

Se compilaron los dos programas con advertencias estrictas tratadas como errores. Pasaron las dos pruebas existentes de reparto y turnos. También se compararon 3000 rondas con la versión anterior: coincidieron cartas, estados, turnos y resultados.

La prueba de red comparó ambas versiones y comprobó registro, órdenes TCP fragmentadas, rechazo de jugadas fuera de turno, carta oculta, ingreso durante una ronda, rechazo del sexto cliente y desconexión del jugador de turno.

Las comprobaciones se realizaron con el entorno POSIX de MSYS2 en Windows. Queda pendiente repetir la ejecución nativa en Linux.
