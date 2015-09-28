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
//////////////////////////////////////////////////////////////////////////////////////////////////

struct ParamsWriter{
	pthread_mutex_t mutex;
    pthread_cond_t condvar;
	QueueArray queue;
};
struct Params{
  pthread_mutex_t mutex;
  pthread_cond_t condvar;
  int num;
  int sum;
  char *str;
  struct ParamsWriter* paramsWriter;
};
struct Params2{
	struct Params structArr[WORKING_THREADS_COUNT];

};

void* work(void *p);
void* readAndSend(void *p);
void* sumAndWrite(void *p);



int main(){
	pthread_t working[WORKING_THREADS_COUNT];	
	pthread_t  reader, writer;

	//очередь
	setlocale(LC_CTYPE, "");
    QueueArray F;
    queueArrayBaseType b;
    initQueueArray(&F);

    struct ParamsWriter paramsW;
    pthread_mutex_init(&paramsW.mutex, NULL);
   	pthread_cond_init(&paramsW.condvar, NULL); 
   	paramsW.queue = F;


	struct Params2 params2;
	for(int i = 0; i<WORKING_THREADS_COUNT; i++){
		struct Params params;  
		pthread_mutex_init(&params.mutex, NULL);
    	pthread_cond_init(&params.condvar, NULL); 
    	params.num = i;
    	params.paramsWriter = &paramsW;
    	params2.structArr[i] = params;
    	    	
	}
	for(int i = 0; i<WORKING_THREADS_COUNT; i++){
		pthread_create(&working[i], NULL, work, &params2.structArr[i]);
	}


 	

    pthread_create(&writer, NULL, sumAndWrite, &paramsW);
 	

 	pthread_create(&reader, NULL, readAndSend, &params2);
 	


 	pthread_join(reader, NULL);
 	pthread_join(writer, NULL);

	for(int i = 0; i<4; i++)
	{	
		 
		 pthread_mutex_destroy(&params2.structArr[i].mutex); 
 		 pthread_cond_destroy(&params2.structArr[i].condvar); 
		//printf("я поток %d->\n",params2.structArr[i].num);
	}
   return 0;
}


void* work(void* p){
	
	struct Params* params = (struct Params*) p;
	pthread_mutex_lock(&params->mutex);
	while(1){
		//printf("я поток %d->  \n",params->num);
		
		pthread_cond_wait(&params->condvar,&params->mutex);
		//printf("я работаю %s -> %d \n",params->str, params->num);
		int sum = 0;
		for(int i = 0; i<50; i++){
			if( isdigit(params->str[i]) != 0){
				sum+=atoi(&params->str[i]);
				printf("%c ",params->str[i]);
			}
			//printf("%s ",&params->str[i]);
		}
		//params->str = NULL;
		printf("я поток %d сумма %d -> %s\n",params->num,sum,params->str);
		
		pthread_mutex_lock(&params->paramsWriter->mutex);
		//printf("я поток %d залочил \n",params->num);
		putQueueArray(&params->paramsWriter->queue, sum);
		
		pthread_mutex_unlock(&params->paramsWriter->mutex);
		//printf("я поток %d отпустил \n",params->num);
		
		
		}
		pthread_mutex_unlock(&params->mutex);
}

void* sumAndWrite(void* p){
	//sleep(17);
	
	queueArrayBaseType b;
	int sum = 0;
	struct ParamsWriter* params = (struct ParamsWriter*) p;
	pthread_mutex_lock(&params->mutex);
	pthread_cond_wait(&params->condvar,&params->mutex);
	while(isEmptyQueueArray(&params->queue) == 0){
	getQueueArray(&params->queue, &b);
	printf("%d\n", b);
	sum+=b;
	}	
	pthread_mutex_unlock(&params->mutex);
	FILE *mf;
	mf = fopen("output","w");
	fprintf(mf,"%d",sum);
	fclose(mf);
	printf("SUUUUMM = %d \n",sum);
}

void* readAndSend(void* p){

	sleep(1);
	struct Params2* params = (struct Params2*) p;
    FILE *mf;
    char str[4][50];


    char *estr;
    mf = fopen("input","r");

    int count = -1;
    while (1)
    {    	

    	count++;
    	pthread_mutex_lock(&params->structArr[count].mutex);
        estr = fgets(str[count],sizeof(str[count]),mf);

       if (estr != NULL){       
      	
      	params->structArr[count].str = str[count];
      	sleep(1);
      	pthread_cond_signal(&params->structArr[count].condvar);
      	
      }
      else{
      	sleep(2);
      	pthread_cond_signal(&params->structArr[0].paramsWriter->condvar);
      	break;
      }
      pthread_mutex_unlock(&params->structArr[count].mutex);
      if (count == WORKING_THREADS_COUNT-1){
      		count = -1;

    	}
      	
    }

}