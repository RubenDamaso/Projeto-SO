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

#define MAX_SIZE 100



//Criação da Estrutura de dados para a memoria Partilhada
struct SharedData {
    int bestPath[MAX_SIZE];
    int bestDistance;
    int IterationsNeeded;
    struct timeval timeToBestPath;
};


//Variaveis
int matrix[MAX_SIZE][MAX_SIZE];
int size , pid , i , newBestDistance , numProcesses;

pid_t  *childPIDs;
sem_t *mutex;
void *shmem ;
struct SharedData *sharedData;


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

void signal_handler(int signal) {
        if (signal == SIGUSR1) {
            for (int j = 0; j < numProcesses; j++) {
             
                kill(childPIDs[j], SIGUSR2);
                
            }
        }
        else if(signal == SIGUSR2){
            
            sem_wait(mutex); 
            newBestDistance = sharedData->bestDistance;
            sem_post(mutex);
        }
}



int main(int argc, char *argv[]) {

   signal(SIGUSR2, signal_handler);
   signal(SIGUSR1, signal_handler);

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <nome_do_ficheiro> <tempo_limite> <num_processos>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int timeLimit = atoi(argv[2]);
    numProcesses = atoi(argv[3]);
  
 

    sem_unlink("mutex");
    mutex = sem_open("mutex", O_CREAT, 0644, 1);

    getMatrix(filename);
    printMatrix();


    //Criar a memoria Partilhada
    int sizeMemory = sizeof( struct SharedData);
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_ANONYMOUS | MAP_SHARED;


    shmem = mmap(NULL , sizeMemory , protection , visibility , 0, 0);
    sharedData =  (struct SharedData *)shmem;
    
    childPIDs = malloc(numProcesses * sizeof(pid_t));
     if (childPIDs == NULL) {
        perror("Error allocating memory for childPIDs");
        exit(1);
    }

    for (int i = 0; i < numProcesses; i++) {
        int pid = fork();
        if (pid == 0) { // Código executado pelos processos filhos
             //Variaveis
             
            int iterations = 0;
            struct timeval tvi , tvf , tv_res;
            gettimeofday(&tvi , NULL);
            
            // Calcular o Primeiro caminho
            /*Assumimos que o primeiro caminho é o melhor dentro do escopo daquele processo*/
            srand(time(NULL));
            int randomInitialPath[size];
            initializeRandomPath(randomInitialPath);
            int newBestDistance = getDistance(randomInitialPath);
            gettimeofday(&tvf , NULL);
            timersub(&tvf , &tvi , &tv_res);


            time_t startTime = time(NULL); // Começar a contagem do tempo definido
            while (1) {
            
                int randomPath[size];
                initializeRandomPath(randomPath);
                int mutatedDistance = getDistance(randomPath);
              

                if (mutatedDistance < newBestDistance) {
                    
                    sem_wait(mutex);
                    sharedData->bestDistance = mutatedDistance;
                    newBestDistance = mutatedDistance;
                    for (int i = 0; i < size; i++) {
                        sharedData->bestPath[i] = randomPath[i];
                    }   
                    gettimeofday(&tvf , NULL);
                    timersub(&tvf, &tvi, &tv_res);
                    sharedData->timeToBestPath = tv_res;
                    sharedData->IterationsNeeded = iterations;
                    sem_post(mutex);

                    kill(getppid(), SIGUSR1);
                    pause();
                    
                }
                iterations++;

                time_t currentTime = time(NULL);
                if (currentTime - startTime >= timeLimit) {
                    printf("Tempo limite excedido. A interromper interações...\n");
                    break;
                }
            }
                    exit(0);
        } else if (pid < 0) {
            perror("Erro ao criar processo");
            exit(1);
        }
        childPIDs[i] = pid;
    }
   for (int i = 0; i < numProcesses; i++) {
        int pid = wait(NULL);
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
    printf("\nTempo necessario: %ld ms", (long)sharedData -> timeToBestPath.tv_usec / 1000 );
    printf("\n-----------------------------------\n");

    free(childPIDs);
    sem_close(mutex);
    sem_unlink("mutex");
    return 0;
}

