
#ifndef FOO_H
#define FOO_H

#define MAX_COMMAND_LENGTH 50
#define BUFLEN 512
#define BUF_SIZE	1024
#define PORT_MC 8888

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
#include <pthread.h>

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

struct MulticastServerList {
    struct MCserver *server;
    struct MulticastServerList *next;
};

struct MCserver {
    int PORT;
    char *address;
    char *topicId;
};

struct UsrList *users_list;
struct MulticastServerList *multi_server_list;
struct NoticiaList *noticia_list;


int check_valid_user_cred(char *username, char *password, int needsToBeAdmin);
int udp_server(int PORT);
void tcp_server(int PORT_ADMIN);
void killServers();
void* create_multicast_server(void* arg);
void append_multicast_server(struct MCserver *multi_server);
void erro(char *msg);
void delete_user(char *username);
void list_users();
char* list_users_str();
void list_topics();
char *list_topics_str();
void refresh_time(char *segundos);
int user_exists(char *username);
void append_user(struct NormalUser *user);
void append_noticia(struct Noticia *Noticia);
int get_users_size();
char* get_user_type(char* username);
int get_noticia_size();
struct NormalUser *get_user(int index);
struct Noticia *get_noticia(int index);
void save_to_file();
void update_users_list_from_file();
char* generate_multicast_address();
#endif // FOO_H