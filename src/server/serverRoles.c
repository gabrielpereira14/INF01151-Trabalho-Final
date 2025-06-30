#include <netinet/in.h>
#define _DEFAULT_SOURCE // Concerta o aviso chato sobre o h_addr do hostent
#include "./serverRoles.h"
#include "./serverCommon.h"
#include "replica.h"
#include "election.h"
#include <bits/pthreadtypes.h>
#include <time.h>
#include <stdatomic.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>


#define HEARTBEAT_TIMEOUT_SECONDS 5


atomic_int global_server_mode = UNKNOWN_MODE;
atomic_int global_shutdown_flag = 0;
time_t last_heartbeat;

pthread_mutex_t mode_change_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t heartbeat_mutex = PTHREAD_MUTEX_INITIALIZER;

atomic_int is_election_running = 0;
atomic_int new_leader_decided = 0;
int should_start_connection = 0;

void send_heartbeat_to_replicas() {
    ReplicaEvent event = create_heartbeat_event();
    notify_replicas(event);
    free_event(event);
    //fprintf(stderr, "Heartbeat to %d replicas!\n", notified_replicas);
}

void heartbeat_received(){
    pthread_mutex_lock(&heartbeat_mutex);
    last_heartbeat = time(NULL);
    pthread_mutex_unlock(&heartbeat_mutex);
}

int is_manager_running() {
    pthread_mutex_lock(&heartbeat_mutex);
    int time_diff = difftime(time(NULL), last_heartbeat);
    pthread_mutex_unlock(&heartbeat_mutex);

    if (time_diff > HEARTBEAT_TIMEOUT_SECONDS || atomic_load(&new_leader_decided) || atomic_load(&is_election_running)) 
        return 0;
    return 1;
}

void *replica_listener_thread(void *arg) {
    ManagerArgs *manager_args = (ManagerArgs *) arg;
    int port = manager_args->port;

    int listen_sockfd = manager_args->socketfd;
    int newsockfd;

    struct sockaddr_in cli_addr;
    socklen_t clilen;

    if (listen(listen_sockfd, 5) == -1) { 
        perror("[Listener Thread] ERROR on listen");
        close(listen_sockfd);
        free(manager_args);
        pthread_exit(NULL); 
    }

    printf("[Listener Thread] Listening for replica connections on port %d...\n", port);

    while (1) {
        pthread_mutex_lock(&mode_change_mutex);
        int should_exit = atomic_load(&global_shutdown_flag);
        pthread_mutex_unlock(&mode_change_mutex);

        if (should_exit) {
            printf("[Listener Thread] Exiting loop due global shutdown.\n");
            break; 
        }
        int replica_connected = has_data(listen_sockfd, 1000);
        
        if ( replica_connected > 0){
            clilen = sizeof(cli_addr);
            newsockfd = accept(listen_sockfd, (struct sockaddr *)&cli_addr, &clilen);
            if (newsockfd == -1) {
                if (errno == EINTR) {
                    // Interrupted by a signal, retry accept
                    continue;
                }
                perror("[Listener Thread] ERROR on accept");
                break;
            }

            
            if ((atomic_load(&global_server_mode) == BACKUP_MANAGER))
            {
                Packet *packet = read_packet(newsockfd);
                //print_packet(packet);
                if (packet->type == PACKET_CONNECTION_CLOSED){
                    continue;
                }

                int replica_id, replica_listener_port;
                if (sscanf(packet->payload, "%d:%d", &replica_id, &replica_listener_port) != 2) {
                    fprintf(stderr, "ERROR parsing ID and port");
                }

                fprintf(stderr,"Parsed ID = %d, Port = %d\n", replica_id, replica_listener_port);

                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(cli_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                fprintf(stderr, "[Listener Thread] Accepted new replica connection from %s:%hu (fd %d) - Id %d\n",
                    ip_str, ntohs(cli_addr.sin_port), newsockfd, replica_id);

                if(!add_replica(newsockfd, replica_id, replica_listener_port, cli_addr)){
                    fprintf(stderr, "[Listener Thread] Error adding replica");
                    continue;
                }

                ReplicaEvent event = create_replica_added_event(replica_id, replica_listener_port, cli_addr);
                notify_replicas(event);
                free_event(event);

                send_replicas_data_to_new_replica(newsockfd);
            }else if ((atomic_load(&global_server_mode) == BACKUP)){

                // inicia a thread da eleicao (esccuta msg quem inicia eleicao é a do heartbeat)
                pthread_t election_thread;

                fprintf(stderr, "Replica conectada na thread de eleicao\n");
                ElectionArgs *args = malloc(sizeof(ElectionArgs));
                args->election_socket = newsockfd;
                args->id = manager_args->id;

                pthread_create(&election_thread, NULL, election_listener_thread, (void *) args);
                pthread_detach(election_thread);
            }
            

        } else if (replica_connected < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("[Manager Listener Thread] Select error");
            break; 
        } else if (replica_connected == 0) {
            // Timeout occurred, no new connections, loop back to check flags again
            continue;
        }
    }

    pthread_exit(NULL);
}

void *heartbeat_monitor_thread_main(void *arg) {
    int id = *(int*)arg;
    printf("[Backup Thread] Heartbeat monitor started for Backup ID %d.\n", id);

    while (1) {
        pthread_mutex_lock(&mode_change_mutex);
        int should_exit = atomic_load(&global_shutdown_flag) || (atomic_load(&global_server_mode) != BACKUP) ;
        pthread_mutex_unlock(&mode_change_mutex);

        if (should_exit) {
            printf("[Backup Thread] Heartbeat monitor exiting due to mode change or shutdown.\n");
            break;
        }

        if (!is_manager_running()) {
            fprintf(stderr, "[Backup %d Connection Thread] No heartbeat received in the last %d seconds. Connection might be dead.\n", 
                id, HEARTBEAT_TIMEOUT_SECONDS);
            pthread_mutex_lock(&mode_change_mutex);
            if (atomic_load(&global_server_mode) == BACKUP) { 
                printf("[Backup %d] Manager heartbeats stopped. Initiating failover.\n", id);
                atomic_store(&is_election_running, 1);
                // Inicia sua própria eleição em uma nova thread
                pthread_t election_thread;
                int* my_id_ptr = malloc(sizeof(int));
                *my_id_ptr = id;
                pthread_create(&election_thread, NULL, run_election, my_id_ptr);
                pthread_join(election_thread, NULL);
            }
            pthread_mutex_unlock(&mode_change_mutex);
        }

        sleep(1); 
    }
    pthread_exit(NULL);
}


void *connect_to_server_thread(void *arg) {
    BackupArgs *backup_args = (BackupArgs *)arg;
    int id = backup_args->id;
    const char *hostname = backup_args->hostname;
    int port = backup_args->port;

    int has_connection_closed = 0; 

    
    printf("BackupArgs Details:\n");
    printf("  ID: %d\n", backup_args->id);
    printf("  Hostname: %s\n", backup_args->hostname);
    printf("  Port: %d\n", backup_args->port);
    printf("  Has Listener: %s\n", backup_args->has_listener ? "Yes" : "No");
    printf("  Listener Port: %d\n", backup_args->listener_port);

    int socketfd = -1;
    printf("[Backup %d Connection Thread] Attempting to connect to %s:%d.\n", id, hostname, port);

    while (1) {
        pthread_mutex_lock(&mode_change_mutex);
        int should_exit = atomic_load(&global_shutdown_flag) || (atomic_load(&global_server_mode) != BACKUP) || has_connection_closed;
        pthread_mutex_unlock(&mode_change_mutex);

        if (should_exit) {
            printf("[Backup %d Connection Thread] Exiting due to mode change or global shutdown.\n", id);
            break; // Exit the connection/communication loop
        }

        struct hostent *server;
        if ((server = gethostbyname(hostname)) == NULL) {
            fprintf(stderr,"ERRO servidor nao encontrado hostname: %s\n", hostname);
            exit(1);
        }

        if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            fprintf(stderr, "ERRO abrindo o socket da interface\n");
            continue;
        }

        struct sockaddr_in sockaddr;
        memset(&sockaddr, 0, sizeof(sockaddr));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        memcpy(&sockaddr.sin_addr, server->h_addr, server->h_length);

        if (connect(socketfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
            fprintf(stderr,"ERRO conectando ao servidor\n");
            close(socketfd);
            socketfd = -1;
            sleep(1);
            continue;
        }

        printf("[Backup %d Connection Thread] Successfully connected to %s:%d (socket fd: %d).\n", id, hostname, port, socketfd);

        int listener_port = backup_args->listener_port;
        int needed = snprintf(NULL, 0, "%d:%d", id, listener_port) + 1;
        char *id_string = malloc(needed);
        sprintf(id_string, "%d:%d", id, listener_port);

        Packet *packet = create_packet(PACKET_REPLICA_MSG, strlen(id_string), id_string);
        send_packet(socketfd, packet);

        while (1) {
            if (atomic_load(&is_election_running))
            {
                continue;
            }

            pthread_mutex_lock(&mode_change_mutex);
            int comm_should_exit = atomic_load(&global_shutdown_flag) || atomic_load(&global_server_mode) != BACKUP || has_connection_closed;
            pthread_mutex_unlock(&mode_change_mutex);

            if (comm_should_exit) {
                printf("[Backup %d Connection Thread] Communication loop exiting due to mode change or shutdown.\n", id);
                break; // Exit inner communication loop
            }

            if (has_data(socketfd, 1000) > 0) {
                Packet *packet = read_packet(socketfd);
                switch (packet->type) {
                    case PACKET_REPLICA_MSG:{
                        ReplicaEvent event = deserialize_replica_event(packet->payload);
                        if (event.type != EVENT_HEARTBEAT){
                            print_packet(packet);
                        }

                        switch (event.type) {
                        case EVENT_CLIENT_CONNECTED:
                            initialize_user_session_and_threads(event.device_address, -1, -1, -1, event.username);
                            break;
                        case EVENT_CLIENT_DISCONNECTED:{
                            UserContext *context = get_or_create_context(&contextTable, event.username);
                            pthread_mutex_lock(&context->lock);
                            Session *session = get_user_session_by_address(context, &event.device_address);
                            session->user_context->sessions[session->session_index] = NULL;
                            pthread_mutex_unlock(&context->lock);
                            fprintf(stderr, "Usuário %s desconectou.\n", event.username);
                        }
                            
                            break;
                        case EVENT_FILE_DELETED:
                        case EVENT_FILE_UPLOADED:{
                            UserContext *context = get_or_create_context(&contextTable, event.username);
                            pthread_mutex_lock(&context->lock);
                            Session *session = get_user_session_by_address(context, &event.device_address);
                            pthread_mutex_unlock(&context->lock);
                            char *user_folder_path = get_user_folder(event.username);
                            handle_incoming_file(session, socketfd, user_folder_path);
                            free(user_folder_path);
                            break;
                        }
                                
                        case EVENT_REPLICA_ADDED:{
                            int replica_id, replica_listener_port;
                            if (sscanf( event.username, "%d:%d", &replica_id, &replica_listener_port) != 2) {
                                fprintf(stderr, "ERROR parsing replica_id and replica_listener_port");
                                continue;
                            }
                            //fprintf(stderr,"Parsed ID = %d, Port = %d\n", replica_id, replica_listener_port);

                            if (replica_id != id){

                                event.device_address.sin_port = htons(replica_listener_port);

                                char ip_str[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &(event.device_address.sin_addr), ip_str, INET_ADDRSTRLEN);
                                int port = ntohs(event.device_address.sin_port);
                                int new_replica_socketfd;
                                printf("IP: %s, Port: %d\n", ip_str, port);

                                if ((new_replica_socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                                    perror("ERRO criando socket");
                                    close(new_replica_socketfd);
                                    continue;
                                }

                                if (connect(new_replica_socketfd, (struct sockaddr *) &event.device_address,sizeof(event.device_address)) < 0) {
                                    perror("ERRO conectando ao servidor");
                                    continue;
                                }
                                add_replica(new_replica_socketfd, replica_id, replica_listener_port, event.device_address);
                            } 
                                
                            break;
                        }
                        case EVENT_HEARTBEAT:
                            heartbeat_received();
                            break;
                        }
                        break;
                    }
                    case PACKET_CONNECTION_CLOSED:{
                        fprintf(stderr, "[Backup %d Connection Thread] Connection closed.\n",id);
                        has_connection_closed = 1;
                        break;
                    }
                        
                    default :{
                        fprintf(stderr, "[Backup %d Connection Thread] Unsupported packet type: %d\n",id, packet->type);
                        print_packet(packet);
                        break;
                    }
                }
                
            }
            sleep(1);
        }
        printf("[Backup %d Connection Thread] Disconnected from manager or communication loop exited. Closing socket %d.\n", id, socketfd);
        
    }
    close(socketfd);

    printf("[Backup %d Connection Thread] Thread fully exiting.\n", id);
    pthread_exit(NULL);
}

void bind_listener_socket(ManagerArgs *manager_args){
    struct sockaddr_in sockaddr;
    int listen_sockfd;
    int port = manager_args->port;

    if ((listen_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Listener Thread] ERROR opening socket");
        free(manager_args);
        pthread_exit(NULL);
    }

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(sockaddr.sin_zero), 8); 

    while (bind(listen_sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0){
        port = (rand() % 30000) + 2000;
        sockaddr.sin_port = htons(port);
    };

    manager_args->port = port;
    manager_args->socketfd = listen_sockfd;
}

void *manage_replicas(void *args) {
    ManagerArgs *manager_args = (ManagerArgs *) args;
    printf("Server ID %d: Running as MANAGER.\n", manager_args->id);
    atomic_store(&global_server_mode,BACKUP_MANAGER);
    last_heartbeat = time(NULL);

    if (atomic_load(&new_leader_decided))
    {
        atomic_store(&new_leader_decided,0);
    }
    
    if(!manager_args->has_listener){
        manager_args->has_listener = 1;
        bind_listener_socket(manager_args);
        pthread_t listener_tid;
        int rc = pthread_create(&listener_tid, NULL, replica_listener_thread, manager_args);
        if (rc != 0) {
            fprintf(stderr, "Error launching replica listener thread: %s\n", strerror(rc));
            free(manager_args);
            exit(EXIT_FAILURE);
        }
        pthread_detach(listener_tid);
    }

    printf("Manager server (ID %d) is performing its main duties...\n", manager_args->id);
    while (1) {
        pthread_mutex_lock(&mode_change_mutex);
        int should_exit_role = atomic_load(&global_shutdown_flag) || (atomic_load(&global_server_mode) != BACKUP_MANAGER);
        pthread_mutex_unlock(&mode_change_mutex);

        if (should_exit_role) {
            printf("[Manager %d] Exiting MANAGER role loop.\n", manager_args->id);
            break;
        } 
        send_heartbeat_to_replicas();
        sleep(2);
    }
    printf("Manager server (ID %d) is cleaning up resources.\n", manager_args->id);
    pthread_exit(NULL);
}

void notify_clients_of_server_change(struct sockaddr_in new_address){
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &new_address.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(new_address.sin_port);

    size_t needed = snprintf(NULL, 0, "%s:%d", ip, port) + 1;

    char *result = malloc(needed);
    if (!result){
        fprintf(stderr, "ERROR enviando notificação pro cliente\n");
        return;
    }

    sprintf(result, "%s:%d", ip, port);

    Packet *packet = create_packet(PACKET_RECONNECT, needed, result);
    send_packet_all_users(&contextTable, packet);
}

void *run_as_backup(void* arg) {
    BackupArgs *backup_args = (BackupArgs *) arg;

    printf("Server ID %d: Running as BACKUP.\n", backup_args->id);
    printf("Manager to connect to: %s:%d\n", backup_args->hostname, backup_args->port);
    atomic_store(&global_server_mode,BACKUP);



    last_heartbeat = time(NULL);

    if (!backup_args->has_listener)
    {
        ManagerArgs *listener_args = malloc(sizeof(ManagerArgs));
        listener_args->id = backup_args->id;
        listener_args->port = backup_args->port + 1;
        listener_args->has_listener = 1;
        listener_args->socketfd = -1;

        bind_listener_socket(listener_args);

        backup_args->listener_port = listener_args->port;

        pthread_t listener_tid;
        if (pthread_create(&listener_tid, NULL, replica_listener_thread, listener_args) != 0) {
            perror("Backup failed to create replica listener thread");
            exit(EXIT_FAILURE);
        }
        pthread_detach(listener_tid);
        printf("[Servidor %d] Backup agora está escutando por conexões de réplicas na porta %d.\n", listener_args->id, listener_args->port);


    }

    printf("Backup server (ID %d) is performing its main duties...\n", backup_args->id);

    printf("BackupArgs Details:\n");
    printf("  ID: %d\n", backup_args->id);
    printf("  Hostname: %s\n", backup_args->hostname);
    printf("  Port: %d\n", backup_args->port);
    printf("  Has Listener: %s\n", backup_args->has_listener ? "Yes" : "No");
    printf("  Listener Port: %d\n", backup_args->listener_port);

    pthread_t manager_conn_tid;
    int rc = pthread_create(&manager_conn_tid, NULL, connect_to_server_thread, backup_args);
    if (rc != 0) {
        fprintf(stderr, "Error launching manager connection thread: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
    pthread_detach(manager_conn_tid); 

    if (!backup_args->has_listener)
    {
        // Launch heartbeat monitor thread (if not already running)
        pthread_t heartbeat_tid;
        int *backup_id_ptr = malloc(sizeof(int));
        if (backup_id_ptr == NULL) {
            perror("Failed to allocate ID for heartbeat thread");
            exit(EXIT_FAILURE);
        }
        *backup_id_ptr = backup_args->id;

        int rc = pthread_create(&heartbeat_tid, NULL, heartbeat_monitor_thread_main, backup_id_ptr);
        if (rc != 0) {
            fprintf(stderr, "Error launching heartbeat monitor thread: %s\n", strerror(rc));
            free(backup_id_ptr);
            exit(EXIT_FAILURE);
        }
        pthread_detach(heartbeat_tid);
    }
    atomic_store(&new_leader_decided, 0);
    while (1) {
        pthread_mutex_lock(&mode_change_mutex);
        int should_exit_role = atomic_load(&global_shutdown_flag) || (atomic_load(&global_server_mode) != BACKUP) || atomic_load(&new_leader_decided);
        pthread_mutex_unlock(&mode_change_mutex);
        
        if (should_exit_role) {
            printf("[Backup %d] Exiting BACKUP role loop.\n", backup_args->id);
            break;
        }
        sleep(1);
    }
    printf("Backup server (ID %d) is cleaning up resources.\n", backup_args->id);
    int users_disconnected = 0;

    if (atomic_load(&global_server_mode) == BACKUP_MANAGER) {
        

        fprintf(stderr, "Trocando de função \n");
        
        ManagerArgs *args = (ManagerArgs *)malloc(sizeof(ManagerArgs));
        args->id = backup_args->id;
        args->port = backup_args->port;
        args->has_listener = 1;

        struct sockaddr_in my_new_manager_address;
        memset(&my_new_manager_address, 0, sizeof(my_new_manager_address));
        my_new_manager_address.sin_family = AF_INET;
        my_new_manager_address.sin_port = htons(backup_args->listener_port);

        struct hostent *server;
        if ((server = gethostbyname(my_server_ip)) == NULL) {
            fprintf(stderr,"ERRO servidor nao encontrado\n");
        }
        memcpy(&my_new_manager_address.sin_addr, server->h_addr, server->h_length);

        //coordinator msg
        send_coordinator_msg(args->id,my_new_manager_address);

        // notificar o cliente

        struct sockaddr_in new_address = my_new_manager_address;
        new_address.sin_port = htons(interface_socket_port);
    
        
        int current_replica_count = atomic_load(&replica_count);
        replica_list_destroy();

        while (current_replica_count != atomic_load(&replica_count)) {
            sleep(1);
            fprintf(stderr, "Esperando replicas reconectarem\n");
        }

        notify_clients_of_server_change(new_address);
        disconnect_all_users(&contextTable); 
        users_disconnected = 1;   

        pthread_t manager_thread;
        pthread_create(&manager_thread, NULL, manage_replicas, (void *) args);
        pthread_detach(manager_thread);
    }

    if (!users_disconnected)
    {
        disconnect_all_users(&contextTable); 
    }

    free(backup_args);
    pthread_exit(NULL);
}
