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

void process_client(int client_fd, struct NoticiaList *noticia_list, struct UsrList *users_list)
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
        if (check_valid_user_cred(users_list, username, password, 0) == 1)
        {
            // get the user type
            char *aux_type = get_user_type(users_list, username);
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
        nread = read(client_fd, buffer, BUF_SIZE - 1);
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
            if (strcmp(get_user_type(users_list, username), "cliente") != 0)
            {
                printf("[SERVER TCP] User is not jornalista\n");
                send(client_fd, "[ERROR] User is not jornalista!", 30, 0);
                continue;
            }

            char *topics_str = list_topics_str(noticia_list);
            send(client_fd, topics_str, strlen(topics_str), 0);
        }
        else if (strcmp(command, "SUBSCRIBE_TOPIC") == 0)
        {
            // check if user is jornalista
            if (strcmp(get_user_type(users_list, username), "cliente") == 0)
            {
                printf("[SERVER TCP] User is jornalista\n");
                send(client_fd, "[ERROR] User is jornalista!", 28, 0);
                continue;
            }

            printf("[SERVER TCP] SUBSCRIBE_TOPIC\n");
            // TODO check if topic exists and connect to the multicast server corresponding to it
        }
        else if (strcmp(command, "CREATE_TOPIC") == 0)
        {
            // todo, n testei esta merda, ve se faz sentido.
            // @miguelopesantana

            // check if user is jornalista
            if (strcmp(get_user_type(users_list, username), "jornalista") != 0)
            {
                printf("[SERVER TCP] User is not jornalista\n");
                send(client_fd, "[ERROR] User is not jornalista!", 30, 0);
                continue;
            }

            printf("[SERVER TCP] CREATE_TOPIC\n");

            // check if topic exists
            if (get_noticia(noticia_list, atoi(param)) != NULL)
            {
                printf("[SERVER TCP] Topic already exists\n");
                send(client_fd, "[ERROR] Topic already exists!", 28, 0);
                continue;
            }
            // create topic
            struct Noticia *noticia = malloc(sizeof(struct Noticia));
            noticia->id = malloc(strlen(param) + 1);
            strcpy(noticia->id, param);
            noticia->titulo = malloc(strlen(param) + 1);
            strcpy(noticia->titulo, param);
            append_noticia(noticia_list, noticia);
            printf("[SERVER TCP] Topic created\n");

            // write to file
            save_to_file(users_list, noticia_list);
            send(client_fd, "OK", 3, 0);
        }
        else if (strcmp(command, "SEND_NEWS") == 0)
        {
            // check if user is jornalista
            if (strcmp(get_user_type(users_list, username), "jornalista") != 0)
            {
                printf("[SERVER TCP] User is not jornalista\n");
                send(client_fd, "[ERROR] User is not jornalista!", 30, 0);
                continue;
            }

            printf("[SERVER TCP] SEND_NEWS\n");

            // check if topic exists
            if (get_noticia(noticia_list, atoi(param)) == NULL)
            {
                printf("[SERVER TCP] Topic does not exist\n");
                send(client_fd, "[ERROR] Topic does not exist!", 28, 0);
                continue;
            }
            // todo send news to the multicast server
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

char *list_topics_str(struct NoticiaList *noticia_list)
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

void tcp_server(int PORT_ADMIN, struct NoticiaList *noticia_list, struct UsrList *users_list)
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
            if (fork() == 0)
            {
                close(fd);
                process_client(client, noticia_list, users_list);
                exit(0);
            }
            close(client);
        }
    }
}

int udp_server(int PORT, struct NoticiaList *noticia_list, struct UsrList *users_list)
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
            if (check_valid_user_cred(users_list, username, password, 1) == 1)
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
                if (check_valid_user_cred(users_list, username, password, 1) == 1)
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

            if (type == NULL)
            {
                type = malloc(1);
                type[0] = '\0';
            }
            else
            {
                type[strlen(type)] = '\0';
            }

            if (user_exists(username, users_list))
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

            append_user(users_list, user);

            save_to_file(users_list, noticia_list);

            // send ok to client
            sendto(s, "OK", 3, 0, (struct sockaddr *)&si_outra, slen);
        }
        // comand has this format DEL {username}
        else if (strcmp(command, "DEL") == 0)
        {
            username = strtok(NULL, " ");
            // find \n and replace it with \0
            username[strlen(username)] = '\0';

            if (!user_exists(username, users_list))
            {
                printf("User does not exist!\n");
                sendto(s, "[ERROR] User does not exist!", 28, 0, (struct sockaddr *)&si_outra, slen);
                continue;
            }
            printf("[CLIENT] Deleting User: %s\n", username);
            delete_user(users_list, username);

            save_to_file(users_list, noticia_list);

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

void create_multicast_server(char *topicId, int PORT)
{
    int fd;
    struct sockaddr_in addr;
    struct ip_mreq mreq;
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

    printf("[MULTICAST SERVER@%s] Server started with info: %s:%d - %d\n", topicId, inet_ntoa(addr.sin_addr), PORT, fd);

    // Receive and process incoming multicast messages
    while (1)
    {
        bzero(buffer, BUFLEN);
        if (recvfrom(fd, buffer, BUFLEN, 0, NULL, 0) < 0)
        {
            perror("recvfrom");
            exit(1);
        }
        printf("[MULTICAST@topic:%s] Received message: %s\n", topicId, buffer);

        // Process the received message
        // Add your custom processing logic here
    }

    // Cleanup and close the socket
    close(fd);
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

int get_users_size(struct UsrList *users_list)
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

int get_noticia_size(struct NoticiaList *noticia_list)
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

void append_user(struct UsrList *users_list, struct NormalUser *user)
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

struct Noticia *get_noticia(struct NoticiaList *noticia_list, int index)
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

struct NormalUser *get_user(struct UsrList *users_list, int index)
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

char *get_user_type(struct UsrList *users_list, char *username)
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

void append_noticia(struct NoticiaList *noticia_list, struct Noticia *Noticia)
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

int user_exists(char *username, struct UsrList *users_list)
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

void delete_user(struct UsrList *users_list, char *username)
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

void list_users(struct UsrList *users_list)
{
    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {
        printf("%s - %s\n", aux->user->name, aux->user->type);
        aux = aux->next;
    }
}

char *list_users_str(struct UsrList *users_list)
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

void list_topics(struct NoticiaList *noticia_list)
{
    struct NoticiaList *aux = noticia_list->next;
    while (aux != NULL)
    {
        printf("%s - %s", aux->Noticia->id, aux->Noticia->titulo);
        aux = aux->next;
    }
}

int get_users_lenght(struct UsrList *users_list)
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

int check_valid_user_cred(struct UsrList *users_list, char *username, char *password, int needsToBeAdmin)
{
    // list_users(users_list);

    struct UsrList *aux = users_list->next;
    while (aux != NULL)
    {

        /*  printf("Username: %s %s\n", aux->user->name, username);
         printf("Password: %s %s\n", aux->user->password, password);

         printf("username: %d\n", strcmp(aux->user->name, username));
         printf("password: %d\n", strcmp(aux->user->password, password));

         printf("Needs to be admin: %d\n", needsToBeAdmin);
         printf("admin: %d\n", strcmp(aux->user->type, "administrador")); */

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

void save_to_file(struct UsrList *users_list, struct NoticiaList *noticia_list)
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

void append_multicast_server(struct MulticastServerList *multi_server_list, struct MCserver *multi_server)
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
        aux->next = new_multi_server;
    }

    printf("[SERVER UDP] Multicast server added: %d:%s\n", multi_server->pid, multi_server->topicId);

}