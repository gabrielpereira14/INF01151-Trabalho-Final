#ifndef FILE_SYNC_H
#define FILE_SYNC_H
#include <pthread.h>

#define FILE_SYNC_BUFFER_SIZE 10 

struct Session;
typedef struct Session Session;

typedef enum fileEntryType {
    FILE_ENTRY_SEND,
    FILE_ENTRY_DELETE
} FileEntryType;

typedef struct fileEntry {
    int valid;
    FileEntryType type;
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

void add_file_to_sync_buffer(Session *session, const char *filename, FileEntryType type);
FileEntry get_next_file_to_sync(Session *session);
void free_file_entry(FileEntry file_entry);
void init_file_sync_buffer(FileSyncBuffer *buffer);

#endif
