#include "funcs.h"
/*
{Número de utilizadores iniciais} # no máximo 5
[username utilizador 1;password utilizador 1;saldo inicial]
(..)
{mercado};{ação};{preço inicial} # 2 mercados, cada um com 3 ações
(..)
*/

/*
    2
    User1;pass1;1000
    User2;pass2;1500
    bvl;stock_bvl_1;10
    bvl;stock_bvl_2;10
    bvl;stock_bvl_3;10
    nyse;stock_nyse_1;10
    nyse;stock_nyse_2;10
    nyse;stock_nyse_3;10
*/

int REFRESH_TIME;

void process_client(int client_fd, struct AcaoList *acao_list, struct UsrList *users_list)
{
    int nread = 0;
    char buffer[BUF_SIZE];
    // send to the client asking for auth
    send(client_fd, "AUTH", 5, 0);
    // receive the answer
    char *token;

    while (1)
    {
        nread = read(client_fd, buffer, BUF_SIZE);
        buffer[nread] = '\0';

        // split buffer with ;
        token = strtok(buffer, ";");
        char *username = token;
        token = strtok(NULL, ";");
        char *password = token;

        char *message;
        message = malloc(sizeof(char) * BUF_SIZE);
        // check if user exists
        if (check_valid_user_cred(users_list, username, password, 0))
        {
            message = "OK";
            send(client_fd, message, strlen(message), 0);
            printf("[SERVER TCP] Client authenticated.\n");
            break;
        }
        else
        {
            message = "Authentication Failed";
        }
        send(client_fd, message, strlen(message), 0);
    }

    // start sending/recieving commands
    do
    {
        // receive command
        nread = read(client_fd, buffer, BUF_SIZE - 1);
        buffer[nread] = '\0';
        // split buffer with ;
        token = strtok(buffer, ";");
        char *command = token;
        token = strtok(NULL, ";");
        char *param = token;

        // check if command is valid

        fflush(stdout);
    } while (nread > 0);
    close(client_fd);
}

void tcp_server(int PORT_ADMIN, struct AcaoList *acao_list, struct UsrList *users_list)
{
    int fd, client;
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
                process_client(client, acao_list, users_list);
                exit(0);
            }
            close(client);
        }
    }
}

int udp_server(int PORT, struct AcaoList *acao_list, struct UsrList *users_list)
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
    si_minha.sin_addr.s_addr = htonl(INADDR_ANY);

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

            save_to_file(users_list, acao_list);

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

            save_to_file(users_list, acao_list);

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

// TODO: DOESNT WORK
void killServers() {
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

int get_acao_size(struct AcaoList *acao_list)
{
    int i = 0;
    struct AcaoList *aux = acao_list;
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

struct Acao *get_acao(struct AcaoList *acao_list, int index)
{
    int i = 0;
    struct AcaoList *aux = acao_list;
    while (aux != NULL)
    {
        if (i == index)
        {
            return aux->acao;
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

void append_acao(struct AcaoList *acao_list, struct Acao *acao)
{
    struct AcaoList *new_acao = malloc(sizeof(struct AcaoList));
    new_acao->acao = acao;
    new_acao->next = NULL;

    if (acao_list->next == NULL)
    {
        acao_list->next = new_acao;
    }
    else
    {
        struct AcaoList *aux = acao_list->next;
        while (aux->next != NULL)
        {
            aux = aux->next;
        }
        aux->next = new_acao;
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

void list_stocks(struct AcaoList *acao_list)
{
    struct AcaoList *aux = acao_list->next;
    while (aux != NULL)
    {
        printf("%s - %s @ %f\n", aux->acao->mercado, aux->acao->nomestock, aux->acao->currentprice);
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

        /* printf("Username: %s %s\n", aux->user->name, username);
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

void save_to_file(struct UsrList *users_list, struct AcaoList *acao_list)
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

    // write acao_list:
    struct AcaoList *aux2 = acao_list->next;
    while (aux2 != NULL)
    {
        sprintf(together, "%s;%s;%f\n", aux2->acao->mercado, aux2->acao->nomestock, aux2->acao->currentprice);
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
