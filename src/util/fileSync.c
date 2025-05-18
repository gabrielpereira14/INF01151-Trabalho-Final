#include "fileSync.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 100

FileEntry file_sync_buffer[BUFFER_SIZE];
int start = 0;
int end = 0;
int count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

void add_file_to_sync_buffer(FileSyncBuffer *buffer, const char *username, const char *filename, int session_index) {
    pthread_mutex_lock(&buffer->lock);
    while (buffer->count == BUFFER_SIZE)
        pthread_cond_wait(&buffer->not_full, &buffer->lock);

    FileEntry *entry = &buffer->buffer[buffer->end];
    entry->username = strdup(username);
    entry->filename = strdup(filename);
    entry->to_session_index = session_index;

    buffer->end = (buffer->end + 1) % BUFFER_SIZE;
    buffer->count++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->lock);
}


FileEntry get_next_file_to_sync(FileSyncBuffer *buffer) {
    pthread_mutex_lock(&buffer->lock);

    while (buffer->count == 0)
        pthread_cond_wait(&buffer->not_empty, &buffer->lock);

    FileEntry entry = buffer->buffer[buffer->start];
    buffer->start = (buffer->start + 1) % BUFFER_SIZE;
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