#include "../blackjack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Busca repartos reales reproducibles dentro de la biblioteca C utilizada. */
static void buscar_reparto(int natural_dealer, int natural_jugador)
{
    unsigned semilla;
    for (semilla = 0; semilla < 100000; ++semilla) {
        inicializar_juego();
        registrar_jugador(0, "Ana");
        registrar_jugador(1, "Luis");
        srand(semilla);
        assert(iniciar_ronda() == 0);
        /* Dos jugadores, dealer ultimo: J0, J1, D, J0, J1, D. */
        assert(jugadores[0].mano[0].valor == mazo[0].valor);
        assert(jugadores[1].mano[0].valor == mazo[1].valor);
        assert(dealer.mano[0].valor == mazo[2].valor);
        assert(jugadores[0].mano[1].valor == mazo[3].valor);
        assert(jugadores[1].mano[1].valor == mazo[4].valor);
        assert(dealer.mano[1].valor == mazo[5].valor);
        if (es_blackjack(dealer.mano, 2) == natural_dealer &&
            es_blackjack(jugadores[0].mano, 2) == natural_jugador &&
            !es_blackjack(jugadores[1].mano, 2))
            return;
    }
    assert(!"No se encontro el reparto requerido");
}

int main(void)
{
    /* El natural del dealer no resuelve ni publica resultados al repartir. */
    buscar_reparto(1, 0);
    assert(estado_ronda == RONDA_ACTIVA && turno_actual == 0);
    assert(jugadores[0].resultado == SIN_RESULTADO);
    assert(jugadores[1].resultado == SIN_RESULTADO);
    assert(pedir_carta(1) == -1); /* Sigue validando el turno. */
    assert(plantarse(0) == 0);
    assert(estado_ronda == RONDA_ACTIVA && turno_actual == 1);
    assert(plantarse(1) == 0);
    assert(estado_ronda == RONDA_TERMINADA);
    assert(jugadores[0].resultado == PIERDE);
    assert(dealer.cantidad_cartas == 2);

    /* El jugador con natural termina; los otros aun deben jugar. */
    buscar_reparto(1, 1);
    assert(turno_actual == 1 && jugadores[0].estado == JUGADOR_PLANTADO);
    assert(pedir_carta(0) == -1);
    assert(proxima_carta == 6 && jugadores[0].cantidad_cartas == 2);
    assert(estado_ronda == RONDA_ACTIVA && turno_actual == 1);
    assert(plantarse(1) == 0);
    assert(jugadores[0].resultado == EMPATA);

    /* Ni siquiera una carta futura que conservaria 21 habilita otro pedido. */
    buscar_reparto(0, 1);
    mazo[proxima_carta] = (Carta){10, 0};
    assert(pedir_carta(0) == -1);
    assert(jugadores[0].cantidad_cartas == 2 && proxima_carta == 6);
    assert(jugadores[0].estado == JUGADOR_PLANTADO && turno_actual == 1);

    buscar_reparto(0, 0);
    jugadores[0].mano[0] = (Carta){10, 0};
    jugadores[0].mano[1] = (Carta){1, 0};
    /* Proteccion adicional si se recibe PEDIR con 21 y estado inconsistente. */
    assert(pedir_carta(0) == -1);
    jugadores[0].mano[0] = (Carta){9, 0};
    mazo[proxima_carta] = (Carta){2, 0};
    assert(pedir_carta(0) == 0);
    assert(calcular_puntaje(jugadores[0].mano, 3) == 12);
    assert(jugadores[0].estado == JUGADOR_JUGANDO && turno_actual == 0);
    mazo[proxima_carta] = (Carta){10, 0};
    assert(pedir_carta(0) == 0);
    assert(jugadores[0].estado == JUGADOR_PASADO && turno_actual == 1);
    puts("OK: reparto sin cierre anticipado, turnos, empate y puntajes al pedir.");
    return 0;
}
