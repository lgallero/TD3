/* Interfaz de terminal con ncurses. Las cartas y puntajes vienen del master. */
#include "ui.h"
#include "blackjack.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int visible, puntos;
    char nombre[32], estado[24], resultado[16], cartas[128];
} JugadorVisible;
typedef struct {
    int ronda, estado, turno, puntos_dealer, oculta;
    char cartas_dealer[128];
    JugadorVisible jugadores[MAX_JUGADORES];
} Pantalla;

static Pantalla actual, pendiente;
static int recibiendo_estado, mi_id = -1;
static char mi_nombre[32], avisos[4][160];

void ui_aviso(const char *texto)
{
    int i;
    for (i = 0; i < 3; i++) memcpy(avisos[i], avisos[i + 1], sizeof(avisos[i]));
    snprintf(avisos[3], sizeof(avisos[3]), "%s", texto);
}

int ui_iniciar(const char *nombre)
{
    snprintf(mi_nombre, sizeof(mi_nombre), "%s", nombre);
    actual.turno = -1;
    snprintf(actual.cartas_dealer, sizeof(actual.cartas_dealer), "-");
    if (initscr() == NULL) return -1;
    cbreak(); noecho(); keypad(stdscr, TRUE); nodelay(stdscr, TRUE);
    (void)curs_set(0);
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    }
    ui_aviso("Conectado. Esperando registro en la mesa...");
    return 0;
}

void ui_finalizar(void) { endwin(); }
int ui_tecla(void) { return getch(); }

void ui_recibir(const char *linea)
{
    int id;
    char texto[160], nombre[32], resultado[16];
    JugadorVisible jugador;
    if (sscanf(linea, "BIENVENIDO %d", &id) == 1) { mi_id = id; ui_aviso("Registro aceptado. I inicia una ronda."); return; }
    if (!strncmp(linea, "ESTADO ", 7))
    {
        memset(&pendiente, 0, sizeof(pendiente));
        recibiendo_estado = sscanf(linea, "ESTADO %d %d %d", &pendiente.ronda, &pendiente.estado, &pendiente.turno) == 3;
        return;
    }
    if (recibiendo_estado && !strncmp(linea, "DEALER ", 7))
    {
        (void)sscanf(linea, "DEALER %d %d %127[^\n]", &pendiente.puntos_dealer, &pendiente.oculta, pendiente.cartas_dealer);
        return;
    }
    if (recibiendo_estado && !strncmp(linea, "JUGADOR ", 8))
    {
        memset(&jugador, 0, sizeof(jugador));
        if (sscanf(linea, "JUGADOR %d %23s %d %31s %15s %127[^\n]", &id,
                   jugador.estado, &jugador.puntos, jugador.nombre, jugador.resultado, jugador.cartas) == 6 &&
            id >= 0 && id < MAX_JUGADORES)
        { jugador.visible = 1; pendiente.jugadores[id] = jugador; }
        return;
    }
    if (!strcmp(linea, "FIN_ESTADO"))
    { if (recibiendo_estado) actual = pendiente; recibiendo_estado = 0; return; }
    if (!strcmp(linea, "TU_TURNO")) ui_aviso("Es tu turno: H para pedir o S para plantarte.");
    else if (!strcmp(linea, "ESPERANDO")) ui_aviso("Entraste durante una ronda. Participaras en la siguiente.");
    else if (!strcmp(linea, "NO_ES_TU_TURNO")) ui_aviso("Todavia no es tu turno.");
    else if (!strcmp(linea, "RONDA_INICIADA")) ui_aviso("Comienza una nueva ronda.");
    else if (!strcmp(linea, "RONDA_FINALIZADA")) ui_aviso("Ronda finalizada. I inicia la siguiente.");
    else if (!strcmp(linea, "TURNO_DEALER")) ui_aviso("El dealer juega y se calculan los resultados.");
    else if (!strcmp(linea, "SERVIDOR_LLENO")) ui_aviso("La mesa esta llena (maximo cinco conexiones).");
    else if (!strcmp(linea, "SERVIDOR_CERRADO")) ui_aviso("El servidor se ha cerrado.");
    else if (!strcmp(linea, "HASTA_LUEGO")) ui_aviso("Saliste de la mesa.");
    else if (!strncmp(linea, "ERROR ", 6)) ui_aviso(linea + 6);
    else if (sscanf(linea, "RESULTADO %31s %15s", nombre, resultado) == 2)
    { snprintf(texto, sizeof(texto), "%s: %s", nombre, resultado); ui_aviso(texto); }
    else if (sscanf(linea, "JUGADOR_UNIDO %31s", nombre) == 1)
    { snprintf(texto, sizeof(texto), "%s se unio a la mesa.", nombre); ui_aviso(texto); }
    else if (sscanf(linea, "JUGADOR_DESCONECTADO %31s", nombre) == 1)
    { snprintf(texto, sizeof(texto), "%s se desconecto.", nombre); ui_aviso(texto); }
    else if (sscanf(linea, "JUGADOR_PIDE %31s", nombre) == 1)
    { snprintf(texto, sizeof(texto), "%s pidio una carta.", nombre); ui_aviso(texto); }
    else if (sscanf(linea, "JUGADOR_PLANTADO %31s", nombre) == 1)
    { snprintf(texto, sizeof(texto), "%s se planto.", nombre); ui_aviso(texto); }
    /* ESPERAR, TURNO y CONECTADO no llenan el historial en cada actualizacion. */
}

static void renglon(int fila, const char *texto)
{
    if (fila >= 0 && fila < LINES) mvaddnstr(fila, 0, texto, COLS > 1 ? COLS - 1 : 0);
}

void ui_dibujar(void)
{
    int i, fila;
    char texto[256];
    const char *estado = actual.estado == RONDA_ACTIVA ? "EN JUEGO" :
                         actual.estado == RONDA_TERMINADA ? "FINALIZADA" : "ESPERANDO INICIO";
    erase();
    if (COLS < 80 || LINES < 24)
    {
        renglon(0, "Amplia la terminal a 80 columnas x 24 filas.");
        renglon(1, "H: pedir | S: plantarse | I: iniciar | Q: salir");
        refresh(); return;
    }
    attron(A_BOLD);
    snprintf(texto, sizeof(texto), "BLACKJACK | Mesa compartida | Jugador: %s", mi_nombre);
    renglon(0, texto); attroff(A_BOLD);
    renglon(1, "H: PEDIR CARTA    S: PLANTARSE    I: INICIAR RONDA    Q: SALIR");
    snprintf(texto, sizeof(texto), "Ronda %d | %s | Turno: %s", actual.ronda, estado,
             actual.turno >= 0 && actual.turno < MAX_JUGADORES ? actual.jugadores[actual.turno].nombre : "-");
    renglon(3, texto);
    snprintf(texto, sizeof(texto), "DEALER | %s: %d", actual.oculta ? "Puntos visibles" : "Puntos", actual.puntos_dealer);
    renglon(4, texto);
    snprintf(texto, sizeof(texto), "Cartas: %s%s", actual.cartas_dealer, actual.oculta ? " [OCULTA]" : "");
    renglon(5, texto);
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        JugadorVisible *j = &actual.jugadores[i];
        fila = 7 + 2 * i;
        if (!j->visible) { snprintf(texto, sizeof(texto), "Lugar %d: libre", i + 1); renglon(fila, texto); continue; }
        if (actual.turno == i) attron(A_BOLD | COLOR_PAIR(1));
        snprintf(texto, sizeof(texto), "%s%s | %s | Puntos: %d | %s", j->nombre,
                 mi_id == i ? " (vos)" : "", j->estado, j->puntos, j->resultado);
        renglon(fila, texto);
        attroff(A_BOLD | COLOR_PAIR(1));
        snprintf(texto, sizeof(texto), "  Cartas: %s", j->cartas); renglon(fila + 1, texto);
    }
    renglon(18, "Palos: C=corazones D=diamantes T=treboles P=picas. Sin apuestas.");
    for (i = 0; i < 4; i++) renglon(LINES - 4 + i, avisos[i]);
    refresh();
}
