#ifndef REPLICA_HEADER
#define REPLICA_HEADER

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "./serverCommon.h"
#include "../util/communication.h"
#include "../util/connectionManagement.h"
#include "../util/fileSync.h"



extern int  current_manager;

int notify_replicas(ReplicaEvent event);
void listen_for_replicas(int port);
int add_replica(int socketfd, int id, int listener_port, struct sockaddr_in device_address);
void replica_list_destroy();

void connect_to_manager(struct sockaddr_in server_address);

ReplicaEvent create_client_connected_event(char *username, struct sockaddr_in device_address);
ReplicaEvent create_client_disconnected_event(char *username, struct sockaddr_in device_address);
ReplicaEvent create_replica_added_event(int id, int listener_port, struct sockaddr_in device_address);
ReplicaEvent create_heartbeat_event();
ReplicaEvent create_file_upload_event(char *username, struct sockaddr_in device_address, char *filename);
ReplicaEvent create_file_delete_event(char *username, struct sockaddr_in device_address, char *filename);

char* serialize_replica_event(const ReplicaEvent event);
ReplicaEvent deserialize_replica_event(const char *str);

void free_event(ReplicaEvent event);
int send_event(ReplicaEvent event, int socketfd);
ReplicaNode* get_replica_list_head();
pthread_mutex_t* get_replica_list_mutex();

void send_replicas_data_to_new_replica(int new_replica_sockfd);

#endif
