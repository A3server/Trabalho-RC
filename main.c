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
    if (argc != 4)
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
    if (fp == NULL)
    {
        printf("Error opening file.\n");
        exit(-1);
    }

    int initial_users_number = 0;

    struct UsrList *users_list;
    users_list = malloc(sizeof(struct UsrList));
    users_list->next = NULL;
    users_list->user = NULL;

    struct NoticiaList *noticia_list;
    noticia_list = malloc(sizeof(struct NoticiaList));
    noticia_list->next = NULL;
    noticia_list->Noticia = NULL;

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
                return -1;
            }
        }
        else if (i >= 1 && i <= initial_users_number)
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
            char *id = strtok(line, ";");
            char *titulo = strtok(NULL, ";");
            char *descricao = strtok(NULL, ";");

            struct Noticia *Noticia = malloc(sizeof(struct Noticia));
            Noticia->id = malloc(strlen(id) + 1);
            strcpy(Noticia->id, id);
            Noticia->titulo = malloc(strlen(titulo) + 1);
            strcpy(Noticia->titulo, titulo);
            Noticia->descricao = malloc(strlen(descricao) + 1);
            strcpy(Noticia->descricao, descricao);

            // append to the list
            append_noticia(noticia_list, Noticia);
            i++;
        }

        i++;
    }
    printf("List of news:\n");
    list_noticias(noticia_list);

    // print all the users
    printf("List of users:\n");
    list_users(users_list);

    // create a multicast server for each topic id, if there's already a server running for that topic id, don't create another one
    // loop through the list and keep track of unique topic ids
    // get noticias size
    int noticias_size = get_noticia_size(noticia_list);
    char *topic_ids[noticias_size];
    int topic_ids_size = 0;
    struct NoticiaList *current = noticia_list->next;
    while (current != NULL)
    {
        // check if the topic id is already in the array
        int found = 0;
        for (int i = 0; i < topic_ids_size; i++)
        {
            if (strcmp(topic_ids[i], current->Noticia->id) == 0)
            {
                found = 1;
                break;
            }
        }

        // if not found, add it to the array
        if (found == 0)
        {
            topic_ids[topic_ids_size] = malloc(strlen(current->Noticia->id) + 1);
            strcpy(topic_ids[topic_ids_size], current->Noticia->id);
            topic_ids_size++;
        }

        current = current->next;
    }

    // create a multicast server for each topic id
    for (int i = 0; i < topic_ids_size; i++)
    {
        if (fork() == 0)
        {
            // create a multicast server for this topic id
            create_multicast_server(topic_ids[i], PORT);
            exit(0);
        }
    }



    // create two processes, 1 for tcp and one for udp
    if ((tcpServerPID = fork()) == 0)
    {
        // TCP server code
        tcp_server(PORT, noticia_list, users_list);
        exit(0);
    }
    else if ((udpServerPID = fork()) == 0)
    {
        // UDP server code
        udp_server(PORT_ADMIN, noticia_list, users_list);
        exit(0);
    }

    // get the main task pid 
    mainPID = getpid();
    while (1)
    {
        wait(NULL);
    }
    return 0;
}