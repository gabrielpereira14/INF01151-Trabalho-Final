#include "./connectionManagement.h"
#include "./contextHashTable.h"

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

int get_free_session_index(UserContext *context){
    for(int i = 0; i < MAX_SESSIONS; i++){
        if (is_session_empty(context->sessions[i]) != 0)
        {
            return i;
        }
    }

    return -1;
}


int add_session_to_context(HashTable *table, Session* session, char *username, ContextThreads threads){
    UserContext *context = get_or_create_context(table, username);

    pthread_mutex_lock(&context->lock);

    int free_session_index = -1;
    for(int i = 0; i < MAX_SESSIONS; i++){
        if (is_session_empty(context->sessions[i]) != 0) {
            free_session_index = i;
            break;
        }
    }

    if (free_session_index == -1){
        pthread_mutex_unlock(&context->lock);
        return 1;
    }

    session->session_index = free_session_index;
    session->user_context = context;
    session->active = 1;
    init_file_sync_buffer(&session->sync_buffer);

    context->sessions[free_session_index] = session;
    context->threads = threads;

    pthread_mutex_unlock(&context->lock);

    return 0;
}


int add_file_to_context(HashTable *table, const char *filename, char *username){
    UserContext *context = HashTable_search(table, username);
    if (!context){
        return 1;
    }

    pthread_mutex_lock(&context->lock); 

    context->file_list = FileLinkedList_push(context->file_list, filename, crc32(filename));

    pthread_mutex_unlock(&context->lock); 

    return 0;
}


Session *create_session(int interface_socketfd, int receive_socketfd, int send_socketfd){
    Session *session = malloc(sizeof(Session));
    session->session_index = -1;
    session->interface_socketfd = interface_socketfd;
    session->receive_socketfd = receive_socketfd;
    session->send_socketfd = send_socketfd;
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
        add_file_to_sync_buffer(&context->sessions[send_to_index]->sync_buffer,
                                context->username, filepath, send_to_index);
    }
}