#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#define MAX_SIZE 100

//Variaveis
int matrix[MAX_SIZE][MAX_SIZE];
int size;

struct SharedData {
    int bestPath[MAX_SIZE];
    int bestDistance;
    int IterationsNeeded;
    struct timeval timeToBestPath;
};

//Imprimir a matrix
void printMatrix() {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
}

//Obter um caminho aleatorio
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

//Obter a Matrix do Ficheiro de Texto
void getMatrix(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Erro ao abrir Ficheiro");
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
//Função para Obter a distancia 
int getDistance(int path[]) {

    int totalDistance = 0;
    int start;
    int end;

  for (int i = 0; i < size - 1; i++) {

        int start = path[i] - 1;
        int end = path[i + 1] - 1;
        
        totalDistance += matrix[start][end];
    }
    // Adiciona a distância da última cidade de volta à cidade inicial
    totalDistance += matrix[path[size - 1] - 1][path[0] - 1];
    return totalDistance;
}

void  AJ_PE(int timeLimit, struct SharedData *sharedData , sem_t *mutex) {

    int iterations = 0;
    struct timeval tvi , tvf , tv_res;
    gettimeofday(&tvi , NULL);
    
    // Calcular o Primeiro caminho
    srand(time(NULL));
    int randomInitialPath[size];
    initializeRandomPath(randomInitialPath);
    int initialDistance = getDistance(randomInitialPath);
   

    gettimeofday(&tvf , NULL);

    timersub(&tvf , &tvi , &tv_res);

    //Colocar valores inciais na memoria partilhada
    sharedData-> bestDistance = initialDistance;
    for (int i = 0; i < size; i++) {
    sharedData->bestPath[i] = randomInitialPath[i];
    }   
    sharedData-> IterationsNeeded = 0;
    sharedData->timeToBestPath = tv_res;


     time_t startTime = time(NULL); // Começar a contagem do tempo
    while (1) {
      
        int randomPath[size];
        initializeRandomPath(randomPath);
        int mutatedDistance = getDistance(randomPath);
        

        if (mutatedDistance < initialDistance) {
            sem_wait(mutex);
            sharedData->bestDistance = mutatedDistance;
            initialDistance = mutatedDistance;
            for (int i = 0; i < size; i++) {
                sharedData->bestPath[i] = randomPath[i];
            }   
            gettimeofday(&tvf , NULL);
            timersub(&tvf, &tvi, &tv_res);
            sharedData->timeToBestPath = tv_res;
            sharedData->IterationsNeeded = iterations;
            sem_post(mutex);
        }
       iterations++;
    

        time_t currentTime = time(NULL);
        if (currentTime - startTime >= timeLimit) {
            printf("Tempo limite excedido. A interromper interações...\n");
            break;
        }
    }
}



int main(int argc, char *argv[]) {

    

       if (argc != 4) {
        fprintf(stderr, "Usage: %s <nome_do_ficheiro> <tempo_limite> <num_processos>\n", argv[0]);
        return 1;
    }

     const char *filename = argv[1];
    int timeLimit = atoi(argv[2]);
    int numProcesses = atoi(argv[3]);

    getMatrix(filename);
    printMatrix();


    //Criar a memoria Partilhada
    int sizeMemory = sizeof( struct SharedData);
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_ANONYMOUS | MAP_SHARED;


    void *shmem = mmap(NULL , sizeMemory , protection , visibility , 0, 0);
    struct SharedData *sharedData =  (struct SharedData *)shmem;
    

    sem_unlink("mutex");
    sem_t *mutex = sem_open("mutex", O_CREAT, 0644, 1);

    for (int i = 0; i < numProcesses; i++) {
        pid_t pid = fork();
        if (pid == 0) { // Código executado pelos processos filhos
            AJ_PE(timeLimit , sharedData , mutex);
            exit(0);
        } else if (pid < 0) {
            perror("Erro ao criar processo");
            exit(1);
        }
    }
   for (int i = 0; i < numProcesses; i++) {
        wait(NULL); 
    }

    printf("-----------------------------------\n");
    printf("Melhor Resultado :\n");
     printf("-----------------------------------\n");
    printf("Melhor caminho encontrado:");
     for (int i = 0; i < size ; i++){
        printf("%d " , sharedData->bestPath[i]);
     }
    printf("\nValor do Caminho: %d" , sharedData->bestDistance );
    printf("\nIterações necessarias: %d" , sharedData->IterationsNeeded + 1 );
    printf("\nTempo necessario: %ld ms", (long)sharedData -> timeToBestPath.tv_usec /1000 );
    printf("\n-----------------------------------\n");
    return 0;
}
