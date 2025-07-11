#include "./connectionManagement.h"
#include "./contextHashTable.h"
#include "communication.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>

uint32_t crc32(const char *filepath) {
    uint8_t buffer[1024];
    uint32_t crc = 0xFFFFFFFF;
    size_t bytesRead;
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "crc32: Error opening file '%s': ", filepath);
        perror(NULL);
        return 0;
    }

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytesRead; i++) {
            crc ^= buffer[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
        }
    }

    fclose(file);
    return crc ^ 0xFFFFFFFF;
}


UserContext *get_or_create_context(HashTable *table, char *username){
    UserContext *context = HashTable_search(table,username);

    if(context == NULL){
        context = HashTable_insert(table, username, create_context(username));
    }

    return context;
}

int find_free_session_index(UserContext *context){
    for(int i = 0; i < MAX_SESSIONS; i++){
        if (is_session_empty(context->sessions[i]) != 0) {
            return i;
            break;
        }
    }
    return -1;
}

int add_file_to_context(HashTable *table, const char *filename, const char *base_path, char *username){
    UserContext *context = HashTable_search(table, username);
    if (context == NULL) {
        return 1;
    }

    char *filepath = create_filepath(base_path, filename);
    context->file_list = FileLinkedList_push(context->file_list, filename, crc32(filepath));

    free(filepath);
    return 0;
}

int remove_file_from_context(HashTable *table, const char *filename, char *username){
    UserContext *context = HashTable_search(table, username);
    if (!context){
        return 1;
    }

    FileNode **prev = &(context->file_list);
    FileNode *head = context->file_list;
    while (head != NULL) {
        if (strcmp(head->key, filename) == 0) {
            *prev = head->next;

            free(head->key);
            free(head);

            return 0;
        }

        prev = &(head->next);
        head = head->next;
    }

    return 1;
}

Session *create_session(int index, UserContext *context, SessionSockets sockets, struct sockaddr_in device_address){
    Session *session = malloc(sizeof(Session));
    
    session->session_index = index;
    session->active = 1;

    session->device_address = device_address;
    session->user_context = context;
    session->sockets = sockets;

    init_file_sync_buffer(&session->sync_buffer);
   
    return session;
}


UserContext *create_context(char *username){
    UserContext *ctx = malloc(sizeof(UserContext));
    for (int i = 0; i < MAX_SESSIONS; i++) {
        ctx->sessions[i] = NULL;
    }

    ctx->username = username;
    ctx->file_list = NULL;
    pthread_mutex_init(&ctx->lock, NULL);

    return ctx;
}

int is_session_empty(Session *s) {
    return s == NULL;
}

void send_file_to_session(int send_to_index, UserContext *context, char *filename, FileEntryType type){
    if(!is_session_empty(context->sessions[send_to_index])){
        add_file_to_sync_buffer(context->sessions[send_to_index], filename, type);
    }
}

Session *get_user_session(UserContext *context, int session_index) {
    Session *session_to_return = NULL;

    if (context == NULL) {
        fprintf(stderr, "Error: UserContext is NULL when trying to get session.\n");
        return NULL;
    }

    if (session_index < 0 || session_index >= MAX_SESSIONS) {
        fprintf(stderr, "Error: Session index %d is out of bounds (0 to %d).\n", session_index, MAX_SESSIONS - 1);
        return NULL;
    }

    pthread_mutex_lock(&context->lock);
    if (context->sessions[session_index] != NULL) {
        session_to_return = context->sessions[session_index];
    }
    pthread_mutex_unlock(&context->lock);

    return session_to_return;
}

Session *get_user_session_by_address(UserContext *context, const struct sockaddr_in *target_address) {
    Session *found_session = NULL;

    if (context == NULL || target_address == NULL) {
        fprintf(stderr, "Error: UserContext or target_address is NULL.\n");
        return NULL;
    }

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (context->sessions[i] != NULL) {
            Session *current_session = context->sessions[i];

            if (current_session->device_address.sin_addr.s_addr == target_address->sin_addr.s_addr &&
                current_session->device_address.sin_port == target_address->sin_port)
            {

                found_session = current_session;
                break; 
                
            }
        }
    }

    return found_session;
}

void disconnect_all_users(HashTable *table){
    if (!table) {
        return;
    }
    for (size_t i = 0; i < table->size; i++) {
        pthread_mutex_lock(&table->locks[i]);
        LinkedList node = table->array[i];
        if (!node) {
            pthread_mutex_unlock(&table->locks[i]);
            continue;
        }
        while (node) {
            UserContext *context = node->value;

            if (context != NULL)
            {
                pthread_mutex_lock(&context->lock);

                for (size_t i = 0; i < MAX_SESSIONS; i++)
                {
                    if (context->sessions[i] != 0)
                    {
                        free(context->sessions[i]);
                        context->sessions[i] = 0;
                    }
                }

                pthread_mutex_unlock(&context->lock);
            }
            
            node = node->next;
        }
        pthread_mutex_unlock(&table->locks[i]);
    }
}

void send_packet_all_users(HashTable *table, const Packet *packet){
    if (!table) {
        return;
    }
    for (size_t i = 0; i < table->size; i++) {
        pthread_mutex_lock(&table->locks[i]);
        LinkedList node = table->array[i];
        if (!node) {
            pthread_mutex_unlock(&table->locks[i]);
            continue;
        }
        while (node) {
            UserContext *context = node->value;

            if (context != NULL)
            {
                pthread_mutex_lock(&context->lock);

                for (size_t i = 0; i < MAX_SESSIONS; i++)
                {
                    if (context->sessions[i] != 0)
                    {
                        struct sockaddr_in client_address =  context->sessions[i]->device_address;
                        client_address.sin_port = htons(NOTIFICATION_PORT);
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(client_address.sin_addr), ip_str, INET_ADDRSTRLEN);
                        int port = ntohs(client_address.sin_port);
                        int notification_socketfd;

                        if ((notification_socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                            perror("ERRO criando socket --------------------------------------------");
                            close(notification_socketfd);
                            continue;
                        }

                        if (connect(notification_socketfd, (struct sockaddr *) &client_address,sizeof(client_address)) < 0) {
                            perror("ERRO conectando ao servidor --------------------------------------------");
                            continue;
                        }
                        send_packet(notification_socketfd, packet);
                        fprintf(stderr, "Reconnect enviado pra user em %s:%d\n", ip_str,port);
                    }
                }

                pthread_mutex_unlock(&context->lock);
            }
            
            node = node->next;
        }
        pthread_mutex_unlock(&table->locks[i]);
    }
}