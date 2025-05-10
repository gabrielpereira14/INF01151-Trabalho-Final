#include "hashTable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void push(LinkedList *list, Key key, Value value) {
    if (list == NULL)
        return;

    LinkedList new_head = malloc(sizeof(Node));
    new_head->key = key;
    new_head->value = value;
    new_head->next = *list;

    *list = new_head;
}

HashTable newHashTable(size_t size) {
    HashTable result = {.size = size, .array = malloc(sizeof(LinkedList)*size)};

    memset(result.array, (int) EMPTY_LINKED_LIST, size);

    return result;
}

#define PRIME_HASH 131
size_t keyHash(const Key key, size_t modulo) {
    size_t hash = 0;
    for (size_t i = 0; i < strlen(key); i++) {
        hash = (PRIME_HASH * hash + key[i]) % modulo;
    }

    return hash;
}

void insertCell(HashTable table, Key key, Value value) {
    push(table.array + keyHash(key, table.size), key, value);
}

Value *getCell(HashTable table, Key key) {
    LinkedList list = table.array[keyHash(key, table.size)];

    for (LinkedList node = list; list != NULL; node = node->next) {
        if (strcmp(key, node->key) == 0)
            return &(node->value); 
    }
}