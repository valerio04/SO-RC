// il fatto che il mainProcess aspetti tutti  i thread che si avviano mandando un sem_post a mainProcess che fa un ciclo da o a n con sem_wait spettandoli tutti, io l ho implemtetato semplicemnet ceh prima fa il ciclo di avvio dei processi e solo dopo aver finito avvia i processi. I ogn caso è banale implemtare l altro metodo pk basta aggiungere un semaforo e un ciclo in in piu in MainProcess


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
// macros for error handling
#include "common.h"

#define N 100   // child process count
#define M 10    // thread per child process count
#define T 3     // time to sleep for main process

#define FILENAME	"accesses.log"

/*
 * data structure required by threads
 */
typedef struct thread_args_s {
    unsigned int child_id;
    unsigned int thread_id;
} thread_args_t;

/*
 * parameters can be set also via command-line arguments
 */
int n = N, m = M, t = T;

/* TODO: declare as many semaphores as needed to implement
 * the intended semantics, and choose unique identifiers for
 * them (e.g., "/mysem_critical_section") */
sem_t* sem_Partenza;
sem_t* sem_Arrivo;
sem_t* sem_Scrittura;


/* TODO: declare a shared memory and the data type to be placed 
 * in the shared memory, and choose a unique identifier for
 * the memory (e.g., "/myshm") 
 * Declare any global variable (file descriptor, memory 
 * pointers, etc.) needed for its management
 */

typedef struct{
        int TerminaThread;
    } fine_t;
fine_t* shr_var;
int fd;
int ret;
/*
 * Ensures that an empty file with given name exists.
 */
void init_file(const char *filename) {
    printf("[Main] Initializing file %s...", FILENAME);
    fflush(stdout);
    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd<0) handle_error("error while initializing file");
    close(fd);
    printf("closed...file correctly initialized!!!\n");
}

/*
 * Create a named semaphore with a given name, mode and initial value.
 * Also, tries to remove any pre-existing semaphore with the same name.
 */
sem_t *create_named_semaphore(const char *name, mode_t mode, unsigned int value) {
    printf("[Main] Creating named semaphore %s...", name);
    fflush(stdout);
    
    //pr sicurezza:
    sem_unlink(name);
    // TODO
    sem_t* sem = sem_open(name, O_CREAT | O_EXCL, mode, value);

    if(sem==SEM_FAILED){
        handle_error("sem creazione");
    }

    printf("done!!!\n");
    return sem;
}



void parseOutput() {
    // identify the child that accessed the file most times
    int* access_stats = calloc(n, sizeof(int)); // initialized with zeros
    printf("[Main] Opening file %s in read-only mode...", FILENAME);
	fflush(stdout);
    int fd = open(FILENAME, O_RDONLY);
    if (fd < 0) handle_error("error while opening output file");
    printf("ok, reading it and updating access stats...");
	fflush(stdout);

    size_t read_bytes;
    int index;
    do {
        read_bytes = read(fd, &index, sizeof(int));
        if (read_bytes > 0)
            access_stats[index]++;
    } while(read_bytes > 0);
    printf("ok, closing it...");
	fflush(stdout);

    close(fd);
    printf("closed!!!\n");

    int max_child_id = -1, max_accesses = -1;
    for (int i = 0; i < n; i++) {
        printf("[Main] Child %d accessed file %s %d times\n", i, FILENAME, access_stats[i]);
        if (access_stats[i] > max_accesses) {
            max_accesses = access_stats[i];
            max_child_id = i;
        }
    }
    printf("[Main] ===> The process that accessed the file most often is %d (%d accesses)\n", max_child_id, max_accesses);
    free(access_stats);
}

void* thread_function(void* x) {
    thread_args_t *args = (thread_args_t*)x;
     
    /* TODO: protect the critical section using semaphore(s) as needed */
    ret=sem_wait(sem_Scrittura);
    if(ret) handle_error("sem");
    // open file, write child identity and close file
    int fd = open(FILENAME, O_WRONLY | O_APPEND);
    if (fd < 0) handle_error("error while opening file");
    printf("[Child#%d-Thread#%d] File %s opened in append mode!!!\n", args->child_id, args->thread_id, FILENAME);	

    write(fd, &(args->child_id), sizeof(int));
    printf("[Child#%d-Thread#%d] %d appended to file %s opened in append mode!!!\n", args->child_id, args->thread_id, args->child_id, FILENAME);	

    close(fd);
    printf("[Child#%d-Thread#%d] File %s closed!!!\n", args->child_id, args->thread_id, FILENAME);

    ////
    ret=sem_post(sem_Scrittura);
    if(ret) handle_error("sem");
    ////
    free(x);
    pthread_exit(NULL);
}

void mainProcess() {
    /* TODO: the main process waits for all the children to start,
     * it notifies them to start their activities, and sleeps
     * for some time t. Once it wakes up, it notifies the children
     * to end their activities, and waits for their termination.
     * Finally, it calls the parseOutput() method and releases
     * any shared resources. */


   

    for(int i =0 ; i<n; i++){
        ret=sem_post(sem_Partenza);
        if(ret) handle_error("sem");
    }

    sleep(t);
    shr_var->TerminaThread=1;
    
    
    for(int i =0 ; i<n; i++){
        ret=sem_wait(sem_Arrivo);
        if(ret) handle_error("sem");
    }
    
    

    
    
    //per buona norma per evitare processi zombie
    for (int i = 0; i < n; i++) {
        int status;
        if (wait(&status) == -1) handle_error("wait");
    }

    
    parseOutput();



  
    sem_close(sem_Partenza);
    sem_close(sem_Arrivo);
    sem_close(sem_Scrittura);
    sem_unlink("/sem_Partenza");
    sem_unlink("/sem_Arrivo");
    sem_unlink("/sem_Scrittura");

    if (munmap(shr_var, sizeof(fine_t)) != 0) handle_error("munmap");
    if (close(fd) != 0) handle_error("close shm fd");
    if (shm_unlink("/fine") != 0) handle_error("shm_unlink");

    

}

void childProcess(int child_id) {
    /* TODO: each child process notifies the main process that it
     * is ready, then waits to be notified from the main in order
     * to start. As long as the main process does not notify a
     * termination event [hint: use sem_getvalue() here], the child
     * process repeatedly creates m threads that execute function
     * thread_function() and waits for their completion. When a
     * notification has arrived, the child process notifies the main
     * process that it is about to terminate, and releases any
     * shared resources before exiting. */

    sem_Partenza  = sem_open("/sem_Partenza", 0);  // metto zero perche devo solo aprirli
    sem_Arrivo    = sem_open("/sem_Arrivo", 0);  // metto zero perche devo solo aprirli
    sem_Scrittura = sem_open("/sem_Scrittura", 0);  // metto zero perche devo solo aprirli

    if (sem_Partenza == SEM_FAILED || sem_Arrivo == SEM_FAILED || sem_Scrittura == SEM_FAILED)
        handle_error("sem_open in child");


    ret=sem_wait(sem_Partenza);
    if(ret) handle_error("sem");
    
    fd = shm_open("/fine", O_RDONLY, 0660);
    if (fd == -1) handle_error("shm_open child");
    shr_var = mmap(NULL, sizeof(fine_t), PROT_READ, MAP_SHARED, fd, 0);
    if (shr_var == MAP_FAILED) handle_error("mmap child");

    while(1){

        pthread_t lista_thread[m];
        for(int i=0; i<m; i++){
            thread_args_t* t_arg= (thread_args_t*) malloc(sizeof(thread_args_t));
            t_arg->child_id=child_id;
            t_arg->thread_id=i;
            
            if(pthread_create(&lista_thread[i],NULL,thread_function,(void*) t_arg)!=0)
                handle_error("pthread_create");
        }
        for(int i=0; i<m; i++){
            if(pthread_join(lista_thread[i],NULL)!=0) handle_error("pthread_join");
        }
        if(shr_var->TerminaThread==1){
            sem_post(sem_Arrivo);
            break;
        }   

        
    }
    
    
    sem_close(sem_Partenza);
    sem_close(sem_Arrivo);
    sem_close(sem_Scrittura);

    if (munmap(shr_var, sizeof(fine_t)) != 0) handle_error("munmap");
    if (close(fd) != 0) handle_error("close shm fd");
    exit(0);
}

int main(int argc, char **argv) {
    // arguments
    if (argc > 1) n = atoi(argv[1]);
    if (argc > 2) m = atoi(argv[2]);
    if (argc > 3) t = atoi(argv[3]);

    // initialize the file
    init_file(FILENAME);

    
    /* TODO: initialize any semaphore needed in the implementation, and
     * create N children where the i-th child calls childProcess(i); 
     * initialize the shared memory (create it, set its size and map it in the 
     * memory), then the main process executes function mainProcess() once 
     * all the children have been created */
    //semafori
    sem_Partenza=create_named_semaphore("/sem_Partenza", 0660, 0);
    sem_Arrivo=create_named_semaphore("/sem_Arrivo", 0660, 0);
    sem_Scrittura=create_named_semaphore("/sem_Scrittura", 0660, 1);
    //shared_memory
    shm_unlink("/fine");
    fd=shm_open("/fine", O_CREAT | O_EXCL | O_RDWR, 0660);
    if(fd==-1) handle_error("shm_open");
    if(ftruncate(fd,sizeof(fine_t))!=0) handle_error("ftruncate");
    shr_var=mmap(0,sizeof(fine_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shr_var->TerminaThread=0;
    
    


    
    for(int i=0; i<n; i++){
        pid_t pid=fork();
        if(pid==-1){
            handle_error("fork");
        }
        if(pid==0){
            //processo figlio
            childProcess(i);
            break;
        }
    }

    mainProcess();

   
   
   
    
    exit(EXIT_SUCCESS);
}