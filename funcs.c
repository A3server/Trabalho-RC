#include "funcs.h"
/*
{Número de utilizadores iniciais} # no máximo 5
[username utilizador 1;password utilizador 1;saldo inicial]
(..)
{id_topic};{titulo noticia};{descrição}
(..)
*/

int REFRESH_TIME;
extern pid_t mainPID;
extern pid_t tcpServerPID;
extern pid_t udpServerPID;
extern int shmid;
extern struct UsrList *users_list;
extern struct MulticastServerList *multi_server_list;
extern struct NoticiaList *noticia_list;
extern pthread_t threads[100];

void process_client(int client_fd)
{
    int nread = 0;
    char buffer[BUF_SIZE];
    // send to the client asking for auth
    send(client_fd, "AUTH", 5, 0);
    // receive the answer
    char *token;
    char *username;
    char *type;

    while (1)
    {
        nread = read(client_fd, buffer, BUF_SIZE);
        buffer[nread] = '\0';

        // split buffer with ;
        token = strtok(buffer, ";");
        // copy the memory to username
        username = malloc(strlen(token) + 1);
        strcpy(username, token);
        token = strtok(NULL, ";");
        char *password = token;

        char *message = malloc(sizeof(char) * BUF_SIZE);

        // check if user exists
        if (check_valid_user_cred(username, password, 0) == 1)
        {
            // get the user type
            char *aux_type = get_user_type(username);
            // copy the memory to type
            type = malloc(strlen(aux_type) + 1);
            strcpy(type, aux_type);

            // create a message with OK-{type}
            strcat(message, "OK-");
            strcat(message, type);

            printf("[SERVER TCP] Client authenticated: %s\n", message);

            send(client_fd, message, strlen(message), 0);
            break;
        }
        else
        {
            message = "NOK";
        }
        send(client_fd, message, strlen(message), 0);
    }

    // start sending/recieving commands
    do
    {
        // receive command
        nread = read(client_fd, buffer, BUF_SIZE);
        buffer[nread] = '\0';

        printf("[SERVER TCP] Received command: %s\n", buffer);
        // split buffer with ;
        token = strtok(buffer, " ");
        char *command = token;
        token = strtok(NULL, " ");
        char *param = token;

        // printf("[SERVER TCP] Command: %d\n", strcmp(command, "LIST_TOPICS"));

        // check if command is valid
        /**
         * LIST_TOPICS
         * SUBSCRIBE_TOPIC <id do tópico>
         * CREATE_TOPIC <id do tópico> <título do tópico>
         * SEND_NEWS <id do tópico> <noticia>
         */
        if (strcmp(command, "LIST_TOPICS") == 0)
        {
            /* if (strcmp(get_user_type(username), "cliente") != 0)
            {
                printf("[SERVER TCP] User is not jornalista\n");
                send(client_fd, "[ERROR] User is not jornalista!", 30, 0);
                continue;
            } */

            char *topics_str = list_topics_str();
            send(client_fd, topics_str, strlen(topics_str), 0);
        }
        else if (strcmp(command, "SUBSCRIBE_TOPIC") == 0)
        {
            // check if user is jornalista
            /* if (strcmp(get_user_type(username), "cliente") != 0)
            {
                printf("[SERVER TCP] User is not cliente\n");
                send(client_fd, "[ERROR] User is not cliente!", 28, 0);
                continue;
            } */

            printf("[SERVER TCP] SUBSCRIBE_TOPIC:%s\n", param);
            // TODO check if topic exists and connect to the multicast server corresponding to it

            // check if topic exists
            if (get_noticia(atoi(param)) == NULL)
            {
                printf("[SERVER TCP] Topic does not exist\n");
                send(client_fd, "[ERROR] Topic does not exist!", 28, 0);
                continue;
            }

            list_multicast_servers();

            // send multicast server info
            struct MulticastServerList *current = multi_server_list->next;
            while (current != NULL)
            {
                // printf("[SERVER TCP] Current topic: %s\n", current->server->topicId);
                // printf("[SERVER TCP] Topic id: %s\n", param);
                if (strcmp(current->server->topicId, param) == 0)
                {
                    char *message = malloc(sizeof(char) * BUF_SIZE);
                    sprintf(message, "%s %d", current->server->address, current->server->PORT);
                    printf("[SERVER TCP] Sent Server Info: %s\n", message);
                    send(client_fd, message, strlen(message), 0);
                    break;
                }
                current = current->next;
            }
            
        }
        else if (strcmp(command, "CREATE_TOPIC") == 0)
        {
            char *title = strtok(NULL, " ");

            printf("[SERVER TCP] CREATE_TOPIC:%s\n", param);

            // check if topic exists
            if (get_noticia(atoi(param)) != NULL)
            {
                printf("[SERVER TCP] Topic already exists\n");
                send(client_fd, "[ERROR] Topic already exists!", 28, 0);
                continue;
            }


            // create topic
            struct Noticia *noticia = malloc(sizeof(struct Noticia));
            noticia->id = malloc(strlen(param) + 1);
            strcpy(noticia->id, param);
            noticia->titulo = malloc(strlen(title) + 1);
            strcpy(noticia->titulo, title);
            append_noticia(noticia);
            printf("[SERVER TCP] Topic created\n");

            // needs to start a new multicast server
            struct MCserver *multi_server = malloc(sizeof(struct MCserver));
            int noticia_size = get_noticia_size();
            multi_server->PORT = PORT_MC + noticia_size;
            multi_server->address = malloc(17);
            multi_server->topicId = malloc(strlen(param) + 1);
            strcpy(multi_server->address, generate_multicast_address(multi_server_list));
            strcpy(multi_server->topicId, param);

            // append to the list
            append_multicast_server(multi_server);

            // create a thread for the multicast server
            if (pthread_create(&threads[noticia_size], NULL, create_multicast_server, (void *)multi_server) != 0)
            {
                printf("Error creating thread\n");
                send(client_fd, "[ERROR] Error creating thread!", 28, 0);
                exit(1);
            }

            // send multicast server info
            char *message = malloc(sizeof(char) * BUF_SIZE);
            sprintf(message, "%s %d", multi_server->address, multi_server->PORT);
            // printf("[SERVER TCP]Sent Server Info: %s\n", message);
            send(client_fd, message, strlen(message), 0);

            // write to file
            save_to_file();
        }
        else if (strcmp(command, "SEND_NEWS") == 0)
        {
            // printf("[SERVER TCP] SEND_NEWS\n");

            // check if topic exists
            if (get_noticia(atoi(param)) == NULL)
            {
                printf("[SERVER TCP] Topic does not exist\n");
                send(client_fd, "[ERROR] Topic does not exist!", 28, 0);
                continue;
            }
            
            // send multicast server info
            struct MulticastServerList *current = multi_server_list->next;
            while (current != NULL)
            {
                //printf("[SERVER TCP] Current topic: %s\n", current->server->topicId);
                //printf("[SERVER TCP] Topic id: %s\n", param);
                if (strcmp(current->server->topicId, param) == 0)
                {
                    char *message = malloc(sizeof(char) * BUF_SIZE);
                    sprintf(message, "%s %d", current->server->address, current->server->PORT);
                    printf("[SERVER TCP] Sent Server Info: %s\n", message);
                    send(client_fd, message, strlen(message), 0);
                    break;
                }
                current = current->next;
            }

        }
        else
        {
            printf("[SERVER TCP] Invalid command\n");
            send(client_fd, "[ERROR] Invalid command!", 25, 0);
        }

        fflush(stdout);
    } while (nread > 0);
    close(client_fd);
}

char *list_topics_str()
{
    char *topics_str = malloc(sizeof(char) * BUF_SIZE);
    bzero(topics_str, BUF_SIZE);
    struct NoticiaList *current = noticia_list->next;
    while (current != NULL)
    {
        strcat(topics_str, current->Noticia->id);
        strcat(topics_str, " - ");
        strcat(topics_str, current->Noticia->titulo);
        current = current->next;
    }
    return topics_str;
}

void tcp_server(int PORT_ADMIN)
{
    int fd, client;
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // localhost
    addr.sin_port = htons(PORT_ADMIN);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (fd < 0)
        erro("na funcao socket");
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        erro("na funcao bind");
    if (listen(fd, 5) < 0)
        erro("na funcao listen");
    client_addr_size = sizeof(client_addr);
    printf("[SERVER TCP] Started.\n");
    while (1)
    {
        // clean finished child processes, avoiding zombies
        // must use WNOHANG or would block whenever a child process was working
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
        // wait for new connection
        client = accept(fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
        if (client > 0)
        {
            printf("[SERVER TCP] Client Connected\n");
            update_users_list_from_file();
            if (fork() == 0)
            {
                close(fd);
                process_client(client);
                exit(0);
            }
            close(client);
        }
    }
}

int udp_server(int PORT)
{
    // server start:
    struct sockaddr_in si_minha, si_outra;

    int s, recv_len;
    socklen_t slen = sizeof(si_outra);
    char buf[BUFLEN];

    // Cria um socket para recepção de pacotes UDP
    if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        erro("Erro na criação do socket do cliente.");
    }

    // Preenchimento da socket address structure
    si_minha.sin_family = PF_INET;
    si_minha.sin_port = htons(PORT);
    si_minha.sin_addr.s_addr = htonl(INADDR_ANY); // localhost

    // Associa o socket à informação de endereço
    if (bind(s, (struct sockaddr *)&si_minha, sizeof(si_minha)) == -1)
    {
        erro("Erro no bind do cliente.");
    }

    char *username;
    char *password;
    char *type;

    char cChar;

    printf("[SERVER UDP] Waiting for packets\n");

    // Espera recepção de mensagem (a chamada é bloqueante)
    if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen)) == -1)
    {
        erro("[CLIENT] Erro no recvfrom");
    }

    // Para ignorar o restante conteúdo (anterior do buffer)
    buf[recv_len] = '\0';

    printf("[SERVER UDP] Received packet from %s:%d (size: %d bytes) - %s \n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port), recv_len, buf);

    buf[strcspn(buf, "\r\n")] = '\0'; // remove \r\n from buffer

    // ask for authentication (if not, send error message)
    if (strcmp(buf, "AUTH") == 0)
    {
        printf("[SERVER UDP] AUTH\n");
        // send to the client asking for auth

        sendto(s, "AUTH", 6, 0, (struct sockaddr *)&si_outra, slen);
        while (1)
        {
            // Espera recepção de mensagem (a chamada é bloqueante)
            if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen)) == -1)
            {
                erro("[CLIENT] Erro no recvfrom");
            }

            buf[recv_len] = '\0';
            // split buffer with ;
            char *token;
            printf("[SERVER UDP] Received AUTH packet from %s:%d (size: %d bytes) - %s \n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port), recv_len, buf);
            token = strtok(buf, ";");
            username = token;
            token = strtok(NULL, ";");
            password = token;
            password[strcspn(password, "\r\n")] = '\0'; // remove \r\n from buffer

            // check if user exists
            if (check_valid_user_cred(username, password, 1) == 1)
            {
                // send OK
                printf("[SERVER UDP] AUTH OK\n");
                sendto(s, "OK", 3, 0, (struct sockaddr *)&si_outra, slen);
                break;
            }
            else
            {
                // send FAIL
                printf("[SERVER UDP] AUTH FAILED\n");
                sendto(s, "FAIL", 4, 0, (struct sockaddr *)&si_outra, slen);
            }
        }
    }
    else
    {
        printf("[SERVER UDP] NO AUTH TOKEN FAILED\n");
        sendto(s, "AUTH FAILED", 12, 0, (struct sockaddr *)&si_outra, slen);
        return 0;
    }

    while (1)
    {

        // Espera recepção de mensagem (a chamada é bloqueante)
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen)) == -1)
        {
            erro("[CLIENT] Erro no recvfrom");
        }

        // Para ignorar o restante conteúdo (anterior do buffer)
        buf[recv_len] = '\0';

        /*         if (strcmp(buf, "X") == 0)
                {
                    continue;
                } */

        printf("Client[%s:%d] %s\n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port), buf);

        char *command = strtok(buf, " \n");

        // auth in the middle of the session
        if (strcmp(buf, "AUTH") == 0)
        {
            printf("[SERVER UDP] AUTH\n");
            // send to the client asking for auth

            sendto(s, "AUTH", 6, 0, (struct sockaddr *)&si_outra, slen);
            while (1)
            {
                // Espera recepção de mensagem (a chamada é bloqueante)
                if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen)) == -1)
                {
                    erro("[CLIENT] Erro no recvfrom");
                }

                buf[recv_len] = '\0';
                // split buffer with ;
                char *token;
                printf("[SERVER UDP] Received AUTH packet from %s:%d (size: %d bytes) - %s \n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port), recv_len, buf);
                token = strtok(buf, ";");
                username = token;
                token = strtok(NULL, ";");
                password = token;
                password[strcspn(password, "\r\n")] = '\0'; // remove \r\n from buffer

                // check if user exists
                if (check_valid_user_cred(username, password, 1) == 1)
                {
                    // send OK
                    printf("[SERVER UDP] AUTH OK\n");
                    sendto(s, "OK", 3, 0, (struct sockaddr *)&si_outra, slen);

                    // wait for command again
                    if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen)) == -1)
                    {
                        erro("[CLIENT] Erro no recvfrom");
                    }
                    // Para ignorar o restante conteúdo (anterior do buffer)
                    buf[recv_len] = '\0';
                    command = strtok(buf, " \n");
                    break;
                }
                else
                {
                    // send FAIL
                    printf("[SERVER UDP] AUTH FAILED\n");
                    sendto(s, "FAIL", 4, 0, (struct sockaddr *)&si_outra, slen);
                }
            }
        }

        // comand has this format ADD_USER {username} {password} {type (administrador/cliente/jornalista)}
        if (strcmp(command, "ADD_USER") == 0)
        {

            username = strtok(NULL, " ");
            password = strtok(NULL, " ");
            type = strtok(NULL, " ");

            // check if password or type are null

            if (username == NULL || password == NULL || type == NULL)
            {
                printf("[CLIENT] Invalid command: %s\n", command);
                // send error
                sendto(s, "[ERROR] Invalid command!", 24, 0, (struct sockaddr *)&si_outra, slen);
                continue;
            }

            if (user_exists(username))
            {
                printf("[CLIENT] User already exists!\n");
                // send error
                sendto(s, "[ERROR] User already exists!", 28, 0, (struct sockaddr *)&si_outra, slen);
                continue;
            }

            // printf("User: %s Password: %s tipo: %s\n", username, password, saldo);

            struct NormalUser *user = malloc(sizeof(struct NormalUser));
            user->name = malloc(strlen(username) + 1);
            strcpy(user->name, username);
            user->password = malloc(strlen(password) + 1);
            strcpy(user->password, password);

            user->type = malloc(strlen(type) + 1);
            strcpy(user->type, type);

            append_user(user);

            save_to_file();

            // update the number of users
            update_users_list_from_file();

            // list_users();

            // send ok to client
            sendto(s, "OK", 3, 0, (struct sockaddr *)&si_outra, slen);
        }
        // comand has this format DEL {username}
        else if (strcmp(command, "DEL") == 0)
        {
            username = strtok(NULL, " ");
            // find \n and replace it with \0
            username[strlen(username)] = '\0';

            if (!user_exists(username))
            {
                printf("User does not exist!\n");
                sendto(s, "[ERROR] User does not exist!", 28, 0, (struct sockaddr *)&si_outra, slen);
                continue;
            }
            printf("[CLIENT] Deleting User: %s\n", username);
            delete_user(username);

            save_to_file();
            update_users_list_from_file();

            // send to client OK
            sendto(s, "OK", 3, 0, (struct sockaddr *)&si_outra, slen);
        }
        // comand has this format LIST
        else if (strcmp(command, "LIST") == 0)
        {
            char *users_str = list_users_str(users_list);
            sendto(s, users_str, strlen(users_str), 0, (struct sockaddr *)&si_outra, slen);
        }
        // comand has this format QUIT
        else if (strcmp(command, "QUIT") == 0)
        {
            printf("[CLIENT] Quitting...\n");
            // DISCONNECT CLIENT
            // send QUIT to client
            if (sendto(s, "QUIT", strlen("QUIT"), 0, (struct sockaddr *)&si_outra, slen) == -1)
            {
                erro("Erro no sendto");
            }
            continue;
        }
        else if (strcmp(command, "QUIT_SERVER") == 0)
        {
            // qual é a diferença, SAIR DO SERVER
            printf("[SERVER] Quitting...\n");
            break;
        }
        else
        {
            printf("[CLIENT] Invalid command: %s\n", command);
            // send error
            sendto(s, "[ERROR] Invalid command!", 24, 0, (struct sockaddr *)&si_outra, slen);
        }
    }

    //! warning this closes the UDP Thread but not the TCP Thread.
    // send to CLIENT goodbye
    if (sendto(s, "GOODBYE", strlen("GOODBYE"), 0, (struct sockaddr *)&si_outra, slen) == -1)
    {
        erro("Erro no sendto");
    }
    close(s);
    killServers();
    exit(0);
    return 0;
}

void *create_multicast_server(void *arg)
{
    struct MCserver *mcServer = (struct MCserver *)arg;
    char *topicId = mcServer->topicId;
    char *multicastADDR = mcServer->address;
    int PORT = mcServer->PORT;

    int fd;
    char buffer[BUFLEN];

    // Create a UDP socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    // Set socket options to allow multicast
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in addr;

    // Bind the socket to the multicast address and port
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // localhost  
    addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    struct ip_mreq mreq;
    // in localhost
    mreq.imr_multiaddr.s_addr = inet_addr(multicastADDR);
    // use DEFAULT interface
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    printf("[MULTICAST SERVER@%s] Server started with info %s:%d - MCGROUP:%s:%d\n",
           topicId, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), multicastADDR, PORT);

    while (1)
    {
        char buffer[1024];
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

        printf("[SERVER-LISTENING-MULTICAST:%s] Received message from %s:%d: %s\n", topicId,
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
        // Process the received message here
    }
    printf("[SERVER-LISTENING-MULTICAST:%s] Server closed\n", topicId);
    // Cleanup and close the socket
    close(fd);
    pthread_exit(NULL);
}

// TODO: DOESNT WORK
void killServers()
{
    printf("Killing servers: %d %d\n", tcpServerPID, udpServerPID);
    if (tcpServerPID > 0)
        kill(tcpServerPID, SIGKILL);
    if (udpServerPID > 0)
        kill(udpServerPID, SIGKILL);
}

void erro(char *s)
{
    perror(s);
    exit(1);
}

int get_number_of_users()
{
    // check the second line of the database
    FILE *fp = fopen("database.txt", "r");
    char *line = NULL;
    size_t len = 0;

    int i = 0;
    while (fgets(line, len, fp) != NULL)
    {
        if (i == 1)
        {
            return atoi(line);
        }
        i++;
    }
    return 0;
}

int get_users_size()
{
    int i = 0;
    struct UsrList *aux = users_list;
    while (aux != NULL)
    {
        i++;
        aux = aux->next;
    }
    return i;
}

int get_noticia_size()
{
    int i = 0;
    struct NoticiaList *aux = noticia_list;
    while (aux != NULL)
    {
        i++;
        aux = aux->next;
    }
    return i;
}

void append_user(struct NormalUser *user)
{
    struct UsrList *aux = users_list;
    while (aux->next != NULL)
    {
        aux = aux->next;
    }
    aux->next = malloc(sizeof(struct UsrList));

    aux->next->user = user;

    aux->next->next = NULL;
}

struct Noticia *get_noticia(int index)
{
    int i = 0;
    struct NoticiaList *aux = noticia_list;
    while (aux != NULL)
    {
        if (i == index)
        {
            return aux->Noticia;
        }
        i++;
        aux = aux->next;
    }
    return NULL;
}

struct NormalUser *get_user(int index)
{
    int i = 0;
    struct UsrList *aux = users_list;
    while (aux != NULL)
    {
        if (i == index)
        {
            return aux->user;
        }
        i++;
        aux = aux->next;
    }
    return NULL;
}

char *get_user_type(char *username)
{
    // printf("Checking if user exists: %s\n", username);
    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        if (strcmp(aux->user->name, username) == 0)
        {
            // printf("[SERVER UDP] User type: %s\n", aux->user->type);
            return aux->user->type;
        }
        aux = aux->next;
    }
    return NULL;
}

void append_noticia(struct Noticia *Noticia)
{
    struct NoticiaList *new_noticia = malloc(sizeof(struct NoticiaList));
    new_noticia->Noticia = Noticia;
    new_noticia->next = NULL;

    if (noticia_list->next == NULL)
    {
        noticia_list->next = new_noticia;
    }
    else
    {
        struct NoticiaList *aux = noticia_list->next;
        while (aux->next != NULL)
        {
            aux = aux->next;
        }
        aux->next = new_noticia;
    }
}

int user_exists(char *username)
{
    // printf("Checking if user exists: %s\n", username);

    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        /* printf("name: %s\n", aux->user->name);
        printf("Checking if user exists: %d\n", strcmp(aux->user->name, username)); */
        if (strcmp(aux->user->name, username) == 0)
        {
            return 1;
        }
        aux = aux->next;
    }
    return 0;
}

void refresh_time(char *segundos)
{
    // write the user to the database
    // parse seconds to int
    REFRESH_TIME = atoi(segundos);
    printf("Refresh time set to %d seconds\n", REFRESH_TIME);
}

void delete_user(char *username)
{
    struct UsrList *aux = users_list;
    while (aux->next != NULL)
    {
        if (strcmp(aux->next->user->name, username) == 0)
        {
            struct UsrList *aux2 = aux->next;
            aux->next = aux->next->next;
            free(aux2);
            return;
        }
        aux = aux->next;
    }
}

void list_users()
{
    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        printf("%s - %s\n", aux->user->name, aux->user->type);
        aux = aux->next;
    }
}

char *list_users_str()
{
    struct UsrList *aux = users_list->next;
    char *result = NULL;    // Pointer to store the result string
    size_t result_size = 0; // Current size of the result string

    while (aux != NULL)
    {
        // Allocate memory for the new line to be added to the result string
        size_t line_size = strlen(aux->user->name) + strlen(aux->user->type) + 4; // 4 accounts for the additional characters: ' - ' and '\n'
        char *line = malloc(line_size + 1);                                       // +1 for null terminator
        snprintf(line, line_size + 1, "%s - %s\n", aux->user->name, aux->user->type);

        // Resize the result string to accommodate the new line
        result = realloc(result, result_size + line_size + 1); // +1 for null terminator

        // Append the new line to the result string
        strcat(result, line);
        result_size += line_size;

        free(line); // Free the memory allocated for the line

        aux = aux->next;
    }

    return result;
}

void list_topics()
{
    struct NoticiaList *aux = noticia_list->next;
    while (aux != NULL)
    {
        printf("%s - %s", aux->Noticia->id, aux->Noticia->titulo);
        aux = aux->next;
    }
}

int get_users_lenght()
{
    int i = 0;
    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        i++;
        aux = aux->next;
    }
    return i;
}

int check_valid_user_cred(char *username, char *password, int needsToBeAdmin)
{
    // list_users(users_list);

    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {

        /*
        printf("Username: %s %s\n", aux->user->name, username);
        printf("Password: %s %s\n", aux->user->password, password);

        printf("username: %d\n", strcmp(aux->user->name, username));
        printf("password: %d\n", strcmp(aux->user->password, password));

        printf("Needs to be admin: %d\n", needsToBeAdmin);
        printf("admin: %d\n", strcmp(aux->user->type, "administrador"));
        */
        if (strcmp(aux->user->name, username) == 0 && strcmp(aux->user->password, password) == 0)
        {
            if (needsToBeAdmin == 1)
            {
                if (strcmp(aux->user->type, "administrador") == 0)
                {
                    printf("[SERVER UDP] User logged In is admin!\n");
                    return 1;
                }
                printf("[SERVER UDP] User logged In is not admin!\n");
                return 0;
            }
            return 1;
        }
        aux = aux->next;
    }
    return 0;
}

void save_to_file()
{
    // check if database has something inside, if it does delete it

    FILE *fp = fopen("database.txt", "w");
    char *together = malloc(sizeof(char) * 500);
    memset(together, 0, 500); //! important

    char *toprint = malloc(sizeof(char) * 3000);
    memset(toprint, 0, 3000); //! important
    // write users:
    // get users lenght
    int users_lenght = get_users_lenght(users_list);
    sprintf(together, "%d\n", users_lenght);
    strcat(toprint, together);

    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        if (aux->user->type != NULL && strlen(aux->user->type) > 0)
        {
            sprintf(together, "%s;%s;%s\n", aux->user->name, aux->user->password, aux->user->type);
        }
        else
        {
            sprintf(together, "%s;%s;%s\n", aux->user->name, aux->user->password, "cliente");
        }
        //  add \n to the end of the string

        strcat(toprint, together);
        aux = aux->next;
    }
    // printf("%s\n", toprint);

    // write noticia_list:
    struct NoticiaList *aux2 = noticia_list->next;
    while (aux2 != NULL)
    {
        sprintf(together, "%s;%s", aux2->Noticia->id, aux2->Noticia->titulo);
        strcat(toprint, together);
        aux2 = aux2->next;
    }
    // add \0 at the end of toprint
    strcat(toprint, "\0");

    // for each char in toprint, put in database.txt
    int i = 0;
    while (toprint[i] != '\0')
    {
        // printf("%c", toprint[i]);
        fputc(toprint[i], fp);
        i++;
    }

    // close file
    fflush(fp);
    fclose(fp);
}

void update_users_list_from_file()
{
    // update the global variable users_list from the users in the database.txt file
    FILE *fp = fopen("database.txt", "r");

    int initial_users_number = 0;
    char line[256];
    int i = 0;
    while (fgets(line, sizeof(line), fp))
    {
        // if you fget only \n
        if (line[0] == '\n')
        {
            continue;
        }

        if (i == 0)
        {
            // read line
            initial_users_number = atoi(line);

            // check if greater than 10, if it is throw errr
            if (initial_users_number > 10)
            {
                printf("Error: The number of users is greater than 10\n");
                return;
            }
        }
        else if (i >= 1 && i <= initial_users_number)
        {
            int len = get_users_size(users_list);
            if (len >= 10)
            {
                printf("Error: The number of users is greater than 10\n");
                return;
            }

            // user: User1;pass1;(administrador/cliente/jornalista)
            char *name = strtok(line, ";");
            char *pass = strtok(NULL, ";");
            char *type = strtok(NULL, "\n");

            // read line
            struct NormalUser *user = malloc(sizeof(struct NormalUser));
            user->name = malloc(strlen(name) + 1);
            strcpy(user->name, name);

            user->password = malloc(strlen(pass) + 1);
            strcpy(user->password, pass);

            // remove the \n from the end of type
            type[strcspn(type, "\r\n")] = '\0';

            // check if type has the correct format
            if (strcmp(type, "administrador") != 0 && strcmp(type, "cliente") != 0 && strcmp(type, "jornalista") != 0)
            {
                printf("Error: The type of user is not valid\n");
                return;
            }

            user->type = malloc(strlen(type) + 1);
            strcpy(user->type, type);

            // append to the list from the global variable
            struct UsrList *aux = users_list->next;
            while (aux->next != NULL)
            {
                aux = aux->next;
            }
            aux->next = malloc(sizeof(struct UsrList));
            aux->next->user = user;
            aux->next->next = NULL;

            // update users_list with the new user, copy the memory from *aux, which is the new global variable from the list of users into the global variable users_list
            memcpy(users_list, aux, sizeof(struct UsrList)* (len + 1));
            printf("User %s added\n", user->name);
            list_users();
        }
    }
}

void append_multicast_server(struct MCserver *multi_server)
{
    struct MulticastServerList *new_multi_server = malloc(sizeof(struct MulticastServerList));
    new_multi_server->server = multi_server;
    new_multi_server->next = NULL;

    if (multi_server_list->next == NULL)
    {
        multi_server_list->next = new_multi_server;
    }
    else
    {
        struct MulticastServerList *aux = multi_server_list->next;
        while (aux->next != NULL)
        {
            aux = aux->next;
        }
        // malloc the next one
        aux->next = new_multi_server;
    }

    printf("[SERVER UDP] Multicast server added: %s - %s:%d\n", multi_server->topicId, multi_server->address, multi_server->PORT);
}

void list_multicast_servers()
{
    printf("[SERVER UDP] Multicast servers:\n");
    struct MulticastServerList *aux = multi_server_list->next;
    while (aux != NULL)
    {
        printf("[MULTICAST] %s - %s:%d\n", aux->server->topicId, aux->server->address, aux->server->PORT);
        aux = aux->next;
    }
}

struct MCserver *get_multicast_server(char *topicId)
{
    struct MulticastServerList *aux = multi_server_list->next;
    while (aux != NULL)
    {
        if (strcmp(aux->server->topicId, topicId) == 0)
        {
            return aux->server;
        }
        aux = aux->next;
    }
    return NULL;
}

char *generate_multicast_address(struct MulticastServerList *multicast_list)
{
    struct MulticastServerList *current = multicast_list;
    char *address = malloc(16);

    while (current != NULL)
    {
        struct MCserver *server = current->server;

        // Check if the server has an assigned address
        if (server->address != NULL)
        {
            strncpy(address, server->address, 16);
            char *lastByte = strrchr(address, '.');

            if (lastByte != NULL)
            {
                // Extract the last byte of the address
                int lastValue = atoi(lastByte + 1);

                // Increment the last byte by 1
                lastValue = (lastValue + 1) % 256;

                // Update the last byte in the address
                sprintf(lastByte + 1, "%d", lastValue);
            }

            return address;
        }

        current = current->next;
    }

    // Assign the initial multicast address (224.0.0.1) if no previous address is found
    strncpy(address, "224.0.0.1", 16);
    return address;
}