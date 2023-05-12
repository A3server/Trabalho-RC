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
          // copy token to another string to don't lose it in the next strtok

          token = strtok(NULL, " ");

          char *topicId_cp = malloc(strlen(token) + 1);
          strcpy(topicId_cp, token);

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

          char *ip = strtok(buffer, " ");
          char *port = strtok(NULL, " ");

          printf("Trying to connect to server multicast @ %s:%s\n", ip, port);

         // connect to the multicast server
          int fd_udp;

          if ((fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            erro("socket");

          struct sockaddr_in addr;
          memset((void *)&addr, 0, sizeof(addr));
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = inet_addr(ip);
          addr.sin_port = htons((short)atoi(port));

          struct ip_mreq mreq;
          mreq.imr_multiaddr.s_addr = inet_addr(ip);
          mreq.imr_interface.s_addr = htonl(INADDR_ANY);
          if (setsockopt(fd_udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            erro("setsockopt");

          if (connect(fd_udp, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            erro("connect");

          printf("CONNECTED TO MULTICAST SERVER@%s started with info %s:%d - MCGROUP:%s:%s\n",
                 topicId_cp, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), ip, port);

          while (1)
          {
            char buffer[BUFLEN];
            struct sockaddr_in client_addr;
            memset(buffer, 0, sizeof(buffer));
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t len = sizeof(client_addr);

            ssize_t num_bytes = recvfrom(fd, buffer, sizeof(buffer), 0,
                                         (struct sockaddr *)&client_addr, &len);
            if (num_bytes < 0)
            {
              perror("recvfrom failed");
              exit(EXIT_FAILURE);
            }

            buffer[num_bytes] = '\0';

            printf("[CLIENT-LISTENING-MULTICAST:%s] Received message from %s:%d: %s\n", token,
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
            // Process the received message here
          }
          printf("[CLIENT-LISTENING-MULTICAST:%s] Closed connection\n", token);
          // Cleanup and close the socket
          close(fd);
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

      printf("Available Commands:\n  - CREATE_TOPIC <topic id> <titulo>\n  - SEND_NEWS <topic id> <noticia>\nserver@%s$ ", type);

      while (1)
      {
        // read from input and check if we wrote any commands above
        bzero(buffer, BUFLEN);
        fgets(buffer, BUFLEN, stdin);
        buffer[strcspn(buffer, "\r\n")] = '\0';

        char *command = strtok(buffer, " ");
        char *topicId = strtok(NULL, " ");

        if (strcmp(command, "CREATE_TOPIC") == 0)
        {
          char *titulo = strtok(NULL, " ");
          if (titulo == NULL)
          {
            printf("Error: Invalid command\nserver@%s$ ", type);
            continue;
          }

          // CREATE A STRING composed by "CREATE_TOPIC <topic id> <titulo>"
          char b[BUFLEN];
          bzero(b, BUFLEN);
          strcat(b, "CREATE_TOPIC ");
          strcat(b, topicId);
          strcat(b, " ");
          strcat(b, titulo);

          strcpy(buffer, b);

          // SENT TO THE SERVER THE COMMAND "CREATE_TOPIC <topic id> <titulo>"
          n = write(fd, buffer, strlen(buffer));
          if (n < 0)
            erro("ERROR writing to socket");

          // wait for server to send data
          bzero(buffer, BUFLEN);
          n = read(fd, buffer, BUFLEN);
          if (n < 0)
            erro("ERROR reading from socket");

          char *token = strtok(buffer, " ");
          if (strcmp(token, "[ERROR]") == 0)
          {
            printf("%s\nserver@%s$ ", buffer, type);
            continue;
          }
          char *port = strtok(NULL, " ");

          printf("Added topic!\nListen on:%s:%s\n", token, port);
        }
        else if (strcmp(command, "SEND_NEWS") == 0)
        {
          char *titulo = strtok(NULL, " ");
          if (titulo == NULL)
          {
            printf("Error: Invalid command\nserver@%s$ ", type);
            continue;
          }

          // CREATE A STRING composed by "SEND_NEWS <topic id> <titulo>"
          // save topic id and titulo in another string to don't lose it in the next strtok

          char *topicId_cpy = malloc(strlen(topicId) + 1);
          strcpy(topicId_cpy, topicId);

          char *titulo_cpy = malloc(strlen(titulo) + 1);
          strcpy(titulo_cpy, titulo);

          char b[BUFLEN];
          bzero(b, BUFLEN);
          strcat(b, "SEND_NEWS ");
          strcat(b, topicId);
          strcat(b, " ");
          strcat(b, titulo);

          strcpy(buffer, b);

          // SENT TO THE SERVER THE COMMAND "SEND_NEWS <topic id> <titulo>"
          printf("sENT: %s\n", buffer);
          n = write(fd, buffer, strlen(buffer));
          if (n < 0)
            erro("ERROR writing to socket");

          // wait for server to send data
          bzero(buffer, BUFLEN);
          n = read(fd, buffer, BUFLEN);
          if (n < 0)
            erro("ERROR reading from socket");

          printf("Recieved: %s\n", buffer);

          char *ip = strtok(buffer, " ");
          char *port = strtok(NULL, " ");
          if (strcmp(ip, "[ERROR]") == 0)
          {
            printf("%s\nserver@%s$ ", buffer, type);
            continue;
          }
          printf("Sent news@%s:%s on MC server (%s:%s) to send data.\n", topicId_cpy, titulo_cpy, ip, port);

          // connect to the multicast server
          struct sockaddr_in addr;
          int fd_udp;

          if ((fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            erro("socket");

          memset((void *)&addr, 0, sizeof(addr));
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = inet_addr(ip);
          addr.sin_port = htons((short)atoi(port));

          struct ip_mreq mreq;
          mreq.imr_multiaddr.s_addr = inet_addr(ip);
          mreq.imr_interface.s_addr = htonl(INADDR_ANY);
          if (setsockopt(fd_udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            erro("setsockopt");

          if (connect(fd_udp, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            erro("connect");

          printf("Connected to multicast server.\n");

          // send the news to the multicast server
          char news[BUFLEN];
          bzero(news, BUFLEN);
          strcat(news, "NEWS ");
          strcat(news, topicId_cpy);
          strcat(news, " ");
          strcat(news, titulo_cpy);

          ssize_t b_sent = send(fd_udp, news, strlen(news), 0);
          if (b_sent < 0)
            erro("send");

          printf("Sent news to multicast server: %s\n", news);
          close(fd_udp);
        }
        else
        {
          printf("Error: Invalid command\n");
        }
        printf("server@%s$ ", type);
      }
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
