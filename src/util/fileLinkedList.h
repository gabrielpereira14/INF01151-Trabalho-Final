
#ifndef FILE_HASH_TABLE_HEADER
#define FILE_HASH_TABLE_HEADER
#include <stddef.h>
#include <stdint.h>

typedef struct filenode {
    char* key;
    uint32_t crc;
    struct filenode *next;
} FileNode;

FileNode *FileLinkedList_push(FileNode *list, const char* key, uint32_t file_hash);
FileNode *FileLinkedList_get(FileNode *list, const char* key);


#endif