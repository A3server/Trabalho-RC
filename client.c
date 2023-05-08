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

// q: this program is using tcp or udp?
// a:
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

    while (1)
    {
      char username[BUFLEN];
      char password[BUFLEN];
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
      char *type = strtok(NULL, "-");

      if (strcmp(type, "jornalista") != 0 && strcmp(type, "cliente") != 0)
      {
        printf("Authentication failed, wrong user type.\nPlease try again:\n");
        continue;
      }

      printf("Welcome %s - %s\n", type, username);
      if (strcmp(type, "cliente") == 0)
      {
        printf("Available Commands:\nLIST_TOPICS\nSUBSCRIBE_TOPIC <topic id>\n");

        


      }
      else if (strcmp(type, "jornalista") == 0)
      {

        // TODO: check if jornalista, and then write new topics with the command "CREATE_TOPIC"
        // TODO: n pode haver topicos repetidos boi, se houver dá erro fdp
        // TODO: fazes merda, fodo-te
        // TODO: se o topico ja existir, da erro

        // TODO: no server ja dei uns tabs do copilot, ve se aquela merda faz sentido

        // faz o comando CREATE_TOPIC agora, é só escrever e criar o multicast server, está por ai o codigo
      }
    }
  }

  close(fd);
  exit(0);
}

void join_multicast_sv(char *multicastAddr)
{

  int fd;
  struct ip_mreq mreq;

  // Join the multicast group
  mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
  {
    perror("setsockopt");
    exit(1);
  }
}

void erro(char *msg)
{
  printf("Erro: %s\n", msg);
  exit(-1);
}
