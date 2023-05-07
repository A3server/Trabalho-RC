#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>

#define BUFLEN 1024

void erro(char *msg)
{
    printf("Erro: %s\n", msg);
    exit(-1);
}

int main(int argc, char *argv[])
{
    char endServer[100];
    int fd;
    struct sockaddr_in addr;
    struct hostent *hostPtr;

    if (argc != 3)
    {
        printf("./admin <host> <port>\n"); // PORTO_CONFIG
        exit(-1);
    }

    // create a UDP connection to the server

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
        erro("Não consegui obter endereço");

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short)atoi(argv[2]));

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        erro("socket");

    // send a message to the server saying that we wanna authenticate
    char buffer[BUFLEN];
    int n;

    strcpy(buffer, "AUTH"); // Message indicating authentication request

    if (sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        erro("sendto");

    // wait for server to send data
    bzero(buffer, BUFLEN);
    socklen_t addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, BUFLEN, 0, (struct sockaddr *)&addr, &addrlen);
    if (n < 0)
        erro("ERROR reading from socket");
    printf("Recieved: %s\n", buffer);


    // check if server asked for authentication
    if (strcmp(buffer, "AUTH") == 0)
    {
        while (1)
        {
            char username[BUFLEN];
            char password[BUFLEN];

            printf("Username: ");
            bzero(username, BUFLEN);
            fgets(username, BUFLEN, stdin);
            username[strcspn(username, "\r\n")] = '\0';

            printf("\nPassword: ");
            bzero(password, BUFLEN);
            fgets(password, BUFLEN, stdin);
            password[strcspn(password, "\r\n")] = '\0';

            // send to the server the username and password, create a string like this "username;password"
            char auth[BUFLEN];
            bzero(auth, BUFLEN);
            strcat(auth, username);
            strcat(auth, ";");
            strcat(auth, password);

            if (sendto(fd, auth, strlen(auth), 0, (struct sockaddr *)&addr, sizeof(addr)) == -1)
                erro("sendto");

            // wait for server to send data
            bzero(buffer, BUFLEN);
            n = recvfrom(fd, buffer, BUFLEN, 0, (struct sockaddr *)&addr, &addrlen);
            if (n < 0)
                erro("ERROR reading from socket");
            printf("Recieved: %s\n", buffer);

            // check if authentication was successful
            if (strcmp(buffer, "OK") == 0)
            {
                printf("Authentication successful\n");
                break;
            }
            else
            {
                printf("Please try again...\n");
            }
        }
    }

    exit(0);
}
