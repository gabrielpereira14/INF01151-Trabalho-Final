
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

/*
typedef struct {
    size_t size;
    FileLinkedList *array;
} FileHashTable;

FileHashTable FileHashTable_create(size_t size);
// Consome o buffer de key, um novo string deve ser alocado
// para inserir
FileEntry *FileHashTable_insert(FileHashTable table, char* key, FileEntry *value);
FileEntry *FileHashTable_search(FileHashTable table, char* key);
*/

#endif