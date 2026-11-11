#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 5
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define TOTAL_ITEMS 15
#define FIN -1

// Buffer compartido
int buffer[BUFFER_SIZE];
int front = 0;
int rear = 0;

// Semaforos POSIX
sem_t espacios;
sem_t elementos;
sem_t mutex;

// Identificacion de cada hilo
typedef struct {
    int id;
} ThreadData;

// Funcion para esperar un semaforo
int esperar_semaforo(sem_t *sem)
{
    while (sem_wait(sem) == -1)
    {
        if (errno == EINTR)
            continue;

        return -1;
    }

    return 0;
}

// Pausa para visualizar la concurrencia
void pausa_corta(void)
{
    struct timespec pausa = {0, 100000000L};
    nanosleep(&pausa, NULL);
}

// =====================================
// FUNCION DEL PRODUCTOR
// =====================================
void *productor(void *arg)
{
    ThreadData *datos = (ThreadData *)arg;

    // Repartir los 15 productos entre los dos productores
    for (int item = datos->id;
         item <= TOTAL_ITEMS;
         item += NUM_PRODUCERS)
    {
        int producto = item * 10;

        if (esperar_semaforo(&espacios) == -1)
        {
            perror("Error esperando espacios");
            return NULL;
        }

        if (esperar_semaforo(&mutex) == -1)
        {
            perror("Error esperando mutex");
            sem_post(&espacios);
            return NULL;
        }

        int posicion = rear;

        buffer[rear] = producto;
        rear = (rear + 1) % BUFFER_SIZE;

        printf("[+] Productor %d produjo: %d en posicion %d\n",
               datos->id, producto, posicion);

        fflush(stdout);

        sem_post(&mutex);
        sem_post(&elementos);

        pausa_corta();
    }

    return NULL;
}

// =====================================
// FUNCION DEL CONSUMIDOR
// =====================================
void *consumidor(void *arg)
{
    ThreadData *datos = (ThreadData *)arg;

    while (1)
    {
        if (esperar_semaforo(&elementos) == -1)
        {
            perror("Error esperando elementos");
            return NULL;
        }

        if (esperar_semaforo(&mutex) == -1)
        {
            perror("Error esperando mutex");
            sem_post(&elementos);
            return NULL;
        }

        int posicion = front;
        int producto = buffer[front];

        front = (front + 1) % BUFFER_SIZE;

        if (producto == FIN)
        {
            printf("[-] Consumidor %d termino su ejecucion\n",
                   datos->id);
        }
        else
        {
            printf("[-] Consumidor %d consumio: %d de la posicion %d\n",
                   datos->id, producto, posicion);
        }

        fflush(stdout);

        sem_post(&mutex);
        sem_post(&espacios);

        if (producto == FIN)
            break;

        pausa_corta();
    }

    return NULL;
}

// =====================================
// FUNCION PRINCIPAL
// =====================================
int main(void)
{
    pthread_t productores[NUM_PRODUCERS];
    pthread_t consumidores[NUM_CONSUMERS];

    ThreadData datos_productores[NUM_PRODUCERS];
    ThreadData datos_consumidores[NUM_CONSUMERS];

    int rc;

    printf("INICIO DEL PROBLEMA PRODUCTOR-CONSUMIDOR\n\n");

    // Inicializar semaforos compartidos entre hilos
    if (sem_init(&espacios, 0, BUFFER_SIZE) == -1)
    {
        perror("Error inicializando espacios");
        return EXIT_FAILURE;
    }

    if (sem_init(&elementos, 0, 0) == -1)
    {
        perror("Error inicializando elementos");
        sem_destroy(&espacios);
        return EXIT_FAILURE;
    }

    if (sem_init(&mutex, 0, 1) == -1)
    {
        perror("Error inicializando mutex");
        sem_destroy(&elementos);
        sem_destroy(&espacios);
        return EXIT_FAILURE;
    }

    // Crear dos consumidores
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        datos_consumidores[i].id = i + 1;

        rc = pthread_create(
            &consumidores[i],
            NULL,
            consumidor,
            &datos_consumidores[i]
        );

        if (rc != 0)
        {
            fprintf(stderr, "Error creando consumidor: %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    // Crear dos productores
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        datos_productores[i].id = i + 1;

        rc = pthread_create(
            &productores[i],
            NULL,
            productor,
            &datos_productores[i]
        );

        if (rc != 0)
        {
            fprintf(stderr, "Error creando productor: %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    // Esperar a que terminen los productores
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        rc = pthread_join(productores[i], NULL);

        if (rc != 0)
        {
            fprintf(stderr, "Error esperando productor: %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    printf("\nLos dos productores terminaron.\n");

    // Enviar una senal de finalizacion a cada consumidor
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        if (esperar_semaforo(&espacios) == -1)
        {
            perror("Error esperando espacio");
            exit(EXIT_FAILURE);
        }

        if (esperar_semaforo(&mutex) == -1)
        {
            perror("Error esperando mutex");
            sem_post(&espacios);
            exit(EXIT_FAILURE);
        }

        buffer[rear] = FIN;
        rear = (rear + 1) % BUFFER_SIZE;

        sem_post(&mutex);
        sem_post(&elementos);
    }

    // Esperar a que terminen los consumidores
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        rc = pthread_join(consumidores[i], NULL);

        if (rc != 0)
        {
            fprintf(stderr, "Error esperando consumidor: %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    // Destruir semaforos
    sem_destroy(&espacios);
    sem_destroy(&elementos);
    sem_destroy(&mutex);

    printf("\nTodos los hilos terminaron correctamente.\n");

    return EXIT_SUCCESS;
}
