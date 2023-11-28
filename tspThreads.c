#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>

#define MAX_SIZE 100

struct SharedData {
    int bestPath[MAX_SIZE];
    int bestDistance;
    int IterationsNeeded;
    int numTimesBestPathFound;
    int TotalNumberOfIterations;
    struct timeval timeToBestPath;
};

int matrix[MAX_SIZE][MAX_SIZE];
int size, newBestDistance = INT_MAX, numThreads;
pthread_t *threadIDs;
sem_t mutex;
int tempoLimite = 0;
struct SharedData *sharedData;
struct timeval startTime;
time_t timeLimit;

//Função para imprimir Matix
void printMatrix() {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
}
//Função para iniciar um caminho random
void initializeRandomPath(int path[]) {
    for (int i = 0; i < size; i++) {
        path[i] = i + 1;
    }

    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = path[i];
        path[i] = path[j];
        path[j] = temp;
    }
}
//Função para obter a Matrix do ficheiro de texto
void getMatrix(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    fscanf(file, "%d", &size);

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            fscanf(file, "%d", &matrix[i][j]);
        }
    }

    fclose(file);
}
//Função para obter a distancia
int getDistance(int path[]) {
    int totalDistance = 0;

    for (int i = 0; i < size - 1; i++) {
        int start = path[i] - 1;
        int end = path[i + 1] - 1;
        totalDistance += matrix[start][end];
    }

    totalDistance += matrix[path[size - 1] - 1][path[0] - 1];
    return totalDistance;
}
//Função para trocar as posições do caminho dado como parametro
void exchangeMutation(int path[]) {
    int pos1 = rand() % size;
    int pos2;
    do {
        pos2 = rand() % size;
    } while (pos1 == pos2);

    int temp = path[pos1];
    path[pos1] = path[pos2];
    path[pos2] = temp;
}
//Função a correr por cada thread
void *thread_function(void *arg) {
    struct timeval tvi, tvf, tv_res;
    gettimeofday(&tvi, NULL);

    srand(time(NULL));
    int randomPath[size];
    initializeRandomPath(randomPath);

    while (!tempoLimite) {
        exchangeMutation(randomPath);
        int mutatedDistance = getDistance(randomPath);

        sem_wait(&mutex);
        if (mutatedDistance < newBestDistance) {
            newBestDistance = mutatedDistance;
            sharedData->bestDistance = mutatedDistance;
            for (int i = 0; i < size; i++) {
                sharedData->bestPath[i] = randomPath[i];
            }
            gettimeofday(&tvf, NULL);
            timersub(&tvf, &tvi, &tv_res);
            sharedData->timeToBestPath = tv_res;
            sharedData->IterationsNeeded = sharedData->TotalNumberOfIterations;
            sem_post(&mutex);
        } else {
            sem_post(&mutex);
        }

        sem_wait(&mutex);
        if (mutatedDistance == newBestDistance) {
            sharedData->numTimesBestPathFound++;
        }
        sharedData->TotalNumberOfIterations++;
        sem_post(&mutex);

        gettimeofday(&tvf, NULL);
        timersub(&tvf, &startTime, &tv_res);

        if (tv_res.tv_sec >= timeLimit) {
            sem_wait(&mutex);
            tempoLimite = 1;
            sem_post(&mutex);
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    //Validação dos argumentos passados por parametro
    const char *filename = argv[1];
    timeLimit = atoi(argv[2]);
    numThreads = atoi(argv[3]);

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <filename> <time_limit> <num_threads>\n", argv[0]);
        return 1;
    }
    //Inicialização do Semaphore
    sem_init(&mutex, 0, 1);

    getMatrix(filename);
    printMatrix();

    sharedData = malloc(sizeof(struct SharedData));

    threadIDs = malloc(numThreads * sizeof(pthread_t));

    gettimeofday(&startTime, NULL);

    for (int i = 0; i < numThreads; i++) {
        if (pthread_create(&threadIDs[i], NULL, thread_function, NULL) != 0) {
            perror("Error ao Criar threads");
            exit(1);
        }
    }

    for (int i = 0; i < numThreads; i++) {
        if (pthread_join(threadIDs[i], NULL) != 0) {
            perror("Erro ao terminar threads");
            exit(1);
        }
    }

    printf("--------------------------------------------------------------\n");
    printf("Melhor Resultado da matrix ( %s )\n" , filename);
    printf("-------------------------------------------------------------\n");
    printf("Melhor caminho encontrado:");
    for (int i = 0; i < size ; i++){
        printf("%d " , sharedData->bestPath[i]);
    }
    printf("\nValor do Caminho: %d" , sharedData->bestDistance );
    printf("\nIterações necessarias: %d" , sharedData->IterationsNeeded + 1 );
    printf("\nNumero de vezes encontrado: %d" , sharedData->numTimesBestPathFound + 1);
    printf("\nNumero de total de Iteracoes: %d" , sharedData->TotalNumberOfIterations + 1);
    printf("\nTempo necessario: %ld.%06ld s", (long)sharedData->timeToBestPath.tv_sec, (long)sharedData->timeToBestPath.tv_usec);
    printf("\n------------------------------------------------------------\n");


    free(sharedData);
    free(threadIDs);

    sem_destroy(&mutex);

    return 0;
}
