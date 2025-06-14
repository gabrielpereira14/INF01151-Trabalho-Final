#include "./connectionManagement.h"
#include "./contextHashTable.h"
#include <netinet/in.h>

uint32_t crc32(const char *filepath) {
    uint8_t buffer[1024];
    uint32_t crc = 0xFFFFFFFF;
    size_t bytesRead;
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Error opening file");
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

int add_file_to_context(HashTable *table, const char *filename, char *username){
    UserContext *context = HashTable_search(table, username);
    if (!context){
        return 1;
    }

    context->file_list = FileLinkedList_push(context->file_list, filename, crc32(filename));
    return 0;
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

void send_file_to_session(int send_to_index, UserContext *context, char *filepath){
    if(!is_session_empty(context->sessions[send_to_index])){
        add_file_to_sync_buffer(context->sessions[send_to_index], filepath);
    }
}


