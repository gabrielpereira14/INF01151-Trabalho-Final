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


#include "../util/communication.h"
#include "../util/connectionManagement.h"
#include "../util/contextHashTable.h"
#include "../util/fileSync.h"
#include "./replica.h"

#define MAX_USERNAME_LENGTH 32
#define USER_FILES_FOLDER "user files"

void *manage_replicas(void *args);
void *run_as_backup(void *arg);
void initialize_user_session_and_threads(struct sockaddr_in device_address, int sock_interface, int sock_receive, int sock_send, char *username);
void handle_incoming_file(Session *session, int receive_socket, const char *folder_path);

#endif 