#ifndef CONNECTION_MANEGEMENT_HEADER
#define CONNECTION_MANEGEMENT_HEADER

#include "./communication.h"

struct HashTable;
typedef struct HashTable HashTable;

struct UserContext;

typedef struct Session {
    int interface_socketfd;
    int receive_socketfd;
    int send_socketfd;

    struct UserContext *user_context;
} Session;

typedef struct UserContext {
    Session sessions[MAX_SESSIONS];
    char *username;
} UserContext;

int add_session_to_context(HashTable *table, Session* session, char *username);
Session *create_session(int interface_socketfd, int receive_socketfd, int send_socketfd);
UserContext *create_context(char *username);
int is_session_empty(Session *s);

#endif