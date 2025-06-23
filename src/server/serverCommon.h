#ifndef SERVER_H
#define SERVER_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <stdatomic.h>


#include "../util/communication.h"
#include "../util/connectionManagement.h"
#include "../util/contextHashTable.h"
#include "../util/fileSync.h"

enum ReplicaEventType { EVENT_CLIENT_CONNECTED, EVENT_CLIENT_DISCONNECTED, EVENT_FILE_UPLOADED, EVENT_REPLICA_ADDED, EVENT_HEARTBEAT, EVENT_FILE_DELETED};
typedef struct ReplicaEvent {
    enum ReplicaEventType type;
    struct sockaddr_in device_address;
    char *username;
    char *filepath;
} ReplicaEvent;

typedef struct ReplicaNode {
    int id;
    int socketfd;
    int listener_port;
    struct sockaddr_in device_address;
    struct ReplicaNode *next;
} ReplicaNode;


enum ElectionMsgType { MSG_ELECTION, MSG_ELECTION_ANSWER, MSG_COORDINATOR};
typedef struct ElectionEvent {
    enum ElectionMsgType type;
    int id;
    struct sockaddr_in device_address;
} ElectionEvent;

typedef struct {
    int id;
    char hostname[256];
    int port;
    int has_listener; 
    int listener_port;       
} BackupArgs;

typedef struct { 
    int id;
    int port;
    int socketfd;
    int has_listener;
} ManagerArgs;

typedef struct { 
    int election_socket;
    int id;
} ElectionArgs;

#define MAX_USERNAME_LENGTH 32
#define USER_FILES_FOLDER "user files"

extern HashTable contextTable;
extern char my_server_ip[256];

enum ServerMode{ UNKNOWN_MODE, BACKUP, BACKUP_MANAGER };

extern atomic_int global_server_mode;
extern atomic_int global_shutdown_flag;
extern atomic_int is_election_running;
extern atomic_int new_leader_decided;
extern pthread_mutex_t mode_change_mutex;

void *manage_replicas(void *args);
void *run_as_backup(void *arg);
void initialize_user_session_and_threads(struct sockaddr_in device_address, int sock_interface, int sock_receive, int sock_send, char *username);
void handle_incoming_file(Session *session, int receive_socket, const char *folder_path);
char *get_user_folder(const char *username);
int has_data(int socketfd, int timeout_ms);

#endif 