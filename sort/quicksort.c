#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void exit (int code);

void swap (int *x, int *y){
     int z = *x;
     *x = *y;
     *y = z;
}


//int n, a[n]; //n - количество элементов
void qs(int* s_arr, int first, int last)
{
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

    if (i < last)
        qs(s_arr, i, last);
    if (first < j)
        qs(s_arr, first, j);
}

int main()
{
  int fd[2];
  pipe(fd);
  int memsize = 15;
  int sizeOfShm = sizeof(int)*memsize;
  int shmid = shmget(IPC_PRIVATE, sizeOfShm, SHM_W | SHM_R | IPC_CREAT);
  if(shmid<0){
    perror("Impossible to access shared memory");
    exit(EXIT_FAILURE);
  }
  int* sharedMem = shmat(shmid, NULL, 0); //SHM_RDONLY

  if(!fork()){
       
    close(fd[0]);
    int first = 0;
    int n = 15;
    //int a[] = {14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};
    //sharedMem = a;

    for(int i = 0; i < memsize; ++i)
      sharedMem[memsize-i-1] = i;

    shmdt(&shmid);      
    write(fd[1], &first, sizeof(int));

    write(fd[1], &n, sizeof(int));


    }else{
      close(fd[1]);
      int first;
      int last;
      int nbytes = read(fd[0], &first, sizeof(first));
      printf("Received string: %d\n", first);
      nbytes = read(fd[0], &last, sizeof(last));
      printf("Received string: %d\n", last);
      for(int i = 0; i < memsize; ++i)
        printf("%d ", sharedMem[i]);
      printf("\n");
      shmdt(&shmid);
      shmctl(shmid,IPC_RMID,NULL); //IPC_STAT IPC_SET
      qs(sharedMem,first,last-1);

      for(int i = 0; i < memsize; ++i)
        printf("%d ", sharedMem[i]);
      printf("\n");
    }
        //array = a;
        /*qs(a,0,9);


        for(int i = 0; i<n; i++){
        printf("%d \n",a[i]);
        }*/





        return 0;
} 