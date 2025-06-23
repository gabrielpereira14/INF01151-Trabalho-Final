#include "./election.h"
#include "./replica.h"
#include "./serverCommon.h"
#include "serverRoles.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

static int election_answer_received = 0;
static pthread_mutex_t answer_mutex = PTHREAD_MUTEX_INITIALIZER;

void wait_for_barrier(){
    int rc = pthread_barrier_wait(&barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
         fprintf(stderr, "Passou a barreira");
    } else if (rc != 0) {
        fprintf(stderr, "Error waiting at barrier: %d\n", rc);
    }
}

char* serialize_election_event(const ElectionEvent *event) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(event->device_address.sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(event->device_address.sin_port);

    size_t needed = snprintf(NULL, 0, "%d|%s:%d|%d",
                             event->type, ip, port, event->id);

    char *result = malloc(needed + 1);
    if (!result) return NULL;

    sprintf(result, "%d|%s:%d|%d",
            event->type, ip, port, event->id);

    return result;
}


ElectionEvent deserialize_election_event(const char *str) {
    ElectionEvent event;
    memset(&event, 0, sizeof(ElectionEvent)); 

    if (!str)
    {
        ElectionEvent dummy_event = { 0 };
        return dummy_event;
    }

    char *copy = strdup(str);

    char *token;
    char *saveptr; 
    
    // 1. Event type
    token = strtok_r(copy, "|", &saveptr);
    if (token) {
        event.type = (enum ElectionMsgType) atoi(token);
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

    // 3. Id
    token = strtok_r(NULL, "|", &saveptr); 
    if (token) {
        event.id = (int) atoi(token);
    } else {
        free(copy);
        return event;
    }
    free(copy); 
    return event;
}

int send_election_event(ElectionEvent* event, int socketfd){
    char *serialized_event = serialize_election_event(event);

    fprintf(stderr, "\nENVIANDO %d \n\n", event->type);

    Packet *packet = create_packet(PACKET_REPLICA_MSG, strlen(serialized_event), serialized_event);
    int return_code = send_packet(socketfd, packet);
        
    free(serialized_event);
    return return_code;
}

void send_coordinator_msg(int my_id, struct sockaddr_in new_manager_address){
    ReplicaNode* current_replicas = get_replica_list_head();
    ElectionEvent coord_event;
    create_coordinator_event(&coord_event, my_id, new_manager_address);

    pthread_mutex_lock(get_replica_list_mutex()); // Trava a lista pra uso
    for (ReplicaNode *node = current_replicas; node != NULL; node = node->next) {
        fprintf(stderr, "[Servidor %d] Enviando COORDINATOR para %d\n", my_id, node->id);
        send_election_event(&coord_event, node->socketfd);
    }
    pthread_mutex_unlock(get_replica_list_mutex());
}

int send_election_msg(int my_id){
    ReplicaNode* current_replicas = get_replica_list_head();
    int sent_to_higher_id = 0;

    pthread_mutex_lock(get_replica_list_mutex()); // Trava a lista pra uso
    for (ReplicaNode *node = current_replicas; node != NULL; node = node->next) {
        if (node->id > my_id) {
            fprintf(stderr, "[Servidor %d] Enviando ELECTION para %d\n", my_id, node->id);
            ElectionEvent event;
            create_election_event(&event, my_id);
            send_election_event(&event, node->socketfd);
            sent_to_higher_id = 1;
        }
    }
    pthread_mutex_unlock(get_replica_list_mutex());

    return sent_to_higher_id;
}

void *run_election(void *arg) {
    int my_id = *(int *) arg;
    fprintf(stderr, "[Servidor %d] Iniciando eleição...\n", my_id);
    // Reseta o estado de resposta

    ReplicaNode* current_replicas = get_replica_list_head();
    int sent_to_higher_id = send_election_msg(my_id);

    if (!sent_to_higher_id) {
        fprintf(stderr, "[Servidor %d] Nenhum ID maior. Venci a eleição!\n", my_id);
        atomic_store(&global_server_mode, BACKUP_MANAGER);
        atomic_store(&is_election_running, 0);
        atomic_store(&new_leader_decided, 1);
        pthread_exit(NULL);
    }

    sleep(3); // Timeout


    pthread_mutex_lock(&answer_mutex);
    int status = election_answer_received;
    election_answer_received = 0;
    pthread_mutex_unlock(&answer_mutex);

    if (status) {
        fprintf(stderr, "[Servidor %d] Recebi ANSWER. Perdi a eleição.\n", my_id);
    } else {
        fprintf(stderr, "[Servidor %d] Timeout, ninguém respondeu. Venci a eleição!\n", my_id);
        atomic_store(&global_server_mode, BACKUP_MANAGER);
        atomic_store(&new_leader_decided, 1);
    }

    atomic_store(&is_election_running, 0);
    fprintf(stderr, "[Servidor %d] Eleição finalizada...\n", my_id);
    pthread_exit(NULL);
}


ElectionEvent *create_election_event(ElectionEvent *event, int sender_id) {
    event->type = MSG_ELECTION;
    event->id = sender_id;
    memset(&event->device_address, 0, sizeof(event->device_address));
    return event;
}

ElectionEvent *create_election_answer_event(ElectionEvent *event, int sender_id) {
    event->type = MSG_ELECTION_ANSWER;
    event->id = sender_id;
    memset(&event->device_address, 0, sizeof(event->device_address));
    return event;
}

ElectionEvent *create_coordinator_event(ElectionEvent *event, int leader_id, struct sockaddr_in leader_address) {
    event->type = MSG_COORDINATOR;
    event->id = leader_id;
    event->device_address = leader_address;
    return event;
}


void *election_listener_thread(void *arg){
    ElectionArgs election_args = *(ElectionArgs *) arg;
    int election_socket = election_args.election_socket;
    int id = election_args.id;

    while (!atomic_load(&global_shutdown_flag)) {

        int result = has_data(election_socket, 2);

        if (result > 0) {
            Packet *packet = read_packet(election_socket);

            if (packet->type == PACKET_CONNECTION_CLOSED)
            {
                break;
            }

            print_packet(packet);
            ElectionEvent event = deserialize_election_event(packet->payload);
            switch (event.type) { 
            case MSG_ELECTION: {
                int sender_id = event.id;
                fprintf(stderr, "[Servidor %d] Recebeu ELECTION de %d.\n", id, sender_id);

                ElectionEvent answer_event;
                create_election_answer_event(&answer_event, id);
                send_election_event(&answer_event, election_socket);

                if(sender_id >= id){
                    fprintf(stderr, "ERRO: Recebeu um EVENT_ELECTION de um ip maior ou igual\n");
                }

                if (!atomic_load(&is_election_running)) {

                    atomic_store(&is_election_running, 1);
                    // Inicia sua própria eleição em uma nova thread
                    pthread_t election_thread;
                    int* my_id_ptr = malloc(sizeof(int));
                    *my_id_ptr = id;
                    pthread_create(&election_thread, NULL, run_election, my_id_ptr);
                    pthread_detach(election_thread);

                }
                break;
            }
            case MSG_ELECTION_ANSWER: {
                pthread_mutex_lock(&answer_mutex);
                election_answer_received = 1;
                pthread_mutex_unlock(&answer_mutex);
                int sender_id = event.id;
                fprintf(stderr, "[Servidor %d] Recebeu ANSWER de %d.\n", id, sender_id);
                break;
            }

            case MSG_COORDINATOR: {
                pthread_mutex_lock(&answer_mutex);
                election_answer_received = 1;
                pthread_mutex_unlock(&answer_mutex);

                int leader_id = event.id;
                char leader_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(event.device_address.sin_addr), leader_ip, INET_ADDRSTRLEN);
                int leader_port = ntohs(event.device_address.sin_port);

                fprintf(stderr, "[Servidor %d] Anúncio recebido: Novo líder é %d em %s:%d.\n", id, leader_id, leader_ip, leader_port);

                
                atomic_store(&new_leader_decided, 1);
                pthread_t new_backup_thread;
                BackupArgs *args = (BackupArgs *)malloc(sizeof(BackupArgs)); 
                args->id = id;
                strncpy(args->hostname, leader_ip, sizeof(leader_ip) - 1);
                args->hostname[sizeof(args->hostname) - 1] = '\0';
                args->port = leader_port;
                args->has_listener = 1;

                
                pthread_create(&new_backup_thread, NULL, run_as_backup, (void*) args);
                pthread_detach(new_backup_thread);
                break;
            }
            default:
                fprintf(stderr, "[Election thread] Invalid Event");
                break;
            }

        } else if (result == 0) {
            // timeout
            continue;
        }
    }

    pthread_exit(NULL);
}