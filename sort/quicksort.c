#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>


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
      int *array; /* Указатель на разделяемую память */
      int shmid; /* IPC дескриптор для области разделяемой памяти */
      int new = 1; /* Флаг необходимости инициализации элементов массива */
      char pathname[] = "prog.c"; /* Имя файла, использующееся для генерации ключа.
       Файл с таким именем должен существовать в текущей директории */

      key_t key; /* IPC ключ */
      if((key = ftok(pathname,0)) < 0)
      {
           printf("Can't generate keyn");
           exit(-1);
      }

      /* Cоздаем разделяемую память для сгенерированного ключа, т.е. если для этого ключа она уже существует
      системный вызов вернет отрицательное значение. Размер памяти определяем как размер массива из 3-х целых переменных,
      права доступа 0666 - чтение и запись разрешены для всех */

      if((shmid = shmget(key, 3*sizeof(int), 0666|IPC_CREAT|IPC_EXCL)) < 0)
      {
            /* В случае возникновения ошибки пытаемся определить: возникла ли она из-за того, что сегмент
            разделяемой памяти уже существует или по другой причине */
            if(errno != EEXIST)
            {
              /* Если по другой причине - прекращаем работу */
              printf("Can't create shared memoryn");
              exit(-1);
            }
            else
            {
              /* Если из-за того, что разделяемая память уже существует пытаемся получить ее IPC дескриптор и,
              в случае удачи, сбрасываем флаг необходимости инициализации элементов массива */

                if((shmid = shmget(key, 3*sizeof(int), 0)) < 0)
                {
                    printf("Can't find shared memoryn");
                    exit(-1);
                }
                new = 0;
            }
       }

        /* Пытаемся отобразить разделяемую память в адресное пространство текущего процесса. Обратите внимание на то,
         что для правильного сравнения мы явно преобразовываем значение -1 к указателю на целое.*/

        if((array = (int *)shmat(shmid, NULL, 0)) == (int *)(-1))
        {
            printf("Can't attach shared memoryn");
            exit(-1);
        }

        /*  Работа с участком памяти */
        int n = 10;
        int a[] = {9,8,7,6,5,4,3,2,1,0};
        //for(int i = 0; i<10; i++){
          printf("%d",a[0]);
        //}
        //array = a;
        qs(a,0,9);
        //for(int i = 0; i<10; i++){
          printf("%d",a[0]);
        //}
        if(shmdt(array) < 0)
        {
            printf("Can't detach shared memoryn");
            exit(-1);
        }
        return 0;
} 