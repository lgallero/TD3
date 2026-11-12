#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FRECUENCIA 20000
#define CANALES 2
#define BITS 8
#define BLOQUE (FRECUENCIA * CANALES / 50)          /* Tomo 20 ms de audio por bloque. */
#define RETARDO (FRECUENCIA * CANALES * 300 / 1000)  /* Uso un retardo de 300 ms. */
#define RETARDO_ECO (FRECUENCIA * CANALES * 1000 / 1000)
#define SILENCIO 128
#define LIMITE_MS 500
#define REVISION_MS 20
#define MAX_REINICIOS 3

enum { ECO, RETARDO_PURO, FIR, HIJOS };
enum { TURNO, TERMINADO, SEMAFOROS };

/* Uso esta unión para pasarle el valor inicial a semctl con SETVAL. */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* rutinas.c. */
void echo(const unsigned char *, unsigned char *, size_t, size_t);
void delay(const unsigned char *, unsigned char *, size_t, size_t);
void fir_low_pass(const unsigned char *, unsigned char *, size_t);

struct Memoria {
    unsigned char entrada[RETARDO + BLOQUE];
    unsigned char eco[RETARDO_ECO + BLOQUE];
    size_t bytes;
    off_t posicion;
};

static const char *nombres[HIJOS] = {"echo", "delay", "fir_low_pass"};
static int archivos[HIJOS] = {-1, -1, -1};
static pid_t hijos[HIJOS];
static int reinicios[HIJOS];
static int semid = -1;
static int shmid = -1;
static int audio = -1;
static struct Memoria *memoria;
static volatile sig_atomic_t terminar;

static void pedir_cierre(int senal);
static int abrir_dsp(void);
static int revisar_wav(int fd, uint32_t *restantes);
static int leer_completo(int fd, unsigned char *datos, size_t bytes);
static ssize_t leer_microfono(unsigned char *datos);
static int crear_hijo(int hijo);
static int revisar_hijos(void);
static int esperar_hijos(void);
static int cerrar_todo(int resultado, off_t recortar_en);

/* Función principal. Capturo audio y lo reparto entre los tres hijos.
 * Leo de un WAV o de /dev/dsp. Puedo indicar el directorio de salida.
 * Saco tres archivos PCM. Devuelvo 0 si termino bien o 1 si algo falla.
 */
int main(int argc, char **argv)
{
    const char *fuente = "/dev/dsp";
    const char *directorio = "/home/tdiii";
    uint32_t restantes = 0;
    off_t completos = 0;
    off_t recortar_en = -1;
    int estado = 0;


    if (argc > 3) {
        fprintf(stderr, "Uso: %s [entrada.wav|/dev/dsp] [directorio_salida]\n", argv[0]);
        return 1;
    }
    /* Si me pasan la entrada o el directorio, uso esos valores. */
    if (argc >= 2)
        fuente = argv[1];
    if (argc == 3)
        directorio = argv[2];
    bool usar_wav = strcmp(fuente, "/dev/dsp") != 0;

    /* Si falla la configuración de las señales de cierre, termino con error. */
    if (signal(SIGINT, pedir_cierre) == SIG_ERR ||
        signal(SIGTERM, pedir_cierre) == SIG_ERR)
        return cerrar_todo(1, -1);

    /* 1. Abro la fuente de audio. */
    if (usar_wav) {
        audio = open(fuente, O_RDONLY);

        if (audio == -1 || revisar_wav(audio, &restantes) == -1) {
            fprintf(stderr, "Se requiere un WAV PCM estéreo de 8 bits a 20000 Hz.\n");
            return cerrar_todo(1, -1);
        }
    } else {
        audio = abrir_dsp();
        /* Si no puedo abrir o configurar el micrófono, aviso y cierro. */
        if (audio == -1) {
            fprintf(stderr, "No se pudo usar /dev/dsp. Puede indicar un WAV.\n");
            return cerrar_todo(1, -1);
        }
        /* Si el descriptor queda fuera del rango de select. */
        if (audio >= FD_SETSIZE) {
            errno = EMFILE;
            return cerrar_todo(1, -1);
        }
    }

    /* 2. Creo la memoria compartida y los dos semáforos antes de hacer fork. */
    shmid = shmget(IPC_PRIVATE, sizeof(*memoria), IPC_CREAT | 0600);

    if (shmid == -1) /* Si no puedo crear la memoria compartida */
        return cerrar_todo(1, -1);
    memoria = shmat(shmid, NULL, 0);

    if (memoria == (void *)-1) /* Si no puedo conectarme a la memoria */
        return cerrar_todo(1, -1);
    /* Arranco con silencio en el historial del audio y del eco. */
    memset(memoria->entrada, SILENCIO, RETARDO);
    memset(memoria->eco, SILENCIO, RETARDO_ECO);

    union semun inicial = {.val = 0};
    semid = semget(IPC_PRIVATE, SEMAFOROS, IPC_CREAT | 0600);
    /* Si no puedo crear los semáforos o inicializarlos en cero */
    if (semid == -1 || semctl(semid, TURNO, SETVAL, inicial) == -1 ||
        semctl(semid, TERMINADO, SETVAL, inicial) == -1)
        return cerrar_todo(1, -1);

    /* 3. Abro las tres salidas y compruebo que sean archivos distintos. */
    struct stat info[HIJOS + 1];
    if (fstat(audio, &info[0]) == -1) /* Si no puedo identificar la entrada, cierro. */
        return cerrar_todo(1, -1);

    for (int i = 0; i < HIJOS && estado == 0; i++) {
        char ruta[4096];
        int largo = snprintf(ruta, sizeof(ruta), "%s/%s", directorio, nombres[i]);
        /* Si no puedo armar la ruta completa */
        if (largo < 0 || largo >= (int)sizeof(ruta)) {
            errno = ENAMETOOLONG;
            estado = -1;
        } else {
            archivos[i] = open(ruta, O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0644);
            struct stat *salida = &info[i + 1];
            
            if (archivos[i] == -1 || fstat(archivos[i], salida) == -1) {
                estado = -1;
            } else {
                bool valida = S_ISREG(salida->st_mode);
                for (int j = 0; j <= i && valida; j++) {
                    if (salida->st_dev == info[j].st_dev && salida->st_ino == info[j].st_ino)
                        valida = false;
                }
                if (!valida) {
                    errno = EINVAL;
                    estado = -1;
                }
            }
        }
    }

    /* 4. Dejo los archivos en cero y creo un hijo para cada efecto. */
    for (int hijo = 0; hijo < HIJOS && estado == 0; hijo++) {
        estado = ftruncate(archivos[hijo], 0);
        if (estado == 0) /* Si pude vaciar el archivo */
            estado = crear_hijo(hijo);
    }

    /* 5. Leo un bloque, doy el primer turno y espero a los tres hijos. */
    unsigned char *entrada = memoria->entrada + RETARDO;
    struct sembuf dar_turno = {TURNO, HIJOS, 0};
    ssize_t bytes = BLOQUE;

    while (!terminar && bytes > 0 && estado == 0) {
        estado = revisar_hijos();
        if (estado == 0 && usar_wav) {
            bytes = BLOQUE;
            if (restantes < BLOQUE) {
                /* Si me queda menos de un bloque, leo solo lo que queda. */
                bytes = restantes;
            }
            estado = leer_completo(audio, entrada, bytes);
            restantes -= bytes;
        } else if (estado == 0) {
            bytes = leer_microfono(entrada);
        }

        /* Si leí audio y no hubo errores, comparto el tamaño y la posición del bloque. */
        if (estado == 0 && bytes > 0) {
            memoria->bytes = bytes;
            memoria->posicion = completos;

            estado = semop(semid, &dar_turno, 1);
            if (estado == 0) /* Si pude dar el turno */
                estado = esperar_hijos();

            if (estado == -1) {
                /* Si el bloque quedó a medias me quedo con los anteriores. */
                recortar_en = completos;
            } else {
                completos += bytes;
                memmove(memoria->entrada, memoria->entrada + bytes, RETARDO);
                memmove(memoria->eco, memoria->eco + bytes, RETARDO_ECO);
                for (int hijo = 0; hijo < HIJOS; hijo++)
                    reinicios[hijo] = 0;
            }
        }
    }

    if (estado == -1 || bytes == -1)
        return cerrar_todo(1, recortar_en);
    fprintf(stderr, "%lld bytes PCM por archivo.\n", (long long)completos);
    return cerrar_todo(0, -1);
}

/* Cuando recibo la señal, marco que tengo que terminar la captura. */
static void pedir_cierre(int senal)
{
    (void)senal;
    terminar = 1;
}

/* Abro /dev/dsp y lo configuro para capturar audio.
 * Devuelvo el descriptor si sale bien o -1 si falla.
 */
static int abrir_dsp(void)
{
    int fd = open("/dev/dsp", O_RDONLY | O_NONBLOCK);
    int formato = AFMT_U8;
    int canales = CANALES;
    int frecuencia = FRECUENCIA;

    if (fd == -1) /* Si no pude abrir /dev/dsp */
        return -1;

    /* Si falla algún ajuste del formato, los canales o la frecuencia, cierro el audio. */
    if (ioctl(fd, SOUND_PCM_WRITE_BITS, &formato) == -1 ||
        ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &canales) == -1 ||
        ioctl(fd, SOUND_PCM_WRITE_RATE, &frecuencia) == -1) {
        int error = errno;
        close(fd);
        errno = error;
        return -1;
    }

    /* Compruebo que se haya aceptado los valores */
    if (formato != AFMT_U8 || canales != CANALES || frecuencia != FRECUENCIA) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    return fd;
}

/* Recibo 2 o 4 bytes en little endian y los junto para devolver un entero. */
static uint32_t leer_entero(const unsigned char *datos, int bytes)
{
    uint32_t valor = 0;
    for (int i = 0; i < bytes; i++)
        valor |= (uint32_t)datos[i] << (8 * i);
    return valor;
}

/* Leo del descriptor y guardo los bytes en el buffer.
 * Devuelvo 0 si leo todo o -1 si falla la lectura o se termina antes.
 */
static int leer_completo(int fd, unsigned char *datos, size_t bytes)
{
    size_t leidos = 0;
    int estado = 0;

    while (leidos < bytes && estado == 0) {
        ssize_t cantidad = read(fd, datos + leidos, bytes - leidos);
        if (cantidad > 0) { /* Si lei datos avanzo */
            leidos += cantidad;
        } else if (cantidad == 0) { /* Si se terminó antes de completar la lectura marco un error. */
            errno = EINVAL;
            estado = -1;
        } else if (errno != EINTR || terminar) { /* Si no puedo reintentar marco un error. */
            estado = -1;
        }
    }
    return estado;
}

/* Reviso el formato del WAV y busco el bloque data.
 * Dejo el archivo listo para leer las muestras y anoto su tamaño en restantes.
 * Devuelvo 0 si encontré audio válido o -1 si falla la revisión.
 */
static int revisar_wav(int fd, uint32_t *restantes)
{
    unsigned char cabecera[12], bloque[8], formato[16];
    off_t posicion = sizeof(cabecera);
    bool formato_valido = false;
    bool datos_encontrados = false;
    bool seguir = true;

    if (leer_completo(fd, cabecera, sizeof(cabecera)) == -1)
        return -1;
    off_t fin = 8 + (off_t)leer_entero(cabecera + 4, 4);
    /* Compruebo que la cabecera diga RIFF y WAVE. */
    if (memcmp(cabecera, "RIFF", 4) || memcmp(cabecera + 8, "WAVE", 4)) {
        errno = EINVAL;
        return -1;
    }

    while (seguir && !datos_encontrados &&
           posicion + (off_t)sizeof(bloque) <= fin) {
        seguir = leer_completo(fd, bloque, sizeof(bloque)) == 0;
        if (seguir) {
            uint32_t longitud = leer_entero(bloque + 4, 4);
            posicion += sizeof(bloque);
            off_t siguiente = posicion + (off_t)longitud + (longitud % 2);

            if (siguiente > fin) { 
                seguir = false;
            } else if (memcmp(bloque, "data", 4) == 0) {
                /* Si encuentro las muestras compruebo el formato y que haya pares estéreo completos. */
                datos_encontrados = formato_valido && longitud % CANALES == 0;
                *restantes = longitud;
                seguir = false;
            } else if (memcmp(bloque, "fmt ", 4) == 0) {
                /* Si encuentro el formato compruebo que sea PCM estéreo de 8 bits a 20 kHz. */
                formato_valido = longitud >= sizeof(formato) &&
                                 leer_completo(fd, formato, sizeof(formato)) == 0 &&
                                 leer_entero(formato, 2) == 1 &&
                                 leer_entero(formato + 2, 2) == CANALES &&
                                 leer_entero(formato + 4, 4) == FRECUENCIA &&
                                 leer_entero(formato + 14, 2) == BITS;
                seguir = formato_valido;
            }
            if (seguir) { /* Salto al proximo bloque. */
                posicion = lseek(fd, siguiente, SEEK_SET);
                seguir = posicion != -1;
            }
        }
    }

    if (!datos_encontrados) { 
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/* Leo del micrófono y junto hasta 20 ms de audio en el buffer que recibo.
 * Mientras espero, reviso si algún hijo terminó.
 * Devuelvo los bytes de los pares estéreo completos, 0 si no quedó audio o -1 si falla.
 */
static ssize_t leer_microfono(unsigned char *datos)
{
    size_t leidos = 0;
    bool fin_audio = false;
    int estado = 0;

    while (leidos < BLOQUE && !terminar && !fin_audio && estado == 0) {
        fd_set lectura;
        struct timeval espera = {0, 20000};
        FD_ZERO(&lectura);
        FD_SET(audio, &lectura);
        int listo = select(audio + 1, &lectura, NULL, NULL, &espera);

        if (listo == -1) { /* Si select falla sigo solo si lo interrumpió una señal. */
            if (errno != EINTR)
                estado = -1;
        } else {
            estado = revisar_hijos();
            if (estado == 0 && listo > 0) { /* Si hay audio y los hijos están en orden leo lo que falta. */

                ssize_t cantidad = read(audio, datos + leidos, BLOQUE - leidos);

                if (cantidad > 0) 
                    leidos += cantidad;

                else if (cantidad == 0) /* Si llegue al fin de la entrada, dejo de leer. */
                    fin_audio = true;

                else if (errno != EAGAIN && errno != EINTR) /* Si la lectura no se puede reintentar, marco un error. */
                    estado = -1;
            }
        }
    }
    if (estado == -1)
        return -1;

    return leidos - leidos % CANALES;
}

/* Recibo el número del hijo y su audio procesado. Guardo lo que falta del bloque.
 * Devuelvo 0 si el bloque quedó completo o -1 si no pude guardarlo.
 */
static int guardar_bloque(int hijo, const unsigned char *salida)
{
    int fd = archivos[hijo];
    off_t inicio = memoria->posicion;
    off_t fin = inicio + memoria->bytes;

    off_t posicion = lseek(fd, 0, SEEK_END);
    int estado = 0;

    if (posicion == -1) 
        return -1;

    if (posicion < inicio || posicion > fin) { /* Si el archivo termina fuera del bloque devuelvo un error. */
        errno = EIO;
        return -1;
    }

    /* Escribo desde donde quedo el archivo para no repetir bytes */
    while (posicion < fin && estado == 0) {
        ssize_t cantidad = write(fd, salida + (posicion - inicio), fin - posicion);
        if (cantidad > 0) {
            posicion += cantidad;
        } else if (cantidad == 0) {
            errno = EIO;
            estado = -1;
        } else if (errno != EINTR) {
            estado = -1;
        }
    }
    return estado;
}

/* El hijo espera su turno, aplica su efecto y guardo el resultado en mi archivo.
 * Si algo falla, termino con código 1 para que el padre me reemplace.
 */
static void trabajo_hijo(int hijo)
{
    unsigned char buffer[BLOQUE];
    const unsigned char *entrada = memoria->entrada + RETARDO;
    unsigned char *salida_eco = memoria->eco + RETARDO_ECO;
    int turno = HIJOS - hijo;
    int estado = 0;
    struct sembuf avisos[] = {{TURNO, -1, 0}, {TERMINADO, 1, 0}};

    close(audio);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_DFL);
    for (int i = 0; i < HIJOS; i++) {
        if (i != hijo)
            close(archivos[i]);
    }

    while (estado == 0) {
        /* Consulto el semáforo hasta que llegue mi turno. */
        int turno_actual = semctl(semid, TURNO, GETVAL);
        while (turno_actual != -1 && turno_actual != turno) {
            usleep(1000);
            turno_actual = semctl(semid, TURNO, GETVAL);
        }

        if (turno_actual == -1) { /* Si no pude consultar el turno salgo del while. */
            estado = -1;
        } else {
            unsigned char *salida = buffer;

            if (hijo == ECO) {
                /* Guardo el eco en memoria para usarlo en el próximo bloque. */
                salida = salida_eco;
                echo(entrada, salida, memoria->bytes, RETARDO_ECO);
            } else if (hijo == RETARDO_PURO) {
                delay(entrada, salida, memoria->bytes, RETARDO);
            } else {
                fir_low_pass(entrada, salida, memoria->bytes);
            }

            estado = guardar_bloque(hijo, salida);
            if (estado == -1) { 
                perror(nombres[hijo]);
            } else if (turno == 1) {
                /* Si es el ultimo doy el turno y aviso al padre en la misma llamada. */
                estado = semop(semid, avisos, 2);
            } else {
                estado = semop(semid, avisos, 1);
            }
        }
    }
    _exit(1);
}

/* Creo con fork el hijo que me indican y guardo su PID.
 * Devuelvo 0 si lo pude crear o -1 si falla fork.
 */
static int crear_hijo(int hijo)
{
    pid_t pid = fork();
    if (pid == -1)
        return -1;
    if (pid == 0) 
        trabajo_hijo(hijo);

    hijos[hijo] = pid;
    fprintf(stderr, "%s: PID %ld\n", nombres[hijo], (long)pid);
    return 0;
}


 /* Devuelvo 0 si pude crearlo o -1 si se agotaron los intentos o falló fork. */
static int reiniciar_hijo(int hijo)
{
    hijos[hijo] = 0;
    if (reinicios[hijo] == MAX_REINICIOS) { /* Si agoté los intentos del bloque, devuelvo un error. */
        fprintf(stderr, "%s: se agotaron los reinicios del bloque\n", nombres[hijo]);
        errno = EIO;
        return -1;
    }
    reinicios[hijo]++;
    return crear_hijo(hijo);
}

/* Reviso con waitpid si terminó algún hijo.
 * Devuelvo 0 si quedan activos o -1 si falla la revisión o el reemplazo.
 */
static int revisar_hijos(void)
{
    int estado = 0;
    for (int i = 0; i < HIJOS && estado == 0; i++) {
        pid_t pid = waitpid(hijos[i], NULL, WNOHANG);
        if (pid == -1 && errno != EINTR) { 
            estado = -1;
        } else if (pid > 0) {
            fprintf(stderr, "%s: terminó antes de tiempo. Reiniciando.\n", nombres[i]);
            estado = reiniciar_hijo(i);
        }
    }
    return estado;
}

static int parar_hijo(int hijo)
{
    pid_t pid = hijos[hijo];
    if (kill(pid, SIGKILL) == -1 && errno != ESRCH) /* Si no pude detenerlo y todavía existe */
        return -1;
    pid_t recogido;
    do {
        recogido = waitpid(pid, NULL, 0);
    } while (recogido == -1 && errno == EINTR);

    if (recogido == -1)
        return -1;
    hijos[hijo] = 0;
    return 0;
}

/* Espero la confirmación del último hijo.
 * Devuelvo 0 cuando termina el bloque o -1 si algo falla.
 */
static int esperar_hijos(void)
{
    struct sembuf recibir = {TERMINADO, -1, 0};
    const struct timespec intervalo = {0, REVISION_MS * 1000000L};
    int turno_anterior = HIJOS;
    pid_t pid_anterior = hijos[ECO];
    int espera_ms = 0;
    bool confirmado = false;
    int estado = 0;

    while (!confirmado && estado == 0) {
        if (semtimedop(semid, &recibir, 1, &intervalo) == 0) { /* Si recibí la confirmación termino la espera. */
            confirmado = true;
        } else if (errno != EAGAIN && errno != EINTR) {
            estado = -1;
        } else {
            /* Si vencio el intervalo o me interrumpió una señal reviso los hijos. */
            bool vencio_espera = errno == EAGAIN;
            estado = revisar_hijos();
            if (estado == 0) {
                int turno = semctl(semid, TURNO, GETVAL);
                if (turno == -1) {
                    estado = -1;
                } else if (turno > 0) {
                    int hijo = HIJOS - turno;
                   
                    if (turno != turno_anterior || hijos[hijo] != pid_anterior) {
                        turno_anterior = turno;
                        pid_anterior = hijos[hijo];
                        espera_ms = 0;
                    } else if (vencio_espera) {
                        espera_ms += REVISION_MS;
                    }
                    if (espera_ms >= LIMITE_MS) { /* Si el turno sigue trabado, detengo y reemplazo al hijo. */
                        fprintf(stderr, "%s: excedió el tiempo. Reiniciando.\n", nombres[hijo]);
                        estado = parar_hijo(hijo);
                        if (estado == 0)
                            estado = reiniciar_hijo(hijo);
                    }
                }
            }
        }
    }
    return estado;
}

/* Detengo y saco a los hijos, cierro los archivos y libero la memoria y los semáforos.
 * Si me indican una longitud, recorto los archivos hasta el último bloque completo.
 * Devuelvo el código con el que termino el programa.
 */
static int cerrar_todo(int resultado, off_t recortar_en)
{
    if (resultado != 0) 
        perror("TP 4 audio");

    for (int hijo = 0; hijo < HIJOS; hijo++) {
        if (hijos[hijo] > 0 && parar_hijo(hijo) == -1) { 
            perror("detener hijo");
            resultado = 1;
        }
    }
    for (int hijo = 0; hijo < HIJOS; hijo++) {
        if (archivos[hijo] != -1) {
            if (recortar_en >= 0 && ftruncate(archivos[hijo], recortar_en) == -1)
                perror(nombres[hijo]);
            close(archivos[hijo]);
        }
    }
    /* Libero solo los recursos */
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    if (memoria && memoria != (void *)-1)
        shmdt(memoria);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    if (audio != -1)
        close(audio);
    return resultado;
}
