#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fileLinkedList.h"

FileNode *FileLinkedList_push(FileNode *list, char* key, uint32_t file_hash){
    FileNode *new_head = malloc(sizeof(FileNode));
    new_head->key = strdup(key);
    new_head->crc = file_hash;
    new_head->next = list;

    return new_head;
}

uint32_t *FileLinkedList_get(FileNode *list, const char* key){
    for (FileNode *node = list; node != NULL; node = node->next) {
        if (strcmp(key, node->key) == 0)
            return &node->crc; 
    }
    return NULL;
}

/*

FileHashTable FileHashTable_create(size_t size) {
    FileHashTable result = {.size = size, .array = malloc(sizeof(FileLinkedList)*size)};

    for (size_t i = 0; i < size; i++) {
        result.array[i] = EMPTY_FILE_LINKED_LIST;
    }

    return result;
}

#define PRIME_HASH 131
size_t fileKeyHash(const char* key, size_t modulo) {
    size_t hash = 0;
    for (size_t i = 0; i < strlen(key); i++) {
        hash = (PRIME_HASH * hash + key[i]) % modulo;
    }
    
    return hash;
}

FileEntry *FileHashTable_insert(FileHashTable table, char* key, FileEntry *value){
    FileLinkedList *list = table.array + fileKeyHash(key, table.size);
    FileLinkedList_push(list, key, value);
    return (*list)->value;
}

FileEntry *FileHashTable_search(FileHashTable table, char* key) {
    FileLinkedList list = table.array[fileKeyHash(key, table.size)];
    return FileLinkedList_get(list, key);
}

*/
