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
#include <limits.h>
#define MAX_SIZE 100

//Variaveis
int matrix[MAX_SIZE][MAX_SIZE];
int size;

//Criação da Estrutura de dados para a memoria Partilhada
struct SharedData {
    int bestPath[MAX_SIZE];
    int bestDistance;
    int IterationsNeeded;
    int numTimesBestPathFound;
    int TotalNumberOfIterations;
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



int main(int argc, char *argv[]) {
    //Validação dos argumentos passados por parametro
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <nome_do_ficheiro> <tempo_limite> <num_processos>\n", argv[0]);
        return 1;
    }
    //Parametros passados por parametro
    const char *filename = argv[1];
    int timeLimit = atoi(argv[2]);
    int numProcesses = atoi(argv[3]);

    //Obter a Matrix
    getMatrix(filename);
    //Imprimir a Matrix
    printMatrix();


    //Criar a memoria Partilhada
    int sizeMemory = sizeof( struct SharedData);
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_ANONYMOUS | MAP_SHARED;

    void *shmem = mmap(NULL , sizeMemory , protection , visibility , 0, 0);
    struct SharedData *sharedData =  (struct SharedData *)shmem;
    
    //Criação e implementação do Semaphore
    sem_unlink("mutex");
    sem_t *mutex = sem_open("mutex", O_CREAT, 0644, 1);

    //Criação de processos
    for (int i = 0; i < numProcesses; i++) {
        int pid = fork();
        if (pid == 0) { // Código executado pelos processos filhos
            
            //Variaveis Locais
            struct timeval tvi , tvf , tv_res;
            int newBestDistance = INT_MAX;
            gettimeofday(&tvi , NULL);

            srand(time(NULL));
            int randomPath[size];  //Começar um novo caminho aleatorio
            initializeRandomPath(randomPath);

            time_t startTime = time(NULL); // Começar a contagem do tempo definido
            while (1) {
                
                exchangeMutation(randomPath);
                int mutatedDistance = getDistance(randomPath);//Obter o seu valor de percorrer esse caminho
                
                //Caso este seja menor em distancia que o melhor caminho  anterior então dentro daquele escopo é o melhor
                if (mutatedDistance < newBestDistance) {
                    //Semaphore para controlo da entrada na memoria Partilhada
                    sem_wait(mutex);
                    //Atualização dos valores em memoria Partihada
                    sharedData->bestDistance = mutatedDistance;
                    newBestDistance = mutatedDistance;
                    for (int i = 0; i < size; i++) {
                        sharedData->bestPath[i] = randomPath[i];
                    }   
                    gettimeofday(&tvf , NULL);
                    timersub(&tvf, &tvi, &tv_res);
                    sharedData->timeToBestPath = tv_res;
                    sharedData->IterationsNeeded = sharedData->TotalNumberOfIterations;
                    sem_post(mutex);
                }
                //Caso a distancia percorrida do novo caminho seja igual ao melhor então vamos incrementar na memoria partilhada
                else if(mutatedDistance == newBestDistance) {
                     sem_wait(mutex);
                     sharedData -> numTimesBestPathFound++;
                     sem_post(mutex);
                }
                sem_wait(mutex);
                    sharedData->TotalNumberOfIterations++;
                sem_post(mutex);
                time_t currentTime = time(NULL);
                if (currentTime - startTime >= timeLimit) {
                    break;
                }
            }
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
    printf("\nNumero de vezes econtrado: %d" , sharedData->numTimesBestPathFound + 1);
    printf("\nNumero de total de Iteracoes: %d" , sharedData->TotalNumberOfIterations + 1);
    printf("\nTempo necessario: %ld.%06ld s", (long)sharedData->timeToBestPath.tv_sec, (long)sharedData->timeToBestPath.tv_usec);
    printf("\n-----------------------------------\n");
    return 0;
}
