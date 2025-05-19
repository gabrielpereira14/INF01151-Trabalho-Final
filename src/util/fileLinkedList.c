#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fileLinkedList.h"

FileNode *FileLinkedList_push(FileNode *list, const char* key, uint32_t file_hash){
    FileNode *new_head = malloc(sizeof(FileNode));
    new_head->key = strdup(key);
    new_head->crc = file_hash;
    new_head->next = list;

    return new_head;
}

FileNode *FileLinkedList_get(FileNode *list, const char* key){
    for (FileNode *node = list; node != NULL; node = node->next) {
        if (strcmp(key, node->key) == 0)
            return node; 
    }
    return NULL;
}

