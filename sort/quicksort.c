#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
//#define SIZEOFMEM = 30;

void exit (int code);

void swap (int *x, int *y){
     int z = *x;
     *x = *y;
     *y = z;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
void hdl(int sig)
{
  printf("sigterm\n"); 
}
void qs(int* s_arr, int first, int last, int semID)
{


      struct sembuf operations[1]; 
      operations[0].sem_num = 0;   
      operations[0].sem_op = -1;  


      semop(semID, operations, sizeof(operations) / sizeof(struct sembuf)); 
     
      int i = first, j = last, x = s_arr[(first + last) / 2];

      do {
        while (s_arr[i] < x) i++;
        while (s_arr[j] > x) j--;

        if(i <= j) {
            if (s_arr[i] > s_arr[j]) swap(&s_arr[i], &s_arr[j]);
            i++;
            j--;
        }
      } while (i <= j);

      operations[0].sem_op = 1;
      semop(semID, operations, sizeof(operations) / sizeof(struct sembuf)); 

    

    int fd[2];
    pipe(fd);
    int chpid = fork();
    if(!chpid){
      close(fd[0]);
      if (i < last){
        qs(s_arr, i, last,semID);
      }
      
      //wait(NULL);
    }else{


      if (first < j){
        qs(s_arr, first, j,semID);
      }
      waitpid(chpid, NULL, 0);
    }
}

int main()
{
  int fd[2];
  pipe(fd);
  int memsize = 30;
  int sizeOfShm = sizeof(int)*memsize;
  int shmid = shmget(IPC_PRIVATE, sizeOfShm, SHM_W | SHM_R | IPC_CREAT);
  if(shmid<0){
    perror("Impossible to access shared memory");
    exit(EXIT_FAILURE);
  }

  int* sharedMem = shmat(shmid, NULL, 0); //SHM_RDONLY

  int semID = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0600);
  if(semID < 0) {
    perror("semget fail:");
    exit(EXIT_FAILURE);
  }

  if(semctl(semID, 0, SETVAL, 1) < 0){
    perror("setval fail:");
    exit(EXIT_FAILURE);
  }
 
  printf("First - %d\n", getpid());
  int chpid = fork();

  if(!chpid){
      close(fd[1]);
      int first;
      int last;
      int nbytes = read(fd[0], &first, sizeof(first));
      //printf("Received string: %d\n", first);
      nbytes = read(fd[0], &last, sizeof(last));
      //printf("Received string: %d\n", last);
      printf("\n");
      for(int i = 0; i < memsize; ++i)
        printf("%d ", sharedMem[i]);
      printf("\n");
      shmdt(&shmid);
      shmctl(shmid,IPC_RMID,NULL); //IPC_STAT IPC_SET
      
      qs(sharedMem,first,last-1,semID);
    }else{
             
    close(fd[0]);
    int first = 0;
    //int n = 30;


    for(int i = 0; i < memsize; ++i)
      sharedMem[memsize-i-1] = rand() % 100;

    shmdt(&shmid);      
    write(fd[1], &first, sizeof(int));

    write(fd[1], &memsize, sizeof(int));
    /*sleep(1);
    for(int i = 0; i < memsize; ++i)
        printf("%d ", sharedMem[i]);
      printf("\n");*/ 
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = hdl;
    sigset_t   set; 
    sigemptyset(&set);                                                             
    sigaddset(&set, SIGTERM); 
    act.sa_mask = set;
    sigaction(SIGTERM, &act, 0);
     
      //пробная передача
      /*pipe(fd);

      if(!fork()){
      close(fd[0]);
      
      int cfirst = 67;
      int clast = 66;

      write(fd[1], &cfirst, sizeof(int));

      write(fd[1], &clast, sizeof(int));

      }else{
        int n1,n2;
        int cnbytes = read(fd[0], &n1, sizeof(n1));
        printf("Received string: %d\n", n1);
        cnbytes = read(fd[0], &n2, sizeof(n2));
        printf("Received string: %d\n", n2);
      }*/


    }
    //pid_t pid = getpid();
    if(chpid){
    waitpid(chpid, NULL, 0); 
    //wait(NULL);
    //sleep(1);
    printf("END - %d\n", getpid());
    printf("\n");
    printf("\n");
    for(int i = 0; i < memsize; ++i)
        printf("%d ", sharedMem[i]);
      printf("\n");
    if(semctl(semID, 0, IPC_RMID) < 0) {
      perror("remove fail:");
      exit(EXIT_FAILURE);
      }
    }
    printf("%d\n", getpid());
        return 0;
} 