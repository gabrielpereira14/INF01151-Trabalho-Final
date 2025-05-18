#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "contextHashTable.h"

const LinkedList EMPTY_LINKED_LIST = NULL;

void LinkedList_push(LinkedList *list, char* key, UserContext *value) {
    if (list == NULL)
        return;

    LinkedList new_head = malloc(sizeof(Node));
    new_head->key = strdup(key);
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
    HashTable result;
    result.size = size;
    result.array = malloc(sizeof(LinkedList) * size);
    result.locks = malloc(sizeof(pthread_mutex_t) * size);

    for (size_t i = 0; i < size; i++) {
        result.array[i] = EMPTY_LINKED_LIST;
        pthread_mutex_init(&result.locks[i], NULL); 
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

UserContext *HashTable_insert(HashTable *table, char* key, UserContext *value){
    size_t index = keyHash(key, table->size);

    pthread_mutex_lock(&table->locks[index]);  

    LinkedList *list = &table->array[index];
    LinkedList_push(list, key, value);  

    UserContext *result = (*list)->value;

    pthread_mutex_unlock(&table->locks[index]); 

    return result;
}

UserContext *HashTable_search(HashTable *table, char* key) {
    size_t index = keyHash(key, table->size);

    pthread_mutex_lock(&table->locks[index]);

    LinkedList list = table->array[index];
    UserContext *result = LinkedList_get(list, key);

    pthread_mutex_unlock(&table->locks[index]);

    return result;
}


void HashTable_destroy(HashTable *table) {
    for (size_t i = 0; i < table->size; i++) {
        pthread_mutex_destroy(&table->locks[i]);

        // Free linked list
        LinkedList node = table->array[i];
        while (node) {
            LinkedList next = node->next;
            free(node->key);      
            free(node->value); 
            free(node);
            node = next;
        }
    }
    free(table->array);
    free(table->locks);
}
