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
#include <signal.h>
#include <limits.h>
#define MAX_SIZE 100



//Criação da Estrutura de dados para a memoria Partilhada
struct SharedData {
    int bestPath[MAX_SIZE];
    int bestDistance;
    int IterationsNeeded;
    int numTimesBestPathFound;
    int TotalNumberOfIterations;
    struct timeval timeToBestPath;
};


//Variaveis
int matrix[MAX_SIZE][MAX_SIZE];
int size , pid , i , newBestDistance = INT_MAX , numProcessos;

pid_t  *childPIDs;
sem_t *mutex;
sem_t *mutex_2;
void *shmem ;
struct SharedData *sharedData;
int tempoLimite = 0;

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
    //Ler somente inteiros no ficheiro
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
//Função para tratamento de Sinais
void signal_handler(int signal) {
        for (int j = 0; j < numProcessos; j++) {    
            kill(childPIDs[j], SIGUSR2);
           
        }
}
void signal_handlerS2(int signal) {

    sem_wait(mutex_2);
        newBestDistance = sharedData->bestDistance;
    sem_post(mutex_2);
}


//Função para tratamento do Alarm Signal
void alarm_handler(int signal) {
    sem_wait(mutex);
        tempoLimite = 1;
    sem_post(mutex);
    for (int j = 0; j < numProcessos; j++) {
        kill(childPIDs[j], SIGTERM);
    }
}
//Função para tratamento do Sinal que termina os processos
void termination_handler(int signal) {
    exit(0);
}

int main(int argc, char *argv[]) {
    //Validação dos argumentos passados por parametro
    const char *filename = argv[1];
    time_t timeLimit = atoi(argv[2]);
    numProcessos = atoi(argv[3]);
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <nome_do_ficheiro> <tempo_limite> <num_processos>\n", argv[0]);
        return 1;
    }
    //Sinais
   signal(SIGUSR2, signal_handlerS2);
   signal(SIGUSR1, signal_handler);
   signal(SIGALRM, alarm_handler);
   signal(SIGTERM, termination_handler);
   alarm(timeLimit);

   //Semaphores
    sem_unlink("mutex");
    sem_unlink("mutex2");
    mutex = sem_open("mutex", O_CREAT, 0644, 1);
    mutex_2 = sem_open("mutex2", O_CREAT, 0644, 1);

    //Obter a Matrix
    getMatrix(filename);
    //Imprimir a Matrix
    printMatrix();


    //Criar a memoria Partilhada
    int sizeMemory = sizeof( struct SharedData);
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_ANONYMOUS | MAP_SHARED;

    shmem = mmap(NULL , sizeMemory , protection , visibility , 0, 0);
    sharedData =  (struct SharedData *)shmem;
    
    //Alocação dinamica de memoria para o array de PIDs
    childPIDs = malloc(numProcessos * sizeof(pid_t));

    for (int i = 0; i < numProcessos; i++) {
        pid = fork();
        if (pid == 0) { // Código executado pelos processos filhos
            //Variaveis
            struct timeval tvi , tvf , tv_res;
            gettimeofday(&tvi , NULL);
            
            srand(time(NULL));
            int randomPath[size];
            initializeRandomPath(randomPath);

            time_t startTime = time(NULL); // Começar a contagem do tempo definido
            while (!tempoLimite) {
               
                exchangeMutation(randomPath);
                int mutatedDistance = getDistance(randomPath);
              
                if (mutatedDistance < newBestDistance) {
                    sem_wait(mutex);
                    if(mutatedDistance < newBestDistance){
                        sharedData->bestDistance = mutatedDistance;
                        for (int i = 0; i < size; i++) {
                            sharedData->bestPath[i] = randomPath[i];
                        }   
                        gettimeofday(&tvf , NULL);
                        timersub(&tvf, &tvi, &tv_res);
                        sharedData->timeToBestPath = tv_res;
                        sharedData->IterationsNeeded = sharedData->TotalNumberOfIterations;
                    
                        sem_post(mutex);
                        
                        kill(getppid(), SIGUSR1);
                        pause();
                    }else sem_post(mutex);
                  
                    
                }
                else if(mutatedDistance == newBestDistance) {
                    sem_wait(mutex);
                        sharedData -> numTimesBestPathFound++;
                    sem_post(mutex);
                }
                sem_wait(mutex);
                    sharedData->TotalNumberOfIterations++;
                sem_post(mutex);
            }
           
            exit(0); //Termina o Processo
        } else if (pid < 0) {
            perror("Erro ao criar processo");
            exit(1);
        }
        else if (pid > 0) { // Código executado pelo processo pai
            childPIDs[i] = pid;
        }
       
    }
    for (int i = 0; i < numProcessos; i++) {
        wait(NULL);
    }

    printf("--------------------------------------------------------------\n");
    printf("Melhor Resultado\n");
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


    //Libertação de memoria 
    free(childPIDs);
    sem_close(mutex);
    sem_unlink("mutex");
    sem_close(mutex_2);
    sem_unlink("mutex2");
    munmap(shmem, sizeMemory);
    
    return 0;
}

