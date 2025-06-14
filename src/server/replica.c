#include "./replica.h"

int current_manager = -1;

typedef struct ReplicaNode {
    int id;
    int socketfd;
    struct ReplicaNode *next;
} ReplicaNode;

static ReplicaNode *head = NULL;
static pthread_mutex_t replica_list_mutex;

static ReplicaNode *create_new_node(int socketfd, int id) {
    ReplicaNode *newNode = (ReplicaNode *)malloc(sizeof(ReplicaNode));
    if (newNode == NULL) {
        perror("Failed to allocate memory for new ReplicaNode");
        return NULL;
    }
    newNode->id = id;
    newNode->socketfd = socketfd;
    newNode->next = NULL;
    return newNode;
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

int add_replica(int socketfd, int id) {
    pthread_mutex_lock(&replica_list_mutex);
    if (find_node(socketfd) != NULL) {
        fprintf(stderr, "Warning: Replica with socketfd %d already exists in the list.\n", socketfd);
        pthread_mutex_unlock(&replica_list_mutex);
        return 0;
    }

    ReplicaNode *newNode = create_new_node(socketfd, id);
    if (newNode == NULL) {
        pthread_mutex_unlock(&replica_list_mutex); 
        return 0;
    }
    newNode->next = head;
    head = newNode;
    pthread_mutex_unlock(&replica_list_mutex); 
    return 1;
}

int replica_list_contains(int socketfd) {
    pthread_mutex_lock(&replica_list_mutex);
    int contains = find_node(socketfd) != NULL;
    pthread_mutex_unlock(&replica_list_mutex);
    return contains;
}

int replica_list_remove(int socketfd) {
    pthread_mutex_lock(&replica_list_mutex);

    ReplicaNode *current = head;
    ReplicaNode *prev = NULL;

    if (current != NULL && current->socketfd == socketfd) {
        head = current->next;
        free(current);
        pthread_mutex_unlock(&replica_list_mutex);
        return 1;
    }

    while (current != NULL && current->socketfd != socketfd) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        pthread_mutex_unlock(&replica_list_mutex); 
        return 0;
    }

    prev->next = current->next;
    free(current);
    pthread_mutex_unlock(&replica_list_mutex);
    return 1;
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

void replica_list_destroy() {
    pthread_mutex_lock(&replica_list_mutex); 
    ReplicaNode *current = head;
    ReplicaNode *next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
    printf("Replica list destroyed.\n");
    pthread_mutex_unlock(&replica_list_mutex); 
}

int notify_replicas(ReplicaEvent* event){
    char *serialized_event = serialize_replica_event(event);

    int notified_replicas = 0;

    Packet packet = create_control_packet(PACKET_REPLICA_MSG, strlen(serialized_event), serialized_event);

    pthread_mutex_lock(&replica_list_mutex);
    ReplicaNode *current = head;
    while (current != NULL) {
        if (!send_packet(current->socketfd, &packet)) {
            fprintf(stderr, "Error sending packet to replica. (socketfd: %d)\n", current->socketfd);
        }
        notified_replicas++;
        current = current->next;
    }
    pthread_mutex_unlock(&replica_list_mutex); 

    free(serialized_event);

    return notified_replicas;
}


void free_event(ReplicaEvent* event){
    if (event == NULL) return; 

    if (event->username != NULL) {
        free(event->username);
        event->username = NULL;
    }
}


ReplicaEvent *create_client_connected_event(ReplicaEvent *event, Session *session, struct sockaddr_in device_address){

    event->username = strdup(session->user_context->username);
    event->device_address = device_address;
    event->type = EVENT_CLIENT_CONNECTED;

    return event;
}

ReplicaEvent *create_heartbeat_event(ReplicaEvent *event){
    event->type = EVENT_HEARTBEAT;
    event->username = NULL;
     
    struct sockaddr_in empty_struct = {0};
    event->device_address = empty_struct;

    return event;
}

char* serialize_replica_event(const ReplicaEvent *event) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(event->device_address.sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(event->device_address.sin_port);

    size_t needed = snprintf(NULL, 0, "%d|%s:%d|%s", event->type, ip, port, event->username);
    char *result = malloc(needed + 1);
    if (!result) return NULL;

    sprintf(result, "%d|%s:%d|%s", event->type, ip, port, event->username);
    return result;
}

ReplicaEvent deserialize_replica_event(const char *str){
    ReplicaEvent event;
    memset(&event, 0, sizeof(ReplicaEvent)); 

    char *copy = strdup(str);
    if (!copy) return event;

    char *token = strtok(copy, "|");
    if (token) {
        event.type = atoi(token);
    }

    token = strtok(NULL, "|");
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
    }

    token = strtok(NULL, "|");
    if (token) {
        event.username = strdup(token);
    }

    free(copy);
    return event;
}