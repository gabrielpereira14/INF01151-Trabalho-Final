#ifndef FILE_SYNC_H
#define FILE_SYNC_H
#include <pthread.h>

#define FILE_SYNC_BUFFER_SIZE 10 

typedef struct fileEntry {
    char *username;
    int to_session_index;
    char *filename;
} FileEntry;

typedef struct FileSyncBuffer {
    FileEntry buffer[FILE_SYNC_BUFFER_SIZE];
    int start;
    int end;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} FileSyncBuffer;

void add_file_to_sync_buffer(FileSyncBuffer *buffer, const char *username, const char *filename, int session_index);
FileEntry get_next_file_to_sync(FileSyncBuffer *buffer);
void free_file_entry(FileEntry file_entry);
void init_file_sync_buffer(FileSyncBuffer *buffer);

#endif
