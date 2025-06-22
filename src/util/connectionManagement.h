#ifndef CONNECTION_MANEGEMENT_HEADER
#define CONNECTION_MANEGEMENT_HEADER

#include <netinet/in.h>
#include <pthread.h>
#include "./fileSync.h"
#include "./communication.h"
#include "./fileLinkedList.h"


struct HashTable;
typedef struct HashTable HashTable;
struct UserContext;

typedef struct SessionThreads{
    pthread_t interface_thread;
    pthread_t send_thread;
    pthread_t receive_thread;
} SessionThreads;


typedef struct SessionSockets{
    int interface_socketfd;
    int receive_socketfd;
    int send_socketfd;
} SessionSockets;

typedef struct Session {
    int session_index;
    int active;

    struct sockaddr_in device_address; 
    SessionSockets sockets;
    SessionThreads threads;
    
    FileSyncBuffer sync_buffer;
    struct UserContext *user_context;
} Session;

typedef struct UserContext {
    char *username;
    Session *sessions[MAX_SESSIONS];
    FileNode *file_list;

    pthread_mutex_t lock; 
} UserContext;

UserContext *get_or_create_context(HashTable *table, char *username);
int find_free_session_index(UserContext *context);
int add_file_to_context(HashTable *table, const char *filename, char *username);
Session *create_session(int index, UserContext *context, SessionSockets sockets, struct sockaddr_in device_address);
UserContext *create_context(char *username);
int is_session_empty(Session *s);
uint32_t crc32(const char *filepath);
void send_file_to_session(int send_to_index, UserContext *context, char *filepath);
Session *get_user_session(UserContext *context, int session_index);
Session *get_user_session_by_address(UserContext *context, const struct sockaddr_in *target_address);
void disconnect_all_users(HashTable *table);

#endif