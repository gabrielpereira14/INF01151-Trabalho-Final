#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hashTable.h"

const LinkedList EMPTY_LINKED_LIST = NULL;

void LinkedList_push(LinkedList *list, char* key, UserContext *value) {
    if (list == NULL)
        return;

    LinkedList new_head = malloc(sizeof(Node));
    new_head->key = key;
    new_head->value = value;
    new_head->next = *list;

    *list = new_head;
}

UserContext *LinkedList_get(LinkedList list, char* key){
    for (LinkedList node = list; list != NULL; node = node->next) {
        if (strcmp(key, node->key) == 0)
            return node->value; 
    }
    return NULL;
}

HashTable HashTable_create(size_t size) {
    HashTable result = {.size = size, .array = malloc(sizeof(LinkedList)*size)};

    for (size_t i = 0; i < size; i++) {
        result.array[i] = EMPTY_LINKED_LIST;
    }

    return result;
}

#define PRIME_HASH 131
size_t keyHash(const char* key, size_t modulo) {
    size_t hash = 0;
    for (size_t i = 0; i < strlen(key); i++) {
        hash = (PRIME_HASH * hash + key[i]) % modulo;
    }

    return hash;
}

UserContext *HashTable_insert(HashTable table, char* key, UserContext *value){
    LinkedList *list = table.array + keyHash(key, table.size);
    LinkedList_push(list, key, value);
    return (*list)->value;
}

UserContext *HashTable_search(HashTable table, char* key) {
    LinkedList list = table.array[keyHash(key, table.size)];
    return LinkedList_get(list, key);
}

