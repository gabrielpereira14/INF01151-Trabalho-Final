#include "./replica.h"
#include "serverCommon.h"
#include <stdatomic.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdio.h>

atomic_int replica_count = 0;

static ReplicaNode *head = NULL;
static pthread_mutex_t replica_list_mutex;

static ReplicaNode *create_new_node(int socketfd, int id, int listener_port, struct sockaddr_in device_address) {
    ReplicaNode *new_node = (ReplicaNode *)malloc(sizeof(ReplicaNode));
    if (new_node == NULL) {
        perror("Failed to allocate memory for new ReplicaNode");
        return NULL;
    }
    new_node->id = id;
    new_node->socketfd = socketfd;
    new_node->listener_port = listener_port;
    new_node->device_address = device_address;
    new_node->next = NULL;
    return new_node;
}

static ReplicaNode *find_node(int socketfd) {
    ReplicaNode *current = head;
    while (current != NULL) {
        if (current->socketfd == socketfd) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


void init_replica_list_mutex() {
    pthread_mutex_init(&replica_list_mutex, NULL);
}

void destroy_replica_list_mutex() {
    pthread_mutex_destroy(&replica_list_mutex);
}

int add_replica(int socketfd, int id, int listener_port, struct sockaddr_in device_address) {
    pthread_mutex_lock(&replica_list_mutex);
    if (find_node(socketfd) != NULL) {
        fprintf(stderr, "Warning: Replica with socketfd %d already exists in the list.\n", socketfd);
        pthread_mutex_unlock(&replica_list_mutex);
        return 0;
    }

    ReplicaNode *newNode = create_new_node(socketfd, id, listener_port ,device_address);
    if (newNode == NULL) {
        pthread_mutex_unlock(&replica_list_mutex); 
        return 0;
    }
    newNode->next = head;
    head = newNode;
    pthread_mutex_unlock(&replica_list_mutex);

    atomic_fetch_add(&replica_count, 1);
    fprintf(stderr, "Replica %d successfully added to list\n", id);
    return 1;
}

int replica_list_contains(int socketfd) {
    pthread_mutex_lock(&replica_list_mutex);
    int contains = find_node(socketfd) != NULL;
    pthread_mutex_unlock(&replica_list_mutex);
    return contains;
}

ReplicaNode* replica_list_remove_node(ReplicaNode **head_ptr, ReplicaNode *prev, ReplicaNode *node_to_remove) {
    if (node_to_remove == NULL) {
        return NULL;
    }

    if (prev == NULL) {
        *head_ptr = node_to_remove->next;
    } else {
        prev->next = node_to_remove->next;
    }

    close(node_to_remove->socketfd); 
    free(node_to_remove);

    atomic_fetch_add(&replica_count, -1);
    return (prev == NULL) ? *head_ptr : prev->next;
}

void replica_list_print_all() {
    pthread_mutex_lock(&replica_list_mutex); 
    printf("Replica List: [");
    ReplicaNode *current = head;
    while (current != NULL) {
        printf("%d - %d", current->id, current->socketfd);
        if (current->next != NULL) {
            printf(", ");
        }
        current = current->next;
    }
    printf("]\n");
    pthread_mutex_unlock(&replica_list_mutex); 
}

int remove_replica_by_id(int id) {
    pthread_mutex_lock(&replica_list_mutex);

    ReplicaNode *current = head;
    ReplicaNode *prev = NULL;
    int removed = 0;

    while (current != NULL) {
        if (current->id == id) {
            replica_list_remove_node(&head, prev, current);
            fprintf(stderr, "Replica %d successfully removed from list.\n", id);
            removed = 1;
            break; // Node removed, exit loop
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&replica_list_mutex);

    if (!removed) {
        fprintf(stderr, "Warning: Replica with ID %d not found in the list.\n", id);
    }
    return removed;
}

void replica_list_destroy() {
    pthread_mutex_lock(&replica_list_mutex); 
    ReplicaNode *current = head;
    ReplicaNode *next;
    while (current != NULL) {
        next = current->next;
        if (current->socketfd > 0)
        {
            close(current->socketfd);
        }
        free(current);
        current = next;
    }
    head = NULL;
    atomic_store(&replica_count, 0);
    printf("Replica list destroyed.\n");
    pthread_mutex_unlock(&replica_list_mutex); 
}

int send_event(ReplicaEvent event, int socketfd){
    char *serialized_event = serialize_replica_event(event);

    Packet *packet = create_packet(PACKET_REPLICA_MSG, strlen(serialized_event), serialized_event);
    int return_code = send_packet(socketfd, packet);

    switch (event.type) {
        case EVENT_FILE_UPLOADED:
            send_file(socketfd, event.filepath, get_user_folder(event.username));
            break;
        case EVENT_FILE_DELETED: {
            Packet *delete_packet = create_packet(PACKET_DELETE, strlen(event.filepath), event.filepath);
            send_packet(socketfd, delete_packet);
            free(delete_packet);
        }
        default:
            break;
    }
        
    free(packet);
    free(serialized_event);
    return return_code;
}

void send_replicas_data_to_new_replica(int new_replica_sockfd){
    ReplicaNode *current = head;

     while (current != NULL) {
        ReplicaEvent event = create_replica_added_event( current->id, current->listener_port, current->device_address);
        send_event(event, new_replica_sockfd);
        free_event(event);
        current = current->next;
    }
    pthread_mutex_unlock(&replica_list_mutex); 
}

int notify_replicas(ReplicaEvent event){
    int notified_replicas = 0;

    if (event.type != EVENT_HEARTBEAT){
        fprintf(stderr, "Notifying replicas...\n");
    }
    
    pthread_mutex_lock(&replica_list_mutex);
    ReplicaNode *current = head;
    ReplicaNode *prev = NULL;
     while (current != NULL) {
        int return_code = send_event(event, current->socketfd);
        if (return_code == OK){
            notified_replicas++;
            prev = current;
            current = current->next;
        }else {
            fprintf(stderr, "Error sending packet to replica %d (socketfd: %d).\n", current->id, current->socketfd);
            if (return_code == SOCKET_CLOSED) {
                fprintf(stderr, "Replica %d closed the connection.\n", current->id);
                current = replica_list_remove_node(&head, prev, current);
            } else {
                prev = current;
                current = current->next;
            }
        }
        
    }
    pthread_mutex_unlock(&replica_list_mutex); 

    if (event.type != EVENT_HEARTBEAT){
        fprintf(stderr, "%d replicas notified\n", notified_replicas);
    }
    
    return notified_replicas;
}


void free_event(ReplicaEvent event){
    if (event.username != NULL) {
        free(event.username);
        event.username = NULL;
    }

    if (event.filepath != NULL) {
        free(event.filepath);
        event.filepath = NULL;
    }
}


ReplicaEvent create_client_connected_event(char *username, struct sockaddr_in device_address){
    ReplicaEvent event;
    event.type = EVENT_CLIENT_CONNECTED;
    event.username = strdup(username);
    event.device_address = device_address;
    event.filepath = NULL;

    return event;
}

ReplicaEvent create_client_disconnected_event(char *username, struct sockaddr_in device_address){
    ReplicaEvent event;
    event.type = EVENT_CLIENT_DISCONNECTED;
    event.username = strdup(username);
    event.device_address = device_address;
    event.filepath = NULL;

    return event;
}

ReplicaEvent create_replica_added_event(int id, int listener_port, struct sockaddr_in device_address){
    ReplicaEvent event;
    event.type = EVENT_REPLICA_ADDED;
    int needed = snprintf(NULL, 0, "%d:%d", id, listener_port) + 1;
    event.username = malloc(needed);
    sprintf(event.username, "%d:%d", id, listener_port);
    event.device_address = device_address;
    event.filepath = NULL;

    return event;
}

ReplicaEvent create_file_upload_event(char *username, struct sockaddr_in device_address, char *filepath){
    ReplicaEvent event;
    event.type = EVENT_FILE_UPLOADED;
    event.username = strdup(username);
    event.device_address = device_address;
    event.filepath = strdup(filepath);

    return event;
}

ReplicaEvent create_file_delete_event(char *username, struct sockaddr_in device_address, char *filepath){
    ReplicaEvent event;
    event.type = EVENT_FILE_DELETED;
    event.username = strdup(username);
    event.device_address = device_address;
    event.filepath = strdup(filepath);

    return event;
}

ReplicaEvent create_heartbeat_event(){
    ReplicaEvent event;
    event.type = EVENT_HEARTBEAT;
    event.username = NULL;
    event.filepath = NULL;
     
    struct sockaddr_in empty_struct = {0};
    event.device_address = empty_struct;

    return event;
}

char* serialize_replica_event(const ReplicaEvent event) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(event.device_address.sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(event.device_address.sin_port);

    const char *username_str = (event.username != NULL) ? event.username : "";
    const char *filepath_str = (event.filepath != NULL) ? event.filepath : "";

    size_t needed = snprintf(NULL, 0, "%d|%s:%d|%s|%s",
                             event.type, ip, port, username_str, filepath_str);

    char *result = malloc(needed + 1);
    if (!result) return NULL;

    sprintf(result, "%d|%s:%d|%s|%s",
            event.type, ip, port, username_str, filepath_str);

    return result;
}

ReplicaEvent deserialize_replica_event(const char *str) {
    ReplicaEvent event;
    memset(&event, 0, sizeof(ReplicaEvent)); 

    char *copy = strdup(str);

    char *token;
    char *saveptr; 
    
    // 1. Event type
    token = strtok_r(copy, "|", &saveptr);
    if (token) {
        event.type = (enum ReplicaEventType) atoi(token);
    } else {
        free(copy);
        return event; 
    }

    // 2. Device Address (IP:Port)
    token = strtok_r(NULL, "|", &saveptr);
    if (token) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            char *ip = token;
            int port = atoi(colon + 1);

            event.device_address.sin_family = AF_INET;
            inet_pton(AF_INET, ip, &(event.device_address.sin_addr));
            event.device_address.sin_port = htons(port);
        } 
    } else {
        free(copy);
        return event;
    }

    // 3. Username
    token = strtok_r(NULL, "|", &saveptr);
    if (token) {
        if (strcmp(token, "") != 0) {
            event.username = strdup(token);
        } else {
            event.username = NULL; 
        }
    } else {
        event.username = NULL;
    }

    // 4. Filepath
    token = strtok_r(NULL, "|", &saveptr);
    if (token) {
        if (strcmp(token, "") != 0) {
            event.filepath = strdup(token);
        } else {
            event.filepath = NULL;
        }
    } else {
        event.filepath = NULL;
    }

    free(copy); 
    return event;
}

ReplicaNode* get_replica_list_head() {
    return head;
}

pthread_mutex_t* get_replica_list_mutex() {
    return &replica_list_mutex;
}
