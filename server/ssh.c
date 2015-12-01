#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <signal.h>
#include <pthread.h>

#include "dict.h"
#include "dict.c"

#define WORKING_THREADS_COUNT 5

struct Params{
  pthread_mutex_t mutex;
  pthread_cond_t condvar;
  int currrent;
  int end;
  struct epoll_event* events;
  int epollfd;
  int socketfd;
  Dict login;
  Dict fLogin;
};

struct UserInfo{
  char *login;
  char *password;
  int connectionfd;
  int fLogin;
  int fPass;
};

struct addrinfo* getAvilableAddresses(struct addrinfo* hints, char* port){
  struct addrinfo* addresses; 
  int status = getaddrinfo(NULL, port, hints, &addresses);
  if(status != 0){
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return NULL;
  }
  return addresses;
}

int assignSocket(struct addrinfo* addresses){
  int yes = 1;
  int socketfd = 0;
  for(struct addrinfo *address = addresses; address != NULL; address = address->ai_next){
    socketfd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if(socketfd == -1){
      perror("impossible to create socket\n");
      continue;
    }
    if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
      perror("impossible to make soket reusable\n");
      continue;
    }
    if(bind(socketfd, address->ai_addr, address->ai_addrlen) == -1){
      close(socketfd);
      perror("impossible to bind socket\n");
      continue;
    }
    return socketfd;
  }
  perror("socket bind error\n");
  return -1;
}

int setNonBlock(int socketfd){
  int flags = fcntl(socketfd, F_GETFL, 0);
  if (fcntl(socketfd, F_SETFL, flags | O_NONBLOCK)){
    perror ("impossible to make socket nonblock");
    return -1;
  }
  return 0;
}

int addToEpoll(int epollfd, int socketfd){
  // добавляем сокет в epoll для отслеживания событий
  // (в нашем случае события EPOLLIN - что-то пришло, есть что читать)
  struct epoll_event event;
  event.data.fd = socketfd;
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &event) == -1){
    perror("failed to add descriptor to epoll");
    return -1;
  }
}

int acceptConnection(int epollfd, int socketfd,Dict login, Dict fLogin){
  struct sockaddr addr;
  socklen_t addrlen = sizeof addr;
  // Принимаем соединение. Так как ранее мы сделали сокет неблокирующим, то
  // остановки на accept не будет.
  // Если новых соединений нет или произошла ошибка то connectionfd будет == -1 
  int connectionfd = accept(socketfd, &addr, &addrlen); // NON BLOCK
  if (connectionfd == -1){
    perror ("failed to accept connection");
    return -1;
  }
  
  char host[NI_MAXHOST]; // баффер для имени хоста (или ip)
  char service[NI_MAXSERV]; // баффер для порта (номера сервиса)
  // Флаги NI_NUMERICHOST, NI_NUMERICSERV указывают на то, что нужно вернуть ip и номер порта, не пытаясь 
  // зарезолвить символьный адрес (имя хоста) и имя сервиса.
  if(!getnameinfo(&addr, addrlen, host, sizeof host, service, sizeof service, NI_NUMERICHOST | NI_NUMERICSERV)){
    //Выводим информацию о клиентеклиента
    printf("new connection: %s:%s\n", host, service);
  }
  // Делаем сокет принятого соединения неблокирующим
  if (setNonBlock(connectionfd) == -1){
    fprintf(stderr, "failed to set socket %d nonblock", connectionfd);
    return -1;
  }
  // Добавляем его в epoll, чтобы отслеживать события на нем
  if (addToEpoll(epollfd, connectionfd) == -1){
    fprintf(stderr, "failed to add socket %d to epoll", connectionfd);
    return -1;
  }
  dprintf(connectionfd,"Enter login: ");
  
  char conBuf[4];
  snprintf(conBuf, 4,"%d",connectionfd);
  DictInsert(login, conBuf, "");

  snprintf(conBuf, 4,"%d",connectionfd);
  DictInsert(fLogin, conBuf, "0");

  return 0;  
}
int checkLogin(const char *login, char * password, pthread_mutex_t mutex){
  printf("%s\n", login);
  printf("%s\n", password);
  pthread_mutex_lock(&mutex);
  FILE *mf;
  char *estr;
  char str[50];
  mf = fopen("registr","r");
  while(1)
  {

    estr = fgets(str,50,mf);
    if (estr == NULL)
      break;
    printf("%s\n", estr);
    printf("%s\n", login);
  
    if (!strcmp(str, login)){
      fclose(mf);
      return 1;
    }

    estr = fgets(str,50,mf);
    printf("%s\n", estr);
    printf("%s\n", password);

    if (!strcmp(str, password)){
      fclose(mf);
      return 1;
    }

  }
  fclose(mf);
  pthread_mutex_unlock(&mutex);
  return 0;
}

int handlerRequest(int epollfd, int connectionfd,Dict login, Dict fLogin, pthread_mutex_t mutex){
  // читаем сообщение клиента
  // все данные могут не влезть - в реальных условиях нужно сделать цикл для считывания
  // но здесь будем читать просто максимум 1024 символа (байта)
  char buffer[1024];
  char conBuf[4];
  ssize_t count = read(connectionfd, buffer, sizeof buffer);
  switch(count){
    case -1:
      if (errno != EAGAIN)
        perror ("failed to read data"); 
      break;
    case 0:
      printf("client closed the connection");
      break;
    default:
      snprintf(conBuf, 4,"%d",connectionfd);
      printf("->%s\n", DictSearch(fLogin, conBuf));
      if(!strcmp(DictSearch(login, conBuf),"")){

        //uinfo[connectionfd].login =(char*) malloc(count+1); 
        printf("->%d\n", count);
        if (count > 1){
          char *tmpLogin = (char*) malloc(count+1);
          strncpy(tmpLogin, buffer, count);
          tmpLogin[count] = '\0';
          DictInsert(login,conBuf,tmpLogin);

          printf("!!!%s!!!\n", DictSearch(login, conBuf));
          dprintf(connectionfd, "Enter password: ");
        }
        else
          dprintf(connectionfd,"Enter login: ");
      }
      else{
        if (!strcmp(DictSearch(fLogin, conBuf),"0")){
          /*uinfo[connectionfd].password =(char*) malloc(count+1); 
          strncpy(uinfo[connectionfd].password, buffer, count);
          uinfo[connectionfd].password[count] = '\0';

          if (!checkLogin(uinfo[connectionfd])){
            uinfo[connectionfd].fLogin = 0;
            uinfo[connectionfd].fPass = 0;
            dprintf(connectionfd, "Enter login!: ");
            break;
          }

          dprintf(connectionfd, "login: %s password: %s\n",uinfo[connectionfd].login,uinfo[connectionfd].password);
          */
          char *tmpPass=(char*) malloc(count+1); 
          strncpy(tmpPass, buffer, count);
          tmpPass[count] = '\0';
          int check = checkLogin(DictSearch(login, conBuf), tmpPass, mutex);
          if(check == 1){
            DictDelete(fLogin,conBuf);
            DictInsert(fLogin,conBuf,"1");
            dprintf(connectionfd, "!!Enter command: ");
          }
          else{
            DictDelete(login,conBuf);
            DictInsert(login,conBuf,"");
            dprintf(connectionfd,"Enter login: ");
          }
        }
        else{
          if(!strncmp(buffer, "logout",6))
            close(connectionfd);
          else{
          int c;
          //~ pp - pipe pointer
          FILE *pp;
          extern FILE *popen();
 
          if ( !(pp=popen(buffer, "r")) )  //"ls -l"
            return 1;
 
          while ( (c=fgetc(pp)) != EOF ) {
          putc(c, stdout); //stdout
          dprintf(connectionfd,"%c",c);
          //~ fflush нужна, чтобы не возникало задержки из-за буферизации
          fflush(pp);
          }   
          pclose(pp);

          printf("%d user message: %.*s", connectionfd, count, buffer); // Выводим сообщение
          //dprintf(connectionfd, "Hello, %.*s", count, buffer);
          dprintf(connectionfd, "Enter command: ");
        }
        }
      }
      /*char str[20] = "/bin/";
      strncat(str,buffer,10);
      printf("%s\n", str);*/


  }
  //printf("connection %d closed\n", connectionfd);
  //close(connectionfd);
}


int handleEvent(struct epoll_event* event, int epollfd, int socketfd, Dict login, Dict fLogin, pthread_mutex_t mutex){

  if ((event->events & EPOLLERR) || (event->events & EPOLLHUP)){
    printf("impossible to handle event\n");
    close(event->data.fd);
    return -1;
  }

  return socketfd == event->data.fd ?
         acceptConnection(epollfd, socketfd, login, fLogin): 
         // событие на серверном сокете, например новое соединение
         handlerRequest(epollfd, event->data.fd, login, fLogin, mutex);
         // событие на сокете соединения, например пришли данные  
}

volatile sig_atomic_t done = 0;

void handleSignal(int signum){
  done = 1;
}

void* workThread(void* p){
  struct Params* params = (struct Params*) p;
  printf("Wait");
  pthread_mutex_lock(&params->mutex);
  pthread_cond_wait(&params->condvar,&params->mutex);
  pthread_mutex_unlock(&params->mutex);
  printf("Hi");
  int cur = malloc(sizeof(int));
  while(!done)
  {
    
    int flag = 0;
    pthread_mutex_lock(&params->mutex);
    if(params->currrent < params->end){
      cur = params->currrent;
      params->currrent++;
      flag = 1;
    } 
    else{
      pthread_cond_wait(&params->condvar,&params->mutex);
    }
    pthread_mutex_unlock(&params->mutex);
    if(flag){
    printf("handling event %d of %d\n", params->currrent, params->end);
    printf("%d\n",cur );
    handleEvent(params->events+cur, params->epollfd, params->socketfd, params->login, params->fLogin,params->mutex);
    }//int eventsNumber = epoll_wait(epollfd, events, maxEventNum, -1);
   //handleEvent(params);
  }

}



int main (int argc, char *argv[]){
  // указываем параметры адреса
  //struct UserInfo uinfo[10];
  Dict login;

  login = DictCreate();

  Dict fLogin;

  fLogin = DictCreate();


  printf("starting server\n");
  struct addrinfo hints; 
  memset(&hints, 0, sizeof hints);
  hints.ai_flags =  AI_PASSIVE; // AI_PASSIVE - server socket, AI_CANONNAME
  hints.ai_family = AF_UNSPEC; // AF_INET, AF_INET6, AF_UNSPEC - любой
  hints.ai_socktype = SOCK_STREAM; // SOCK_DGRAM
  hints.ai_protocol = IPPROTO_TCP; // IPPROTO_TCP, IPPROTO_UDP, 0 - любой
  
  // получаем список доступных адресов, соответствующих параметрам выше
  char* port = "8080";
  struct addrinfo* addresses = getAvilableAddresses(&hints, port); 
  if(!addresses)
    return EXIT_FAILURE;
  
  // коннектим сокет к выбранному адресу
  int socketfd = assignSocket(addresses);
  if(!socketfd)
    return EXIT_FAILURE;

  // делаем дескриптор сокета неблокирующим (также можно сделать и при его создании, указав флаг SOCK_NONBLOCK)
  if (setNonBlock(socketfd) == -1){
    perror("non_block");
    return EXIT_FAILURE;
  }

  // переводим сокет в режим приема
  if (listen (socketfd, SOMAXCONN) == -1){
    perror ("listen");
    return EXIT_FAILURE;
  }
  printf("listening port %s\n", port);
  
  // создаем epoll
  int epollfd = epoll_create1(0);
  if (epollfd == -1){
    perror ("epoll_create");
    return EXIT_FAILURE;
  }

  // добавляем сокет в epoll для отслеживания событий
  if (addToEpoll(epollfd, socketfd) == -1)
    return EXIT_FAILURE;
  
  // регистрируем обработчик сигналов
  signal(SIGINT, handleSignal);
    
    
  // выделяем память для событий (в эту структуру epoll (когда мы спросим его)
  // запишет описания событий, произошедших с дескрипторами, за которыми он следит)
  int maxEventNum = 8;
  struct epoll_event events[maxEventNum * sizeof(struct epoll_event)];
  
  //потоки
  struct Params params;
  pthread_mutex_init(&params.mutex, NULL);
  pthread_cond_init(&params.condvar, NULL); 
  params.end = 0;
  params.currrent = 0;
  params.epollfd = epollfd;
  params.events = events;
  params.socketfd = socketfd;
  params.login = login;
  params.fLogin = fLogin;
  pthread_t working[WORKING_THREADS_COUNT]; 
  for(int i = 0; i<WORKING_THREADS_COUNT; i++){   
    pthread_create(&working[i], NULL, workThread, &params);
  } 

  // начинаем обрабатывать события

  int timeout = -1 ; // сколько миллисекунд нужно ждать получения события
  while(!done){


    //if (eventsNumber == 0) 
      //printf("no events\n");
    if(params.currrent >= params.end){
      printf("waiting new events\n");
      int eventsNumber = epoll_wait(epollfd, events, maxEventNum, timeout);
      params.currrent = 0;
      params.end = eventsNumber;
      printf("Send\n");
      pthread_cond_signal(&params.condvar); 
    }
    /*for (int i = 0; i<97; ++i){
      if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
      printf("--------%d\n",i);
    }*/

    /*for (int i = 0; i < eventsNumber; ++i) {
      //printf("--------%d\n", events[i].data.fd);
      printf("handling event %d of %d\n", i + 1, eventsNumber);
      handleEvent(events + i, epollfd, socketfd, login, fLogin);
      //printf("--------%d\n", events[i].data.fd);
    }*/
  }
  
  printf("server is going down\n");
  printf("closing connections\n");
  pthread_mutex_destroy(&params.mutex); 
  pthread_cond_destroy(&params.condvar); 
  close(socketfd);
  close(epollfd);
  printf("done\n");
  return EXIT_SUCCESS;
}