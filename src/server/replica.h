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


#include "../util/communication.h"
#include "../util/connectionManagement.h"
#include "../util/fileSync.h"
#include "../server/serverCommon.h"

enum EventTypes { 
    EVENT_CLIENT_CONNECTED,
    EVENT_CLIENT_DISCONNECTED,
    EVENT_FILE_UPLOADED,
    EVENT_REPLICA_ADDED,
    EVENT_HEARTBEAT,
    EVENT_FILE_DELETED,
    EVENT_ELECTION,
    EVENT_ELECTION_ANSWER,
    EVENT_COORDINATOR
};

typedef struct replicaEvent {
    enum EventTypes type;
    struct sockaddr_in device_address;
    char *username;
    char *filename;
} ReplicaEvent;

typedef struct ReplicaNode {
    int id;
    int socketfd;
    struct sockaddr_in device_address;
    struct ReplicaNode *next;
} ReplicaNode;

extern int  current_manager;

int notify_replicas(ReplicaEvent event);
void listen_for_replicas(int port);
int add_replica(int socketfd, int id, struct sockaddr_in device_address) ;
void connect_to_manager(struct sockaddr_in server_address);

ReplicaEvent create_client_connected_event(char *username, struct sockaddr_in device_address);
ReplicaEvent create_client_disconnected_event(char *username, struct sockaddr_in device_address);
ReplicaEvent create_replica_added_event(int id, struct sockaddr_in device_address);
ReplicaEvent create_heartbeat_event();
ReplicaEvent create_file_upload_event(char *username, struct sockaddr_in device_address, char *filename);
ReplicaEvent create_file_delete_event(char *username, struct sockaddr_in device_address, char *filename);

char* serialize_replica_event(const ReplicaEvent event);
ReplicaEvent deserialize_replica_event(const char *str);

void free_event(ReplicaEvent event);

ReplicaEvent create_election_event(int sender_id);
ReplicaEvent create_election_answer_event(int sender_id);
ReplicaEvent create_coordinator_event(int leader_id, struct sockaddr_in leader_address);

#endif
