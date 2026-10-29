/* Reglas del Blackjack simplificado. No contiene sockets ni interfaz.
   Solo el padre modifica el juego.
   Recorrido: iniciar_ronda -> pedir_carta/plantarse -> siguiente_turno -> resolver_dealer. */
#include "blackjack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Carta mazo[52];
Jugador jugadores[MAX_JUGADORES];
Dealer dealer;
int proxima_carta, turno_actual, estado_ronda, numero_ronda;

void inicializar_juego(void)
{
    memset(jugadores, 0, sizeof(jugadores));
    memset(&dealer, 0, sizeof(dealer));
    proxima_carta = 0;
    numero_ronda = 0;
    turno_actual = -1;
    estado_ronda = SIN_RONDA;
}

void registrar_jugador(int id, const char *nombre)
{
    memset(&jugadores[id], 0, sizeof(jugadores[id]));
    jugadores[id].id = id;
    snprintf(jugadores[id].nombre, sizeof(jugadores[id].nombre), "%s", nombre);
    jugadores[id].conectado = 1;
    if (estado_ronda == RONDA_ACTIVA)
    {
        jugadores[id].estado = JUGADOR_ESPERANDO;
    }
    else
    {
        jugadores[id].estado = JUGADOR_LISTO;
    }
}

void crear_mazo(void)
{
    int palo, valor, indice = 0;
    for (palo = 0; palo < 4; palo++)
    {
        for (valor = 1; valor <= 13; valor++)
        {
            mazo[indice].valor = valor;
            mazo[indice].palo = palo;
            indice++;
        }
    }
    proxima_carta = 0;
}

void mezclar_mazo(void)
{
    int i, j;
    Carta auxiliar;
    /* Intercambiar cada carta con una de las posiciones que aun quedan. */
    for (i = 51; i > 0; i--)
    {
        j = rand() % (i + 1);
        auxiliar = mazo[i];
        mazo[i] = mazo[j];
        mazo[j] = auxiliar;
    }
}

int calcular_puntaje(const Carta *mano, int cantidad)
{
    int i, total = 0, ases = 0;
    for (i = 0; i < cantidad; i++)
    {
        if (mano[i].valor == 1)
        {
            total += 11;
            ases++;
        }
        else if (mano[i].valor > 10)
        {
            total += 10;
        }
        else
        {
            total += mano[i].valor;
        }
    }
    /* Cambiar un as de 11 a 1 reduce el total en 10. */
    while (total > 21 && ases > 0)
    {
        total -= 10;
        ases--;
    }
    return total;
}

int es_blackjack(const Carta *mano, int cantidad)
{
    int puntos;

    if (cantidad == 2)
    {
        puntos = calcular_puntaje(mano, cantidad);

        if (puntos == 21)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

static int repartir(Carta *mano, int *cantidad)
{
    if (proxima_carta >= 52 || *cantidad >= MAX_CARTAS) // Error: mazo agotado o maximo de cartas por mano alcanzado.
    {
        return -1;
    }
    /* Primero se copia la carta; despues se avanzan los dos contadores. */
    mano[*cantidad] = mazo[proxima_carta];
    (*cantidad)++;
    proxima_carta++;
    return 0;
}

static void preparar_esperando(void)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && jugadores[i].estado == JUGADOR_ESPERANDO)
        {
            jugadores[i].estado = JUGADOR_LISTO;
        }
    }
}

/* Anula la ronda si falla el reparto: mazo agotado o mano sin espacio para otra carta. */
static void anular_ronda(void)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && jugadores[i].participa)
        {
            jugadores[i].resultado = ANULADA;
        }
    }
    turno_actual = -1;
    estado_ronda = RONDA_TERMINADA;
    preparar_esperando();
}

/* El dealer juega una sola vez, despues de los jugadores. */
static void resolver_dealer(void)
{
    int i;
    int puntos;
    int puntos_dealer;
    int hay_jugador = 0;
    int natural_dealer;

    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && jugadores[i].participa &&
            jugadores[i].estado != JUGADOR_PASADO)
        {
            hay_jugador = 1;
        }
    }

    /* Se planta tambien con 17 blando (un as contado como 11). */
    while (hay_jugador && calcular_puntaje(dealer.mano, dealer.cantidad_cartas) < 17)
    {
        if (repartir(dealer.mano, &dealer.cantidad_cartas) == -1)
        {
            anular_ronda();
            return;
        }
    }

    puntos_dealer = calcular_puntaje(dealer.mano, dealer.cantidad_cartas);
    natural_dealer = es_blackjack(dealer.mano, dealer.cantidad_cartas);

    /* Cada jugador se compara con el dealer, nunca con otro jugador. */
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && jugadores[i].participa)
        {
            puntos = calcular_puntaje(jugadores[i].mano, jugadores[i].cantidad_cartas);

            if (puntos > 21)
            {
                jugadores[i].resultado = PIERDE;
            }
            else if (natural_dealer)
            {
                if (es_blackjack(jugadores[i].mano, jugadores[i].cantidad_cartas))
                {
                    jugadores[i].resultado = EMPATA;
                }
                else
                {
                    jugadores[i].resultado = PIERDE;
                }
            }
            else if (es_blackjack(jugadores[i].mano, jugadores[i].cantidad_cartas))
            {
                jugadores[i].resultado = GANA;
            }
            else if (puntos_dealer > 21 || puntos > puntos_dealer)
            {
                jugadores[i].resultado = GANA;
            }
            else if (puntos < puntos_dealer)
            {
                jugadores[i].resultado = PIERDE;
            }
            else
            {
                jugadores[i].resultado = EMPATA;
            }
        }
    }

    turno_actual = -1;
    estado_ronda = RONDA_TERMINADA;
    preparar_esperando();
}

void siguiente_turno(void)
{
    int i;
    /* Buscar desde el lugar siguiente; no pedir cartas por otro jugador. */
    for (i = turno_actual + 1; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && jugadores[i].participa &&
            jugadores[i].estado == JUGADOR_JUGANDO)
        {
            turno_actual = i;
            return;
        }
    }
    resolver_dealer();
}

int iniciar_ronda(void)
{
    int i, vuelta, cantidad = 0;
    if (estado_ronda == RONDA_ACTIVA)
    {
        return -1;
    }
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        cantidad += jugadores[i].conectado;
    }
    if (!cantidad)
    {
        return -1;
    }
    crear_mazo();
    mezclar_mazo();
    memset(&dealer, 0, sizeof(dealer));
    estado_ronda = RONDA_ACTIVA;
    numero_ronda++;
    turno_actual = -1;
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        jugadores[i].participa = jugadores[i].conectado;
        jugadores[i].cantidad_cartas = 0;
        jugadores[i].resultado = SIN_RESULTADO;
        if (jugadores[i].conectado)
        {
            jugadores[i].estado = JUGADOR_JUGANDO;
        }
    }
    /* Dos vueltas de reparto: una carta a cada jugador y una al dealer. */
    for (vuelta = 0; vuelta < 2; vuelta++)
    {
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            if (jugadores[i].participa)
            {
                (void)repartir(jugadores[i].mano, &jugadores[i].cantidad_cartas);
                /* Se comprueba solo la mano que acaba de recibir su carta. */
                if (jugadores[i].cantidad_cartas == 2 &&
                    calcular_puntaje(jugadores[i].mano, 2) == 21)
                {
                    jugadores[i].estado = JUGADOR_PLANTADO;
                }
            }
        }
        (void)repartir(dealer.mano, &dealer.cantidad_cartas);
    }
    /* La primera carta del dealer sigue oculta. Su mano se resuelve
       cuando ya no quedan jugadores por actuar, nunca mirando el mazo. */
    siguiente_turno();
    return 0;
}

int pedir_carta(int id)
{
    int puntos;

    /* Validar el lugar antes de acceder al arreglo. */
    if (id < 0 || id >= MAX_JUGADORES)
    {
        return -1;
    }
    if (estado_ronda != RONDA_ACTIVA || id != turno_actual)
    {
        return -1;
    }
    if (jugadores[id].estado != JUGADOR_JUGANDO)
    {
        return -1;
    }

    puntos = calcular_puntaje(jugadores[id].mano, jugadores[id].cantidad_cartas);
    if (puntos >= 21)
    {
        return -1;
    }

    if (repartir(jugadores[id].mano, &jugadores[id].cantidad_cartas) == -1)
    {
        anular_ronda();
        return 0;
    }

    puntos = calcular_puntaje(jugadores[id].mano, jugadores[id].cantidad_cartas);
    if (puntos > 21)
    {
        jugadores[id].estado = JUGADOR_PASADO;
    }
    else if (puntos == 21)
    {
        jugadores[id].estado = JUGADOR_PLANTADO;
    }

    if (jugadores[id].estado != JUGADOR_JUGANDO)
    {
        siguiente_turno();
    }
    return 0;
}

int plantarse(int id)
{
    if (id < 0 || id >= MAX_JUGADORES)
    {
        return -1;
    }
    if (estado_ronda != RONDA_ACTIVA || id != turno_actual)
    {
        return -1;
    }
    if (jugadores[id].estado != JUGADOR_JUGANDO)
    {
        return -1;
    }
    jugadores[id].estado = JUGADOR_PLANTADO;
    siguiente_turno();
    return 0;
}

void desconectar_jugador(int id)
{
    jugadores[id].conectado = 0;
    jugadores[id].participa = 0;
    jugadores[id].estado = JUGADOR_DESCONECTADO;
    if (estado_ronda == RONDA_ACTIVA && turno_actual == id)
    {
        siguiente_turno();
    }
}

void texto_cartas(const Carta *mano, int cantidad, char *texto, size_t capacidad)
{
    const char *valores[] = {"?", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    const char *palos[] = {"C", "D", "T", "P"};
    size_t usados = 0;
    int i, escritos;
    const char *separador;
    if (!capacidad)
    {
        return;
    }
    texto[0] = '\0';
    if (!cantidad)
    {
        snprintf(texto, capacidad, "-");
        return;
    }
    for (i = 0; i < cantidad; i++)
    {
        if (i == 0)
        {
            separador = "";
        }
        else
        {
            separador = " ";
        }
        escritos = snprintf(texto + usados, capacidad - usados, "%s%s-%s", separador,
                            valores[mano[i].valor], palos[mano[i].palo]);
        if (escritos < 0 || (size_t)escritos >= capacidad - usados)
        {
            return;
        }
        usados += (size_t)escritos;
    }
}

const char *nombre_estado(int estado)
{
    const char *nombres[] = {"VACIO",    "ESPERANDO", "LISTO",       "JUGANDO",
                             "PLANTADO", "PASADO",    "DESCONECTADO"};
    if (estado >= 0 && estado <= JUGADOR_DESCONECTADO)
    {
        return nombres[estado];
    }
    return "DESCONOCIDO";
}

const char *nombre_resultado(int resultado)
{
    const char *nombres[] = {"-", "GANA", "PIERDE", "EMPATA", "ANULADA"};
    if (resultado >= 0 && resultado <= ANULADA)
    {
        return nombres[resultado];
    }
    return "-";
}
