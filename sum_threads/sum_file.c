#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>
#include "queue_array.h"


#define WORKING_THREADS_COUNT 5

///////////////////////////////////////////////////////////////////////////////////////////
/*Переменная ошибок*/
int errorQueueArray;
 
/*Инициализация очереди*/
void initQueueArray(QueueArray *F) {
    F->ukBegin = 0;
    F->ukEnd = 0;
    F->len = 0;
    errorQueueArray = okQueueArray;
}
 
/*Включение в очередь*/
void putQueueArray(QueueArray *F, queueArrayBaseType E) {
 
    /*Если очередь переполнена*/
    if (isFullQueueArray(F)) {
        return;
    }
    /*Иначе*/
    F->buf[F->ukEnd] = E;                                 // Включение элемента
    F->ukEnd = (F->ukEnd + 1) % SIZE_QUEUE_ARRAY;         // Сдвиг указателя
    F->len++;                                                // Увеличение количества элементов очереди
 
}
 
/*Исключение из очереди*/
void getQueueArray(QueueArray *F, queueArrayBaseType *E) {
 
    /*Если очередь пуста*/
    if (isEmptyQueueArray(F)) {
        return;
    }
    /*Иначе*/
    *E = F->buf[F->ukBegin];                              // Запись элемента в переменную
    F->ukBegin = (F->ukBegin + 1) % SIZE_QUEUE_ARRAY;     // Сдвиг указателя
    F->len--;                                                // Уменьшение длины
}
 
/*Предикат: полна ли очередь*/
int isFullQueueArray(QueueArray *F) {
    if (F->len == SIZE_QUEUE_ARRAY) {
        errorQueueArray = fullQueueArray;
        return 1;
    }
    return 0;
}
 
/*Предикат: пуста ли очередь*/
int isEmptyQueueArray(QueueArray *F) {
    if (F->len == 0) {
        errorQueueArray = emptyQueueArray;
        return 1;
    }
    return 0;
}
int countQueueArray(QueueArray *F){
  return F->len;
}
//////////////////////////////////////////////////////////////////////////////////////////////////

struct ParamsWrRead{
  pthread_mutex_t mutex;
  pthread_cond_t condvar;
  pthread_mutex_t mutexRet;
  pthread_cond_t condvarRet;
	QueueArray queue;
  int count;
};
struct ParamsWorkers{
  struct ParamsWrRead* paramsWriter;
  struct ParamsWrRead* paramsReader;
};


void* work(void *p);
void* readAndSend(void *p);
void* sumAndWrite(void *p);



int main(){
	pthread_t working[WORKING_THREADS_COUNT];	
	pthread_t  reader, writer;

	//очередь
	setlocale(LC_CTYPE, "");
  QueueArray queueWriter;
  queueArrayBaseType b;
  initQueueArray(&queueWriter);

  QueueArray queueReader;
  queueArrayBaseType b1;
  initQueueArray(&queueReader);

  struct ParamsWrRead paramsReader;
  pthread_mutex_init(&paramsReader.mutex, NULL);
  pthread_cond_init(&paramsReader.condvar, NULL); 
  pthread_mutex_init(&paramsReader.mutexRet, NULL);
  pthread_cond_init(&paramsReader.condvarRet, NULL);
  paramsReader.queue = queueReader;

	struct ParamsWrRead paramsWriter;
  pthread_mutex_init(&paramsWriter.mutex, NULL);
  pthread_cond_init(&paramsWriter.condvar, NULL); 
  paramsWriter.queue = queueWriter;

  struct ParamsWorkers params;  
  params.paramsWriter = &paramsWriter;
  params.paramsReader = &paramsReader;
	

  pthread_create(&reader, NULL, readAndSend, &paramsReader);

  pthread_create(&writer, NULL, sumAndWrite, &paramsWriter);
 	
  for(int i = 0; i<WORKING_THREADS_COUNT; i++){   
    pthread_create(&working[i], NULL, work, &params);
  } 
 	
 	pthread_join(reader, NULL);
 	pthread_join(writer, NULL);

  pthread_mutex_destroy(&paramsWriter.mutex); 
  pthread_cond_destroy(&paramsWriter.condvar); 
  pthread_mutex_destroy(&paramsReader.mutex); 
  pthread_cond_destroy(&paramsReader.condvar); 

   return 0;
}


void* work(void* p){
	
struct ParamsWorkers* params = (struct ParamsWorkers*) p;
/*pthread_mutex_lock(&params->paramsReader->mutex);
pthread_cond_wait(&params->paramsReader->condvar,&params->paramsReader->mutex);
pthread_mutex_unlock(&params->paramsReader->mutex);*/
int sum[50];
int count = -1;
while(1){
  pthread_mutex_lock(&params->paramsReader->mutex);
  //printf("Ок жду\n");
  pthread_cond_wait(&params->paramsReader->condvar,&params->paramsReader->mutex);
  //printf("Дождался\n");
  if(isEmptyQueueArray(&params->paramsReader->queue) == 0){
    count++;
    void* typeQR;
    //printf("привет \n");
		//pthread_mutex_lock(&params->paramsReader->mutex);

    //pthread_cond_wait(&params->paramsReader->condvar,&params->paramsReader->mutex);

    getQueueArray(&params->paramsReader->queue, &typeQR);

    char* str = (char *) typeQR;
    //printf("я работаю %s\n",str);
    
		sum[count] = 0;
		for(int i = 0; i<50; i++){
			if( isdigit(str[i]) != 0){
				sum[count]+=atoi(&str[i]);
				//printf("%c ",params->str[i]);
			}
			//printf("%s ",&params->str[i]);
		}
    printf("сумма %d\n",sum[count]);
    
		pthread_mutex_lock(&params->paramsWriter->mutex);

		putQueueArray(&params->paramsWriter->queue, &sum[count]);

    //if(countQueueArray(&params->paramsWriter->queue) >2){
      pthread_cond_signal(&params->paramsWriter->condvar); 
      
    //}
    pthread_mutex_unlock(&params->paramsWriter->mutex);


		

		}
    pthread_mutex_unlock(&params->paramsReader->mutex);
    pthread_mutex_lock(&params->paramsReader->mutexRet);
    pthread_cond_signal(&params->paramsReader->condvarRet);
    pthread_mutex_unlock(&params->paramsReader->mutexRet);

  }
		
}

void* sumAndWrite(void* p){

	void* b;
	int sum = 0;
	struct ParamsWrRead* params = (struct ParamsWrRead*) p;
  while(1){
    pthread_mutex_lock(&params->mutex);
    pthread_cond_wait(&params->condvar,&params->mutex);

    //printf("%d\n", isEmptyQueueArray(&params->queue));
    while(isEmptyQueueArray(&params->queue) == 0){
    getQueueArray(&params->queue, &b);
    int* k = (int *) b;
    sum += *k;
    pthread_mutex_unlock(&params->mutex);

    }	
    printf("SUUUUMM = %d \n",sum);
  }

	FILE *mf;
	mf = fopen("output","w");
	fprintf(mf,"%d",sum);
	fclose(mf);
	printf("SUUUUMM = %d \n",sum);
}

void* readAndSend(void* p){
  sleep(1);
	struct ParamsWrRead* params = (struct ParamsWrRead*) p;
  FILE *mf;
  char *estr;
  char str[50][50];
  //char* str;
  mf = fopen("input","r");
  
  int count = -1;
  pthread_mutex_lock(&params->mutex);
  while (1)
  {
    
    count++;
    estr = fgets(str[count],50,mf);
    if (estr != NULL){       
      putQueueArray(&params->queue, estr);
      //printf("Закинул %d\n", isEmptyQueueArray(&params->queue));
    	//pthread_cond_signal(&params->structArr[count].condvar);

    }
    else{
      	//pthread_cond_signal(&params->structArr[0].paramsWriter->condvar);
        params->count = count;
      	break;
      }
    
    //if((count+1) % 5 == 0){v
      pthread_cond_signal(&params->condvar);
      pthread_mutex_unlock(&params->mutex);
      //for(int i=0;i<WORKING_THREADS_COUNT;i++){
      
            
      //}
      //printf("Послал\n");
      if(count>4){
        pthread_mutex_lock(&params->mutexRet);

        pthread_cond_wait(&params->condvarRet,&params->mutexRet);
        pthread_mutex_unlock(&params->mutexRet);
      }
      pthread_mutex_lock(&params->mutex);
      //printf("Дальше\n");
    //}

  }

  /*while (isEmptyQueueArray(&params->queue) == 0){
    queueArrayBaseType typeQR;
    getQueueArray(&params->queue, &typeQR);
  
    char* str = (char *) typeQR;
    printf("%s\n", str);
  }  */

}