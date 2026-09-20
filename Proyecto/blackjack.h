#ifndef BLACKJACK_H
#define BLACKJACK_H

#include <stddef.h>

#define MAX_JUGADORES 5
#define MAX_CARTAS 12
#define TAM_NOMBRE 32

typedef struct { int valor; int palo; } Carta;

enum EstadoJugador {
    JUGADOR_VACIO, JUGADOR_ESPERANDO, JUGADOR_LISTO, JUGADOR_JUGANDO,
    JUGADOR_PLANTADO, JUGADOR_PASADO, JUGADOR_DESCONECTADO
};
enum EstadoRonda { SIN_RONDA, RONDA_ACTIVA, RONDA_TERMINADA };
enum Resultado { SIN_RESULTADO, GANA, PIERDE, EMPATA, ANULADA };

typedef struct {
    int id;
    char nombre[TAM_NOMBRE];
    int conectado;
    int estado;
    int participa;
    Carta mano[MAX_CARTAS];
    int cantidad_cartas;
    int resultado;
} Jugador;

typedef struct {
    Carta mano[MAX_CARTAS];
    int cantidad_cartas;
} Dealer;

/* Unica instancia de juego. Solo el padre llama a las funciones siguientes. */
extern Carta mazo[52];
extern Jugador jugadores[MAX_JUGADORES];
extern Dealer dealer;
extern int proxima_carta, turno_actual, estado_ronda, numero_ronda;

void inicializar_juego(void);
void registrar_jugador(int id, const char *nombre);
void desconectar_jugador(int id);
void crear_mazo(void);
void mezclar_mazo(void);
int calcular_puntaje(const Carta *mano, int cantidad);
int es_blackjack(const Carta *mano, int cantidad);
int iniciar_ronda(void);
int pedir_carta(int id);
int plantarse(int id);
void siguiente_turno(void);
void texto_cartas(const Carta *mano, int cantidad, char *texto, size_t capacidad);
const char *nombre_estado(int estado);
const char *nombre_resultado(int resultado);

#endif
