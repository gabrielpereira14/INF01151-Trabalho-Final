#ifndef CONNECTION_MANEGEMENT_HEADER
#define CONNECTION_MANEGEMENT_HEADER
#include <pthread.h>
#include "./communication.h"
#include "./fileLinkedList.h"
#include "./fileSync.h"

struct HashTable;
typedef struct HashTable HashTable;

struct UserContext;

typedef struct Session {
    int session_index;
    int active;

    int interface_socketfd;
    int receive_socketfd;
    int send_socketfd;

    struct UserContext *user_context;
    FileSyncBuffer sync_buffer;
} Session;

typedef struct ContextThreads{
    pthread_t *interface_thread;
    pthread_t *send_thread;
    pthread_t *receive_thread;
} ContextThreads;

typedef struct UserContext {
    char *username;
    Session *sessions[MAX_SESSIONS];
    FileNode *file_list;
    ContextThreads threads;

    pthread_mutex_t lock; 
} UserContext;

int add_session_to_context(HashTable *table, Session* session, char *username, ContextThreads threads);
int add_file_to_context(HashTable *table, const char *filename, char *username);
Session *create_session(int interface_socketfd, int receive_socketfd, int send_socketfd);
UserContext *create_context(char *username);
int is_session_empty(Session *s);
uint32_t crc32(const char *filepath);
void send_file_to_session(int send_to_index, UserContext *context, char *filepath);

#endif