/* Reglas del Blackjack simplificado. No contiene sockets ni interfaz. */
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
    proxima_carta = numero_ronda = 0;
    turno_actual = -1;
    estado_ronda = SIN_RONDA;
}

void registrar_jugador(int id, const char *nombre)
{
    Jugador *jugador = &jugadores[id];
    memset(jugador, 0, sizeof(*jugador));
    jugador->id = id;
    snprintf(jugador->nombre, sizeof(jugador->nombre), "%s", nombre);
    jugador->conectado = 1;
    jugador->estado = estado_ronda == RONDA_ACTIVA ? JUGADOR_ESPERANDO : JUGADOR_LISTO;
}

void crear_mazo(void)
{
    int palo, valor, indice = 0;
    for (palo = 0; palo < 4; palo++)
        for (valor = 1; valor <= 13; valor++)
        {
            mazo[indice].valor = valor;
            mazo[indice++].palo = palo;
        }
    proxima_carta = 0;
}

void mezclar_mazo(void)
{
    int i, j;
    Carta auxiliar;
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
        if (mano[i].valor == 1) { total += 11; ases++; }
        else total += mano[i].valor > 10 ? 10 : mano[i].valor;
    }
    while (total > 21 && ases > 0) { total -= 10; ases--; }
    return total;
}

int es_blackjack(const Carta *mano, int cantidad)
{
    return cantidad == 2 && calcular_puntaje(mano, cantidad) == 21;
}

static int repartir(Carta *mano, int *cantidad)
{
    if (proxima_carta >= 52 || *cantidad >= MAX_CARTAS) return -1;
    mano[(*cantidad)++] = mazo[proxima_carta++];
    return 0;
}

static void preparar_esperando(void)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado && jugadores[i].estado == JUGADOR_ESPERANDO)
            jugadores[i].estado = JUGADOR_LISTO;
}

static void anular_ronda(void)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado && jugadores[i].participa)
            jugadores[i].resultado = ANULADA;
    turno_actual = -1;
    estado_ronda = RONDA_TERMINADA;
    preparar_esperando();
}

static void resolver_dealer(void)
{
    int i, puntos, puntos_dealer, hay_jugador = 0;
    int natural_dealer;
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado && jugadores[i].participa &&
            jugadores[i].estado != JUGADOR_PASADO) hay_jugador = 1;

    /* Se planta tambien con 17 blando (un as contado como 11). */
    while (hay_jugador && calcular_puntaje(dealer.mano, dealer.cantidad_cartas) < 17)
        if (repartir(dealer.mano, &dealer.cantidad_cartas) == -1)
        { anular_ronda(); return; }

    puntos_dealer = calcular_puntaje(dealer.mano, dealer.cantidad_cartas);
    natural_dealer = es_blackjack(dealer.mano, dealer.cantidad_cartas);
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        Jugador *j = &jugadores[i];
        if (!j->conectado || !j->participa) continue;
        puntos = calcular_puntaje(j->mano, j->cantidad_cartas);
        if (puntos > 21) j->resultado = PIERDE;
        else if (natural_dealer)
            j->resultado = es_blackjack(j->mano, j->cantidad_cartas) ? EMPATA : PIERDE;
        else if (es_blackjack(j->mano, j->cantidad_cartas)) j->resultado = GANA;
        else if (puntos_dealer > 21 || puntos > puntos_dealer) j->resultado = GANA;
        else if (puntos < puntos_dealer) j->resultado = PIERDE;
        else j->resultado = EMPATA;
    }
    turno_actual = -1;
    estado_ronda = RONDA_TERMINADA;
    preparar_esperando();
}

void siguiente_turno(void)
{
    int i;
    for (i = turno_actual + 1; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado && jugadores[i].participa &&
            jugadores[i].estado == JUGADOR_JUGANDO)
        { turno_actual = i; return; }
    resolver_dealer();
}

int iniciar_ronda(void)
{
    int i, vuelta, cantidad = 0;
    if (estado_ronda == RONDA_ACTIVA) return -1;
    for (i = 0; i < MAX_JUGADORES; i++) cantidad += jugadores[i].conectado;
    if (!cantidad) return -1;
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
        if (jugadores[i].conectado) jugadores[i].estado = JUGADOR_JUGANDO;
    }
    /* Dos vueltas de reparto: una carta a cada jugador y una al dealer. */
    for (vuelta = 0; vuelta < 2; vuelta++)
    {
        for (i = 0; i < MAX_JUGADORES; i++)
            if (jugadores[i].participa)
                (void)repartir(jugadores[i].mano, &jugadores[i].cantidad_cartas);
        (void)repartir(dealer.mano, &dealer.cantidad_cartas);
    }
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].participa && es_blackjack(jugadores[i].mano, 2))
            jugadores[i].estado = JUGADOR_PLANTADO;
    if (es_blackjack(dealer.mano, 2)) resolver_dealer();
    else siguiente_turno();
    return 0;
}

int pedir_carta(int id)
{
    Jugador *j;
    int puntos;
    if (estado_ronda != RONDA_ACTIVA || id != turno_actual || id < 0 ||
        id >= MAX_JUGADORES || jugadores[id].estado != JUGADOR_JUGANDO) return -1;
    j = &jugadores[id];
    if (repartir(j->mano, &j->cantidad_cartas) == -1)
    { anular_ronda(); return 0; }
    puntos = calcular_puntaje(j->mano, j->cantidad_cartas);
    if (puntos > 21) j->estado = JUGADOR_PASADO;
    else if (puntos == 21) j->estado = JUGADOR_PLANTADO;
    if (j->estado != JUGADOR_JUGANDO) siguiente_turno();
    return 0;
}

int plantarse(int id)
{
    if (estado_ronda != RONDA_ACTIVA || id != turno_actual || id < 0 ||
        id >= MAX_JUGADORES || jugadores[id].estado != JUGADOR_JUGANDO) return -1;
    jugadores[id].estado = JUGADOR_PLANTADO;
    siguiente_turno();
    return 0;
}

void desconectar_jugador(int id)
{
    jugadores[id].conectado = 0;
    jugadores[id].participa = 0;
    jugadores[id].estado = JUGADOR_DESCONECTADO;
    if (estado_ronda == RONDA_ACTIVA && turno_actual == id) siguiente_turno();
}

void texto_cartas(const Carta *mano, int cantidad, char *texto, size_t capacidad)
{
    const char *valores[] = {"?", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    const char *palos[] = {"C", "D", "T", "P"};
    size_t usados = 0;
    int i, escritos;
    if (!capacidad) return;
    texto[0] = '\0';
    if (!cantidad) { snprintf(texto, capacidad, "-"); return; }
    for (i = 0; i < cantidad; i++)
    {
        escritos = snprintf(texto + usados, capacidad - usados, "%s%s-%s",
                            i ? " " : "", valores[mano[i].valor], palos[mano[i].palo]);
        if (escritos < 0 || (size_t)escritos >= capacidad - usados) return;
        usados += (size_t)escritos;
    }
}

const char *nombre_estado(int estado)
{
    const char *nombres[] = {"VACIO", "ESPERANDO", "LISTO", "JUGANDO", "PLANTADO", "PASADO", "DESCONECTADO"};
    return estado >= 0 && estado <= JUGADOR_DESCONECTADO ? nombres[estado] : "DESCONOCIDO";
}

const char *nombre_resultado(int resultado)
{
    const char *nombres[] = {"-", "GANA", "PIERDE", "EMPATA", "ANULADA"};
    return resultado >= 0 && resultado <= ANULADA ? nombres[resultado] : "-";
}
