#ifndef UI_H
#define UI_H

/* Solo presentacion: el cliente no calcula ni modifica la partida. */
int ui_iniciar(const char *nombre);
void ui_finalizar(void);
void ui_recibir(const char *linea);
void ui_aviso(const char *texto);
void ui_dibujar(void);
int ui_tecla(void);

#endif
