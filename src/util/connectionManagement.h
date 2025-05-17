#ifndef CONNECTION_MANEGEMENT_HEADER
#define CONNECTION_MANEGEMENT_HEADER

#include "./communication.h"
#include "./fileLinkedList.h"

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
    FileNode *file_list;
} UserContext;

int add_session_to_context(HashTable *table, Session* session, char *username);
int add_file_to_context(HashTable *table, const char *filename, char *username);
Session *create_session(int interface_socketfd, int receive_socketfd, int send_socketfd);
UserContext *create_context(char *username);
int is_session_empty(Session *s);
uint32_t crc32(const char *filename);

#endif