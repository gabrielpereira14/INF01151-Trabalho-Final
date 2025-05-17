
#ifndef FILE_HASH_TABLE_HEADER
#define FILE_HASH_TABLE_HEADER
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char filename[256];
    uint32_t crc;
} FileEntry;

typedef struct filenode {
    char* key;
    FileEntry *value;
    struct filenode *next;
} FileNode;

typedef FileNode *FileLinkedList;

extern const FileLinkedList EMPTY_FILE_LINKED_LIST;
void FileLinkedList_push(FileLinkedList *list, char* key, FileEntry *value);

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