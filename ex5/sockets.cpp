#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

#define BUFFER 256
#define MAX_CLIENTS 5

#define HOSTNAME_ERROR "system error: invalid hostname"
#define CLIENT "client"
#define SERVER "server"
#define INPUT_ERROR "system error: wrong input"
#define WRITE_ERROR "system error: write command failed"
#define SOCKET_ERROR "system error: create socket failure"
#define BIND_ERROR "system error: bind failure"
#define LISTEN_ERROR "system error: listen failure"
#define READ_ERROR "system error: read failure"
#define SYSTEM_ERROR "system error: system failure"
#define SERVER_ERROR "system error: server not created"
#define MEM_ERROR "system error: memory alloc failure"

#define BASE 10
#define SUCCESS 0
#define IDENTICAL 0
#define SERVER_ARGS 3
#define CLIENT_ARGS 4
#define TYPE 1
#define PORT_ARG 2
#define COMMAND 3


int server(char* portnum) {
  char hostname[BUFFER];
  struct sockaddr_in sa;
  struct hostent *hp;
  int s;
  if (gethostname(hostname,BUFFER) < SUCCESS) {
    std::cerr << SERVER_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  if ((hp = gethostbyname(hostname)) == nullptr) {
    std::cerr << HOSTNAME_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  memset(&sa, 0, sizeof(struct sockaddr_in));
  sa.sin_family = hp->h_addrtype;
  memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
  auto port = (uint16_t) strtol(portnum, nullptr, BASE);
  sa.sin_port= htons(port);
  if ((s= socket(AF_INET, SOCK_STREAM, 0)) < SUCCESS){
    std::cerr << SOCKET_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  if (bind(s , (struct sockaddr*)&sa , sizeof(sa)) < SUCCESS) {
    close(s);
    std::cerr << BIND_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  if (listen(s, MAX_CLIENTS) < SUCCESS) {
    close(s);
    std::cerr << LISTEN_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  char* buf = (char *) malloc(sizeof(char)*BUFFER);
  if (buf == nullptr) {
    std::cerr << MEM_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  while (true){
    int t = accept(s,NULL, NULL);
    if (t  < SUCCESS) {
      std::cerr << BIND_ERROR << std::endl;
      free(buf);
      close(s);
      exit(EXIT_FAILURE);
    }
    if((read(t, buf, BUFFER)) < SUCCESS){
      std::cerr << READ_ERROR << std::endl;
      free(buf);
      close(s);
      exit(EXIT_FAILURE);
    }
    if(system(buf) < SUCCESS){
      std::cerr << SYSTEM_ERROR << std::endl;
      free(buf);
      close(s);
      exit(EXIT_FAILURE);
    }
  }
}


int client( char* portnum, char* command) {
  char hostname[BUFFER];
  struct sockaddr_in sa;
  struct hostent *hp;
  int s;
  if (gethostname(hostname,BUFFER) < SUCCESS) {
    std::cerr << SERVER_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  if ((hp = gethostbyname(hostname)) == nullptr) {
    std::cerr << HOSTNAME_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr , hp->h_addr , hp->h_length);
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons((u_short) strtol(portnum, nullptr, BASE));
  if ((s = socket(hp->h_addrtype,SOCK_STREAM,0)) < SUCCESS) {
    exit(EXIT_FAILURE);
  }
  if (connect(s, (struct sockaddr *)&sa , sizeof(sa)) < SUCCESS) {
    close(s);
    exit(EXIT_FAILURE);
  }
  if(((int)write(s, command, BUFFER)) < SUCCESS){
    std::cerr << WRITE_ERROR << std::endl;
    close(s);
    return(EXIT_FAILURE);
  }
  close(s);
  return(s);
}

int main (int argc, char* argv[]){
  if (argc == CLIENT_ARGS && strcmp(argv[TYPE], CLIENT) == IDENTICAL) {
    client(argv[PORT_ARG], argv[COMMAND]);
  }
  else if (argc == SERVER_ARGS && strcmp(argv[TYPE], SERVER) == IDENTICAL) {
    server(argv[PORT_ARG]);
  }
  else {
    std::cerr << INPUT_ERROR << std::endl;
  }
}

