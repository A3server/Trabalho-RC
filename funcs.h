
#ifndef FOO_H
#define FOO_H

#define MAX_COMMAND_LENGTH 50
#define BUFLEN 512
#define BUF_SIZE	1024
#include <errno.h>
#include "funcs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/shm.h>




struct NormalUser
{
    char *name;
    char *password;
    char *type;
};

struct Noticia
{
    char *id;
    char *titulo;
    char *descricao;
};
struct UsrList
{
    struct NormalUser *user;
    struct UsrList *next;
};
struct NoticiaList
{
    struct Noticia *Noticia;
    struct NoticiaList *next;
};


int check_valid_user_cred(struct UsrList *users_list, char *username, char *password, int needsToBeAdmin);
int udp_server(int PORT, struct NoticiaList *noticia_list, struct UsrList *users_list);
void tcp_server(int PORT_ADMIN, struct NoticiaList *noticia_list, struct UsrList *users_list);
void killServers();
void create_multicast_server(char* topicId, int PORT);
void erro(char *msg);
void delete_user(struct UsrList *users_list, char *username);
void list_users(struct UsrList *users_list);
char* list_users_str(struct UsrList* users_list);
void list_noticias(struct NoticiaList *noticia_list);
void refresh_time(char *segundos);
int user_exists(char *username, struct UsrList *users_list);
void append_user(struct UsrList *users_list, struct NormalUser *user);
void append_noticia(struct NoticiaList *noticia_list, struct Noticia *Noticia);
int get_users_size(struct UsrList *users_list);
char* get_user_type(struct UsrList *users_list, char* username);
int get_noticia_size(struct NoticiaList *noticia_list);
struct NormalUser *get_user(struct UsrList *users_list, int index);
struct Noticia *get_noticia(struct NoticiaList *noticia_list, int index);
void save_to_file(struct UsrList *users_list, struct NoticiaList *noticia_list);
void write_users_tofile(struct UsrList *users_list);
#endif // FOO_H