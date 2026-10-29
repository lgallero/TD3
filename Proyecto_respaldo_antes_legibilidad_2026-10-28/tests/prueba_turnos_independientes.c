#include "../blackjack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    Jugador siguientes[2];
    int i, usadas;
    inicializar_juego();
    for (i = 0; i < 3; ++i) {
        registrar_jugador(i, "Jugador");
        jugadores[i].participa = 1;
        jugadores[i].estado = JUGADOR_JUGANDO;
        jugadores[i].cantidad_cartas = 2;
        jugadores[i].mano[0] = (Carta){10, i};
        jugadores[i].mano[1] = (Carta){5, i};
        mazo[i] = (Carta){6, i};
    }
    dealer.cantidad_cartas = 2;
    dealer.mano[0] = (Carta){10, 3};
    dealer.mano[1] = (Carta){7, 3};
    estado_ronda = RONDA_ACTIVA;
    turno_actual = 0;
    proxima_carta = 0;
    siguientes[0] = jugadores[1];
    siguientes[1] = jugadores[2];

    /* Los tres obtendrian 21 al pedir. Solo actua el primero. */
    assert(pedir_carta(0) == 0);
    assert(calcular_puntaje(jugadores[0].mano, 3) == 21);
    assert(jugadores[0].estado == JUGADOR_PLANTADO);
    assert(estado_ronda == RONDA_ACTIVA && turno_actual == 1);
    assert(proxima_carta == 1);
    assert(memcmp(&jugadores[1], &siguientes[0], sizeof(Jugador)) == 0);
    assert(memcmp(&jugadores[2], &siguientes[1], sizeof(Jugador)) == 0);
    usadas = proxima_carta;
    assert(pedir_carta(0) == -1); /* No puede volver a pedir con 21. */
    assert(proxima_carta == usadas);

    assert(pedir_carta(1) == 0);
    assert(estado_ronda == RONDA_ACTIVA && turno_actual == 2);
    assert(proxima_carta == 2);
    assert(memcmp(&jugadores[2], &siguientes[1], sizeof(Jugador)) == 0);
    assert(pedir_carta(2) == 0);
    assert(proxima_carta == 3 && estado_ronda == RONDA_TERMINADA);
    for (i = 0; i < 3; ++i) assert(jugadores[i].resultado == GANA);
    puts("OK: llegar a 21 no modifica las manos de los otros dos jugadores.");
    return 0;
}
