#include "funcs.h"
/*
    Adicionar um utilizador com uma identificação e uma password, bem como especificar a que
    mercados pode ter acesso (no máximo existem 2 mercados diferentes) e o saldo da sua
    conta; caso já exista um utilizador com aquele nome e password apenas alterará as bolsas a
    que tem acesso e/ou o saldo. O número de utilizadores está limitado a 10 (além do
    administrador).
    ▪ ADD_USER {username} {password} {bolsas a que tem acesso} {saldo}
    o Eliminar um utilizador
    ▪ DEL {username}
    o Lista utilizadores
    ▪ LIST
    o Configura tempo de atualização do valor das ações geradas pelo servidor
    ▪ REFRESH {segundos}
    o Sair da consola
    ▪ QUIT
    o Desligar servidor
    ▪ QUIT_SERVER
*/

int main(int argc, char *argv[])
{
    if(argc != 4)
    {
        printf("Usage: ./news_server <PORT_NEWS> <PORT_CONFIG> <config_file>\n");
        exit(-1);
    }

    int PORT = atoi(argv[1]);
    int PORT_ADMIN = atoi(argv[2]);
    char *config_file = argv[3];


    // read the database
    FILE *fp = fopen(config_file, "r");
    // check if something went wrong
    if(fp == NULL)
    {
        printf("Error opening file.\n");
        exit(-1);
    }

    int initial_users_number = 0;
    struct RootUser *root;
    root = malloc(sizeof(struct RootUser));

    struct UsrList *users_list;
    users_list = malloc(sizeof(struct UsrList));
    users_list->next = NULL;
    users_list->user = NULL;

    struct AcaoList *acao_list;
    acao_list = malloc(sizeof(struct AcaoList));
    acao_list->next = NULL;
    acao_list->acao = NULL;

    
    char line[256];
    int i = 0;
    while (fgets(line, sizeof(line), fp))
    {
        //if you fget only \n
        if (line[0] == '\n')
        {
            continue;
        }


        if (i == 0)
        {
            // user: User1;pass1;1000
            char *name = strtok(line, "/");
            char *pass = strtok(NULL, "");
            pass[strcspn(pass, "\r\n")] = '\0';


            root->name = malloc(strlen(name) + 1);
            strcpy(root->name, name);
            root->password = malloc(strlen(pass) + 1);
            strcpy(root->password, pass);
        }
        else if (i == 1)
        {
            // read line
            initial_users_number = atoi(line);

            // check if greater than 10, if it is throw errr
            if (initial_users_number > 10)
            {
                printf("Error: The number of users is greater than 10\n");
                return -1;
            }
        }
        else if (i >= 2 && i <= initial_users_number + 1)
        {
            int len = get_users_size(users_list);
            if (len >= 10)
            {
                printf("Error: The number of users is greater than 10\n");
                return -1;
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
                return -1;
            }

        
            user->type = malloc(strlen(type) + 1);
            strcpy(user->type, type);

            // append to the list
            append_user(users_list, user);
        }
        else
        {
            // read line
            char *market = strtok(line, ";");
            char *nomeacao = strtok(NULL, ";");
            char *preco = strtok(NULL, ";");

            struct Acao *acao = malloc(sizeof(struct Acao));
            acao->mercado = malloc(strlen(market) + 1);
            strcpy(acao->mercado, market);
            acao->nomestock = malloc(strlen(nomeacao) + 1);
            strcpy(acao->nomestock, nomeacao);

            acao->currentprice = atof(preco);
            // append to the list

            append_acao(acao_list, acao);
            i++;
        }

        i++;
    }
    // list the root user, all the normal users, and all the news
    printf("Root user: %s %s\n", root->name, root->password);

    printf("List of news:\n");
    list_stocks(acao_list);

    // print all the users
    printf("List of users:\n");
    list_users(users_list);

    //create two processes, 1 for tcp and one for udp
    if(fork()==0) {
        // tcp - clientes non admin 9876
        // use ./operations_terminal 127.0.0.1 9876
        tcp_server(PORT, acao_list, users_list, root);
    }
    else if(fork()==0) {
        // udp - clientes admin 9877
        // run netcat like so:  nc -l -u 127.0.0.1 9877
        udp_server(PORT_ADMIN, acao_list, users_list, root);
    }
    // parent process
    while(1){
        wait(NULL);
    }
    return 0;
}