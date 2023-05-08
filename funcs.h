
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


pid_t mainPID;
pid_t tcpServerPID;
pid_t udpServerPID;

struct NormalUser
{
    char *name;
    char *password;
    char *type;
};

struct Acao
{
    char *mercado;
    char *nomestock;
    double currentprice;
};
struct UsrList
{
    struct NormalUser *user;
    struct UsrList *next;
};
struct AcaoList
{
    struct Acao *acao;
    struct AcaoList *next;
};

int shmid;

int check_valid_user_cred(struct UsrList *users_list, char *username, char *password, int needsToBeAdmin);
int udp_server(int PORT, struct AcaoList *acao_list, struct UsrList *users_list);
void tcp_server(int PORT_ADMIN, struct AcaoList *acao_list, struct UsrList *users_list);
void killServers();
void terminationSignalHandler(int signal);
void erro(char *msg);
void delete_user(struct UsrList *users_list, char *username);
void list_users(struct UsrList *users_list);
char* list_users_str(struct UsrList* users_list);
void list_stocks(struct AcaoList *acao_list);
void refresh_time(char *segundos);
int user_exists(char *username, struct UsrList *users_list);
void append_user(struct UsrList *users_list, struct NormalUser *user);
void append_acao(struct AcaoList *acao_list, struct Acao *acao);
int get_users_size(struct UsrList *users_list);
int get_acao_size(struct AcaoList *acao_list);
struct NormalUser *get_user(struct UsrList *users_list, int index);
struct Acao *get_acao(struct AcaoList *acao_list, int index);
int get_markets_size(struct AcaoList *acao_list);
void save_to_file(struct UsrList *users_list, struct AcaoList *acao_list);
void write_users_tofile(struct UsrList *users_list);
#endif // FOO_H