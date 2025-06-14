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
} BackupArgs;

typedef struct { 
    int id;
    int port;
} ManagerArgs;



extern volatile enum ServerMode global_server_mode;
extern volatile int global_shutdown_flag;
extern pthread_mutex_t mode_change_mutex; 



void *replica_listener_thread(void *arg);
void *heartbeat_monitor_thread_main(void *arg); // New for backup role


void *manage_replicas(void *args);
void *run_as_backup(void *arg);


#endif 