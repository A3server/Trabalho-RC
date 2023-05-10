#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFLEN 512 // Tamanho do buffer

void erro(char *msg);

int main(int argc, char *argv[])
{
  char endServer[100];
  int fd;
  struct sockaddr_in addr;
  struct hostent *hostPtr;

  if (argc != 3)
  {
    printf("./news_client <host> <port>\n");
    exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Não consegui obter endereço");

  bzero((void *)&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short)atoi(argv[2]));

  // TCP connection
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    erro("socket");

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("connect");
  }

  // wait for server to send data
  char buffer[BUFLEN];
  int n;
  bzero(buffer, BUFLEN);
  n = read(fd, buffer, BUFLEN);
  if (n < 0)
    erro("ERROR reading from socket");
  printf("%s\n", buffer);

  // check if server asked for authentication
  if (strcmp(buffer, "AUTH") == 0)
  {
    char username[BUFLEN];
    char password[BUFLEN];
    char *type;
    while (1)
    {

      // printf("Authentication required\n");
      printf("Username: ");
      bzero(username, BUFLEN);
      fgets(username, BUFLEN, stdin);
      username[strcspn(username, "\r\n")] = '\0';

      printf("Password: ");
      bzero(password, BUFLEN);
      fgets(password, BUFLEN, stdin);
      password[strcspn(password, "\r\n")] = '\0';

      // send to the server the username and password, create a string like this "username;password"
      char auth[BUFLEN];
      bzero(auth, BUFLEN);
      strcat(auth, username);
      strcat(auth, ";");
      strcat(auth, password);
      n = write(fd, auth, strlen(auth));
      if (n < 0)
        erro("ERROR writing to socket");

      // wait for server to send data
      bzero(buffer, BUFLEN);
      n = read(fd, buffer, BUFLEN);
      if (n < 0)
        erro("ERROR reading from socket");
      printf("Recieved: %s\n", buffer);

      // check if authentication was successful if recieved NOK or its not type of OK-{type}
      if (strcmp(buffer, "NOK") == 0 || strncmp(buffer, "OK", 2) != 0)
      {
        printf("Authentication failed, Please try again:\n");
        continue;
      }

      char *token = strtok(buffer, "-");
      char *aux_type = strtok(NULL, "-");
      // copy the type of user
      type = malloc(strlen(aux_type) + 1);
      strcpy(type, aux_type);

      if (strcmp(type, "jornalista") == 0 || strcmp(type, "cliente") == 0 || strcmp(type, "administrador") == 0)
      {
        break;
      }
      else
      {
        printf("Authentication failed, Please try again:\n");
      }
    }

    printf("Welcome %s - %s\n", type, username);
    if (strcmp(type, "cliente") == 0 || strcmp(type, "administrador") == 0)
    {
      printf("Available Commands:\n  - LIST_TOPICS\n  - SUBSCRIBE_TOPIC <topic id>\nserver@%s$ ", type);

      while (1)
      {
        // read from input and check if we wrote any commands above
        bzero(buffer, BUFLEN);
        fgets(buffer, BUFLEN, stdin);
        buffer[strcspn(buffer, "\r\n")] = '\0';

        // printf("SUBSCRIBE_TOPIC: %d\n", strncmp(buffer, "SUBSCRIBE_TOPIC", 15));

        // check if we wrote any commands above
        if (strcmp(buffer, "LIST_TOPICS") == 0)
        {
          // send to the server the command "LIST_TOPICS"
          n = write(fd, buffer, strlen(buffer));
          if (n < 0)
            erro("ERROR writing to socket");

          // wait for server to send data
          bzero(buffer, BUFLEN);
          n = read(fd, buffer, BUFLEN);
          if (n < 0)
            erro("ERROR reading from socket");
          printf("%s\n", buffer);
        }
        else if (strncmp(buffer, "SUBSCRIBE_TOPIC", 15) == 0)
        {
          // check if we wrote SUBSCRIBE_TOPIC with a topic id
          char *token = strtok(buffer, " ");
          token = strtok(NULL, " ");
          // printf("token: %s\n", token);
          if (token == NULL)
          {
            printf("Error: Invalid command\nserver@%s$ ", type);
            continue;
          }

          // CREATE A STRING composed by "SUBSCRIBE_TOPIC <topic id>"
          char b[BUFLEN];
          bzero(b, BUFLEN);
          strcat(b, "SUBSCRIBE_TOPIC ");
          strcat(b, token);
          strcpy(buffer, b);

          // SENT TO THE SERVER THE COMMAND "SUBSCRIBE_TOPIC <topic id>"
          n = write(fd, buffer, strlen(buffer));
          if (n < 0)
            erro("ERROR writing to socket");

          // wait for server to send data
          bzero(buffer, BUFLEN);
          n = read(fd, buffer, BUFLEN);
          if (n < 0)
            erro("ERROR reading from socket");


          // check if message starts with [ERROR]
          if (strncmp(buffer, "[ERROR]", 7) == 0)
          {
            printf("%s\nserver@%s$ ", buffer, type);
            continue;
          }
          printf("Recieved: %s\n", buffer);
    
          // connect to the socket msg is like this: "PORT;IP"
          char *port = strtok(buffer, " ");
          char *ip = strtok(NULL, " ");

          printf("port: %s\n", port);
          printf("ip: %s\n", ip);


          // create a UDP connection to the server
          struct sockaddr_in addr;
          struct hostent *hostPtr;

          if ((hostPtr = gethostbyname(ip)) == 0)
            erro("Não consegui obter endereço");

          bzero((void *)&addr, sizeof(addr));
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
          addr.sin_port = htons((short)atoi(port));

          // UDP connection
          int fd_udp;
          if ((fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            erro("socket");

          if (connect(fd_udp, (struct sockaddr *)&addr, sizeof(addr)) < 0)
          {
            perror("connect");
          }

          printf("Listening to multicast server...\n");
        }
        else
        {
          printf("Error: Invalid command\n");
        }
        printf("server@%s$ ", type);
      }
    }
    else if (strcmp(type, "jornalista") == 0)
    {

      // TODO: check if jornalista, and then write new topics with the command "CREATE_TOPIC"
      // TODO: n pode haver topicos repetidos boi, se houver dá erro fdp
      // TODO: fazes merda, fodo-te
      // TODO: se o topico ja existir, da erro

      // TODO: no server ja dei uns tabs do copilot, ve se aquela merda faz sentido

      // faz o comando CREATE_TOPIC agora, é só escrever e criar o multicast server, está por ai o codigo

      printf("Available Commands:\n  - CREATE_TOPIC <topic id> <topic title>\nserver@%s$ ", type);
    }
  }

  close(fd);
  exit(0);
}

void erro(char *msg)
{
  printf("Erro: %s\n", msg);
  exit(-1);
}
