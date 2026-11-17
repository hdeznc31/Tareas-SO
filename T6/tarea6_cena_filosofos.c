
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define N 5
#define COMIDAS 3

// Estados de los filosofos
#define THINKING 0
#define HUNGRY 1
#define EATING 2

// Variables compartidas
int state[N];

// Semaforo mutex y semaforos individuales
sem_t mutex;
sem_t sem_filosofos[N];

// Datos para identificar cada hilo
typedef struct {
    int id;
} ThreadData;

// Esperar un semaforo, manejando interrupciones
void esperar(sem_t *sem)
{
    while (sem_wait(sem) == -1)
    {
        if (errno == EINTR)
            continue;

        perror("Error en sem_wait");
        exit(EXIT_FAILURE);
    }
}

// Pausa para visualizar la simulacion
void pausa_corta(void)
{
    struct timespec tiempo = {0, 200000000L};
    nanosleep(&tiempo, NULL);
}

// Obtener el filosofo de la izquierda
int izquierda(int i)
{
    return (i + N - 1) % N;
}

// Obtener el filosofo de la derecha
int derecha(int i)
{
    return (i + 1) % N;
}

// ======================================
// VERIFICAR SI UN FILOSOFO PUEDE COMER
// ======================================
void test(int i)
{
    if (state[i] == HUNGRY &&
        state[izquierda(i)] != EATING &&
        state[derecha(i)] != EATING)
    {
        state[i] = EATING;

        sem_post(&sem_filosofos[i]);
    }
}

// ======================================
// TOMAR LOS TENEDORES
// ======================================
void take_forks(int i)
{
    esperar(&mutex);

    state[i] = HUNGRY;

    printf("Filosofo %d tiene hambre y solicita sus tenedores.\n", i + 1);
    fflush(stdout);

    test(i);

    sem_post(&mutex);

    // Esperar hasta que pueda comer
    esperar(&sem_filosofos[i]);
}

// ======================================
// SOLTAR LOS TENEDORES
// ======================================
void put_forks(int i)
{
    esperar(&mutex);

    // El filosofo termina de comer
    state[i] = THINKING;

    printf("Filosofo %d suelta sus tenedores y comienza a pensar.\n", i + 1);
    fflush(stdout);

    // Revisar si los vecinos pueden comer
    test(izquierda(i));
    test(derecha(i));

    sem_post(&mutex);
}

// ======================================
// FUNCION DE CADA FILOSOFO
// ======================================
void *filosofo(void *arg)
{
    ThreadData *datos = (ThreadData *)arg;
    int id = datos->id;

    for (int i = 1; i <= COMIDAS; i++)
    {
        // Pensar
        printf("Filosofo %d esta pensando (ronda %d).\n", id + 1, i);
        fflush(stdout);

        pausa_corta();

        // Solicitar los dos tenedores
        take_forks(id);

        // Comer
        printf("Filosofo %d esta COMIENDO (ronda %d).\n", id + 1, i);
        fflush(stdout);

        pausa_corta();

        // Liberar los tenedores
        put_forks(id);
    }

    printf("Filosofo %d termino sus comidas.\n", id + 1);
    fflush(stdout);

    return NULL;
}

// ======================================
// FUNCION PRINCIPAL
// ======================================
int main(void)
{
    pthread_t hilos[N];
    ThreadData datos[N];

    printf("========================================\n");
    printf("       CENA DE LOS FILOSOFOS\n");
    printf("========================================\n");
    printf("Numero de filosofos: %d\n", N);
    printf("Numero de tenedores: %d\n", N);
    printf("Comidas por filosofo: %d\n\n", COMIDAS);

    // Inicializar el semaforo mutex
    if (sem_init(&mutex, 0, 1) == -1)
    {
        perror("Error al inicializar mutex");
        return EXIT_FAILURE;
    }

    // Inicializar estados y semaforos individuales
    for (int i = 0; i < N; i++)
    {
        state[i] = THINKING;

        if (sem_init(&sem_filosofos[i], 0, 0) == -1)
        {
            perror("Error al inicializar semaforo individual");
            return EXIT_FAILURE;
        }
    }

    // Crear los cinco hilos
    for (int i = 0; i < N; i++)
    {
        datos[i].id = i;

        int rc = pthread_create(
            &hilos[i],
            NULL,
            filosofo,
            &datos[i]
        );

        if (rc != 0)
        {
            fprintf(stderr, "Error al crear hilo: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
    }

    // Esperar a que terminen los cinco filosofos
    for (int i = 0; i < N; i++)
    {
        int rc = pthread_join(hilos[i], NULL);

        if (rc != 0)
        {
            fprintf(stderr, "Error al esperar hilo: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
    }

    // Destruir los semaforos
    for (int i = 0; i < N; i++)
    {
        sem_destroy(&sem_filosofos[i]);
    }

    sem_destroy(&mutex);

    printf("\n========================================\n");
    printf("Todos los filosofos terminaron correctamente.\n");
    printf("No se produjo bloqueo mutuo (Deadlock).\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
