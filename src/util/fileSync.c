#include "fileSync.h"
#include "./connectionManagement.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int start = 0;
int end = 0;
int count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

void add_file_to_sync_buffer(Session *session, const char *filename, FileEntryType type) {
    FileSyncBuffer *buffer = &session->sync_buffer;
    pthread_mutex_lock(&buffer->lock);
    while (buffer->count == FILE_SYNC_BUFFER_SIZE)
        pthread_cond_wait(&buffer->not_full, &buffer->lock);

    FileEntry *entry = &buffer->buffer[buffer->end];
    entry->username = strdup(session->user_context->username);
    entry->filename = strdup(filename);
    entry->type = type;
    entry->to_session_index = session->session_index;
    entry->valid = 1;

    buffer->end = (buffer->end + 1) % FILE_SYNC_BUFFER_SIZE;
    buffer->count++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->lock);
}


FileEntry get_next_file_to_sync(Session *session) {
    FileSyncBuffer *buffer = &session->sync_buffer;

    pthread_mutex_lock(&buffer->lock);
    while (buffer->count == 0 && session->active) {
        pthread_cond_wait(&buffer->not_empty, &buffer->lock);
    }
    
    if (!session->active) {
        pthread_mutex_unlock(&buffer->lock);
        FileEntry invalid = { .valid = 0 };
        return invalid;
    }


    FileEntry entry = buffer->buffer[buffer->start];
    buffer->start = (buffer->start + 1) % FILE_SYNC_BUFFER_SIZE;
    buffer->count--;

    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->lock);

    return entry;
}


void free_file_entry(FileEntry file_entry){
    free(file_entry.username);
    free(file_entry.filename);
}

void init_file_sync_buffer(FileSyncBuffer *buffer) {
    buffer->start = 0;
    buffer->end = 0;
    buffer->count = 0;

    pthread_mutex_init(&buffer->lock, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
}