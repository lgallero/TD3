#define _DEFAULT_SOURCE
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
#define BLOQUE (FRECUENCIA * CANALES / 50)          /* 20 ms */
#define RETARDO (FRECUENCIA * CANALES * 300 / 1000)  /* 300 ms */
#define RETARDO_ECO (512 * CANALES)
#define SILENCIO 128
#define LIMITE_MS 500
#define MAX_REINICIOS 3

enum { ECO, RETARDO_PURO, FIR, HIJOS };
enum { TURNO, TERMINADO, SEMAFOROS };

/* Argumento de semctl para SETVAL. */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

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

/* Operación: pide detener la captura. Entrada: señal. Salida: bandera global. */
static void pedir_fin(int senal)
{
    (void)senal;
    terminar = 1;
}

/* Operación: abre y configura OSS. Entrada: ninguna. Salida: descriptor o -1. */
static int abrir_dsp(void)
{
    int fd = open("/dev/dsp", O_RDONLY | O_NONBLOCK);
    int formato = AFMT_U8;
    int canales = CANALES;
    int frecuencia = FRECUENCIA;

    if (fd == -1)
        return -1;

    if (ioctl(fd, SOUND_PCM_WRITE_BITS, &formato) == -1 ||
        ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &canales) == -1 ||
        ioctl(fd, SOUND_PCM_WRITE_RATE, &frecuencia) == -1) {
        int error = errno;
        close(fd);
        errno = error;
        return -1;
    }

    if (formato != AFMT_U8 || canales != CANALES || frecuencia != FRECUENCIA) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    return fd;
}

/* Operación: lee un entero little endian. Entrada: 2 o 4 bytes. Salida: entero. */
static uint32_t leer_entero(const unsigned char *datos, int bytes)
{
    uint32_t valor = 0;
    for (int i = 0; i < bytes; i++)
        valor |= (uint32_t)datos[i] << (8 * i);
    return valor;
}

/* Operación: completa una lectura. Entrada: descriptor, buffer y longitud.
 * Salida: bytes en el buffer y 0, o -1 si hay error o fin prematuro.
 */
static int leer_exacto(int fd, unsigned char *datos, size_t bytes)
{
    size_t leidos = 0;
    while (leidos < bytes) {
        ssize_t cantidad = read(fd, datos + leidos, bytes - leidos);
        if (cantidad == -1 && errno == EINTR && !terminar)
            continue;
        if (cantidad <= 0) {
            if (cantidad == 0)
                errno = EINVAL;
            return -1;
        }
        leidos += cantidad;
    }
    return 0;
}

/* Operación: valida el WAV y busca su bloque data. Entrada: descriptor.
 * Salida: descriptor posicionado en las muestras y su longitud en restantes.
 * Devuelve 0 si el WAV es válido o -1 ante error.
 */
static int preparar_wav(int fd, uint32_t *restantes)
{
    unsigned char cabecera[12], bloque[8], formato[16];
    struct stat info;
    off_t posicion = sizeof(cabecera);
    off_t fin;
    int formato_valido = 0;

    if (fstat(fd, &info) == -1 || leer_exacto(fd, cabecera, sizeof(cabecera)) == -1)
        return -1;
    fin = 8 + (off_t)leer_entero(cabecera + 4, 4);
    if (memcmp(cabecera, "RIFF", 4) || memcmp(cabecera + 8, "WAVE", 4) ||
        fin > info.st_size)
        goto wav_invalido;

    while (posicion + (off_t)sizeof(bloque) <= fin) {
        if (leer_exacto(fd, bloque, sizeof(bloque)) == -1)
            return -1;
        uint32_t longitud = leer_entero(bloque + 4, 4);
        posicion += sizeof(bloque);
        off_t siguiente = posicion + (off_t)longitud + (longitud % 2);
        if (siguiente > fin)
            goto wav_invalido;

        if (memcmp(bloque, "fmt ", 4) == 0) {
            if (longitud < sizeof(formato))
                goto wav_invalido;
            if (leer_exacto(fd, formato, sizeof(formato)) == -1)
                return -1;
            if (leer_entero(formato, 2) != 1 ||
                leer_entero(formato + 2, 2) != CANALES ||
                leer_entero(formato + 4, 4) != FRECUENCIA ||
                leer_entero(formato + 12, 2) != CANALES ||
                leer_entero(formato + 14, 2) != BITS)
                goto wav_invalido;
            formato_valido = 1;
        }

        if (memcmp(bloque, "data", 4) == 0) {
            if (!formato_valido || longitud % CANALES != 0)
                goto wav_invalido;
            *restantes = longitud;
            return 0;
        }

        if (lseek(fd, siguiente, SEEK_SET) == -1)
            return -1;
        posicion = siguiente;
    }

wav_invalido:
    errno = EINVAL;
    return -1;
}

/* Operación: guarda el tramo pendiente de un bloque. Entrada: número de
 * hijo y audio procesado. Salida: 0 si el bloque está completo o -1.
 */
static int guardar_bloque(int hijo, const unsigned char *salida)
{
    int fd = archivos[hijo];
    off_t inicio = memoria->posicion;
    off_t fin = inicio + memoria->bytes;
    off_t posicion = lseek(fd, 0, SEEK_END);

    if (posicion == -1)
        return -1;
    if (posicion < inicio || posicion > fin) {
        errno = EIO;
        return -1;
    }

    /* Si se relanzó el hijo, el archivo ya puede contener parte o todo el bloque. */
    while (posicion < fin) {
        ssize_t cantidad = write(fd, salida + (posicion - inicio), fin - posicion);
        if (cantidad == -1 && errno == EINTR)
            continue;
        if (cantidad <= 0) {
            if (cantidad == 0)
                errno = EIO;
            return -1;
        }
        posicion += cantidad;
    }
    return 0;
}

/* Operación: espera su turno, procesa y guarda cada bloque. Entrada: hijo.
 * Salida: archivo de ese efecto y, para echo, bloque de realimentación.
 * En caso de error termina con código 1 para que el padre lo reponga.
 */
static void procesar_audio(int hijo)
{
    unsigned char salida[BLOQUE];
    const unsigned char *entrada = memoria->entrada + RETARDO;
    unsigned char *eco = memoria->eco + RETARDO_ECO;
    short turno = HIJOS - hijo;
    /* Las tres operaciones juntas esperan exactamente el valor 'turno'. */
    struct sembuf esperar[] = {{TURNO, -turno, 0}, {TURNO, 0, 0}, {TURNO, turno, 0}};
    struct sembuf avisar[] = {{TURNO, -1, 0}, {TERMINADO, 1, 0}};

    close(audio);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_DFL);
    for (int i = 0; i < HIJOS; i++) {
        if (i != hijo)
            close(archivos[i]);
    }

    while (1) {
        if (semop(semid, esperar, 3) == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (hijo == ECO)
            echo(entrada, eco, memoria->bytes, RETARDO_ECO);
        if (hijo == RETARDO_PURO)
            delay(entrada, salida, memoria->bytes, RETARDO);
        if (hijo == FIR)
            fir_low_pass(entrada, salida, memoria->bytes);

        if (guardar_bloque(hijo, hijo == ECO ? eco : salida) == -1) {
            perror(nombres[hijo]);
            break;
        }

        /* Sólo el último hijo avisa al padre. Ambas operaciones son atómicas. */
        int operaciones = 1;
        if (turno == 1)
            operaciones = 2;
        if (semop(semid, avisar, operaciones) == -1)
            break;
    }
    _exit(1);
}

/* Operación: crea un hijo con fork. Entrada: número de hijo.
 * Salida: guarda su PID y devuelve 0, o devuelve -1 si falla fork.
 */
static int crear_hijo(int hijo)
{
    pid_t pid = fork();
    if (pid == -1)
        return -1;
    if (pid == 0)
        procesar_audio(hijo);
    hijos[hijo] = pid;
    fprintf(stderr, "%s: PID %ld\n", nombres[hijo], (long)pid);
    return 0;
}

/* Operación: repone un hijo ya recogido. Entrada: número de hijo.
 * Salida: 0 si se pudo reponer o -1 si se agotaron los intentos o falló fork.
 */
static int reponer_hijo(int hijo)
{
    hijos[hijo] = 0;
    if (reinicios[hijo] == MAX_REINICIOS) {
        fprintf(stderr, "%s: se agotaron los reinicios del bloque\n", nombres[hijo]);
        errno = EIO;
        return -1;
    }
    reinicios[hijo]++;
    return crear_hijo(hijo);
}

/* Operación: recoge y repone los hijos que terminaron. Entrada: tabla de PIDs.
 * Salida: 0 si están activos o -1 ante error.
 */
static int revisar_hijos(void)
{
    for (int i = 0; i < HIJOS; i++) {
        pid_t pid = waitpid(hijos[i], NULL, WNOHANG);
        if (pid == -1 && errno != EINTR)
            return -1;
        if (pid > 0) {
            fprintf(stderr, "%s: terminó antes de tiempo. Reiniciando.\n", nombres[i]);
            if (reponer_hijo(i) == -1)
                return -1;
        }
    }
    return 0;
}

/* Operación: detiene y recoge un hijo. Entrada: número de hijo.
 * Salida: 0 si quedó recogido o -1 ante error.
 */
static int detener_hijo(int hijo)
{
    pid_t pid = hijos[hijo];
    if (kill(pid, SIGKILL) == -1 && errno != ESRCH)
        return -1;
    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR)
            return -1;
    }
    hijos[hijo] = 0;
    return 0;
}

/* Operación: lee el reloj monotónico. Entrada: ninguna. Salida: milisegundos o -1. */
static int64_t ahora_ms(void)
{
    struct timespec reloj;
    if (clock_gettime(CLOCK_MONOTONIC, &reloj) == -1)
        return -1;
    return (int64_t)reloj.tv_sec * 1000 + reloj.tv_nsec / 1000000;
}

/* Operación: publica el bloque y vigila a los hijos hasta la confirmación.
 * Entrada: bloque ya preparado en memoria compartida. Salida: 0 o -1.
 */
static int distribuir_bloque(void)
{
    struct sembuf publicar = {TURNO, HIJOS, 0};
    struct sembuf recibir = {TERMINADO, -1, IPC_NOWAIT};
    int turno_anterior = HIJOS;
    int intentos_anteriores = reinicios[ECO];
    int64_t desde = ahora_ms();

    if (desde == -1 || semop(semid, &publicar, 1) == -1)
        return -1;
    while (1) {
        if (semop(semid, &recibir, 1) == 0)
            return 0;
        if (errno != EAGAIN && errno != EINTR)
            return -1;
        if (revisar_hijos() == -1)
            return -1;

        int turno = semctl(semid, TURNO, GETVAL);
        if (turno == -1)
            return -1;
        if (turno == 0)
            continue;

        int hijo = HIJOS - turno;
        int64_t ahora = ahora_ms();
        if (ahora == -1)
            return -1;
        if (turno != turno_anterior || reinicios[hijo] != intentos_anteriores) {
            desde = ahora;
            turno_anterior = turno;
            intentos_anteriores = reinicios[hijo];
        }

        if (ahora - desde >= LIMITE_MS) {
            int actual = semctl(semid, TURNO, GETVAL);
            if (actual == -1)
                return -1;
            if (actual == turno) {
                fprintf(stderr, "%s: excedió el tiempo. Reiniciando.\n", nombres[hijo]);
                if (detener_hijo(hijo) == -1 || reponer_hijo(hijo) == -1)
                    return -1;
            }
        }
        usleep(1000);
    }
}

/* Operación: captura hasta 20 ms de OSS, vigilando hijos mientras espera.
 * Entrada: descriptor global y buffer de destino.
 * Salida: cantidad de bytes estéreo completos, 0 al terminar o -1 ante error.
 */
static ssize_t capturar_dsp(unsigned char *datos)
{
    size_t leidos = 0;
    while (leidos < BLOQUE && !terminar) {
        fd_set lectura;
        struct timeval espera = {0, 20000};
        FD_ZERO(&lectura);
        FD_SET(audio, &lectura);
        int listo = select(audio + 1, &lectura, NULL, NULL, &espera);
        if (listo == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (revisar_hijos() == -1)
            return -1;
        if (listo == 0)
            continue;
        ssize_t cantidad = read(audio, datos + leidos, BLOQUE - leidos);
        if (cantidad == -1) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            return -1;
        }
        if (cantidad == 0)
            break;
        leidos += cantidad;
    }
    return leidos - leidos % CANALES;
}

/* Operación: abre y valida los tres archivos PCM. Entrada: directorio.
 * Salida: descriptores en archivos[] y 0, o -1 ante error.
 */
static int abrir_salidas(const char *directorio)
{
    struct stat entrada_info;
    struct stat salidas_info[HIJOS];
    if (fstat(audio, &entrada_info) == -1)
        return -1;

    for (int i = 0; i < HIJOS; i++) {
        char ruta[4096];
        int largo = snprintf(ruta, sizeof(ruta), "%s/%s", directorio, nombres[i]);
        if (largo < 0 || largo >= (int)sizeof(ruta)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        archivos[i] = open(ruta, O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0644);
        if (archivos[i] == -1 || fstat(archivos[i], &salidas_info[i]) == -1)
            return -1;
        if (!S_ISREG(salidas_info[i].st_mode) ||
            (salidas_info[i].st_dev == entrada_info.st_dev &&
             salidas_info[i].st_ino == entrada_info.st_ino)) {
            errno = EINVAL;
            return -1;
        }
        for (int j = 0; j < i; j++) {
            if (salidas_info[i].st_dev == salidas_info[j].st_dev &&
                salidas_info[i].st_ino == salidas_info[j].st_ino) {
                errno = EINVAL;
                return -1;
            }
        }
    }
    return 0;
}

/* Operación: configura entrada, archivos, IPC y el lazo de procesamiento.
 * Entrada: WAV o /dev/dsp y directorio de salida opcionales.
 * Salida: tres archivos PCM. Devuelve 0 si termina correctamente o 1 si falla.
 */
int main(int argc, char **argv)
{
    const char *fuente = "/dev/dsp";
    const char *directorio = "/home/tdiii";
    uint32_t restantes = 0;
    off_t completos = 0;
    int bloque_en_curso = 0;
    int resultado = 1;
    union semun inicial = {.val = 0};

    if (argc > 3) {
        fprintf(stderr, "Uso: %s [entrada.wav|/dev/dsp] [directorio_salida]\n", argv[0]);
        return 1;
    }
    if (argc >= 2)
        fuente = argv[1];
    if (argc == 3)
        directorio = argv[2];
    int usar_wav = strcmp(fuente, "/dev/dsp") != 0;

    if (signal(SIGINT, pedir_fin) == SIG_ERR || signal(SIGTERM, pedir_fin) == SIG_ERR)
        goto error;

    if (usar_wav) {
        audio = open(fuente, O_RDONLY);
        if (audio == -1 || preparar_wav(audio, &restantes) == -1) {
            fprintf(stderr, "Se requiere un WAV PCM estéreo de 8 bits a 20000 Hz.\n");
            goto error;
        }
    } else {
        audio = abrir_dsp();
        if (audio == -1) {
            fprintf(stderr, "No se pudo usar /dev/dsp. Puede indicar un WAV.\n");
            goto error;
        }
        if (audio >= FD_SETSIZE) {
            errno = EMFILE;
            goto error;
        }
    }

    shmid = shmget(IPC_PRIVATE, sizeof(*memoria), IPC_CREAT | 0600);
    if (shmid == -1)
        goto error;
    memoria = shmat(shmid, NULL, 0);
    if (memoria == (void *)-1)
        goto error;
    memset(memoria->entrada, SILENCIO, RETARDO);
    memset(memoria->eco, SILENCIO, RETARDO_ECO);

    semid = semget(IPC_PRIVATE, SEMAFOROS, IPC_CREAT | 0600);
    if (semid == -1 || semctl(semid, TURNO, SETVAL, inicial) == -1 ||
        semctl(semid, TERMINADO, SETVAL, inicial) == -1)
        goto error;

    if (abrir_salidas(directorio) == -1)
        goto error;
    for (int i = 0; i < HIJOS; i++) {
        if (ftruncate(archivos[i], 0) == -1 || crear_hijo(i) == -1)
            goto error;
    }

    while (!terminar) {
        unsigned char *entrada = memoria->entrada + RETARDO;
        ssize_t bytes = BLOQUE;
        if (revisar_hijos() == -1)
            goto error;
        if (usar_wav) {
            if (restantes < BLOQUE)
                bytes = restantes;
            if (bytes == 0)
                break;
            if (leer_exacto(audio, entrada, bytes) == -1)
                goto error;
            restantes -= bytes;
        } else {
            bytes = capturar_dsp(entrada);
            if (bytes == -1)
                goto error;
            if (bytes == 0)
                break;
        }

        memoria->bytes = bytes;
        memoria->posicion = completos;
        bloque_en_curso = 1;
        if (distribuir_bloque() == -1)
            goto error;
        bloque_en_curso = 0;
        completos += bytes;
        /* Guardar las últimas muestras para el próximo bloque. */
        memmove(memoria->entrada, memoria->entrada + bytes, RETARDO);
        memmove(memoria->eco, memoria->eco + bytes, RETARDO_ECO);
        for (int i = 0; i < HIJOS; i++)
            reinicios[i] = 0;
    }

    resultado = 0;
    fprintf(stderr, "%lld bytes PCM por archivo.\n", (long long)completos);
    goto cerrar;

error:
    perror("TP 4 audio");

cerrar:
    for (int i = 0; i < HIJOS; i++) {
        if (hijos[i] > 0 && detener_hijo(i) == -1) {
            perror("detener hijo");
            resultado = 1;
        }
    }
    for (int i = 0; i < HIJOS; i++) {
        /* Ante un fallo definitivo, conservar los bloques confirmados. */
        if (bloque_en_curso && ftruncate(archivos[i], completos) == -1)
            perror(nombres[i]);
        if (archivos[i] != -1)
            close(archivos[i]);
    }
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
