#include "./connectionManagement.h"
#include "./contextHashTable.h"

UserContext *get_or_create_context(HashTable *table, char *username){
    UserContext *context = HashTable_search(table,username);

    if(context == NULL){
        context = HashTable_insert(table, username, create_context(username));
    }

    return context;
}

int get_free_session_index(UserContext *context){
    for(int i = 0; i < MAX_SESSIONS; i++){
        if (is_session_empty(&context->sessions[i]) != 0)
        {
            return i;
        }
    }

    return -1;
}


int add_session_to_context(HashTable *table, Session* session, char *username){
    UserContext *context = get_or_create_context(table, username);

    int free_session_index = get_free_session_index(context);

    if (free_session_index == -1){
        return 1;
    }
    session->user_context = context;
    context->sessions[free_session_index] = *session;
    return 0;
}


Session *create_session(int interface_socketfd, int receive_socketfd, int send_socketfd){
    Session *session = malloc(sizeof(Session));
    session->interface_socketfd = interface_socketfd;
    session->receive_socketfd = receive_socketfd;
    session->send_socketfd = send_socketfd;
    return session;
}


UserContext *create_context(char *username){
    UserContext *ctx = malloc(sizeof(UserContext));
    for (int i = 0; i < MAX_SESSIONS; i++) {
        ctx->sessions[i].interface_socketfd = -1;
        ctx->sessions[i].receive_socketfd = -1;
        ctx->sessions[i].send_socketfd = -1;
    }

    ctx->username = username;

    return ctx;
}

int is_session_empty(Session *s) {
    return s->interface_socketfd == -1 &&
           s->receive_socketfd == -1 &&
           s->send_socketfd == -1;
}
