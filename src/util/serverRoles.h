#ifndef SERVER_ROLES_H
#define SERVER_ROLES_H

#include <pthread.h> 
#include <errno.h>

#include "./replica.h"

enum ServerMode{ UNKNOWN_MODE, BACKUP, BACKUP_MANAGER };


typedef struct {
    int id;
    char hostname[256];
    int port;           
} ConnectionArgs;


extern volatile enum ServerMode global_server_mode;
extern volatile int global_shutdown_flag;
extern pthread_mutex_t mode_change_mutex; 

typedef struct { 
    int port;
} ListenerArgs;


void *replica_listener_thread(void *arg);
void *heartbeat_monitor_thread_main(void *arg); // New for backup role


void manage_replicas(int id, int listen_port);
void run_as_backup(int id, const char *manager_ip, int manager_port);


#endif 