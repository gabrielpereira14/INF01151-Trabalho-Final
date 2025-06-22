
#ifndef HASH_TABLE_HEADER
#define HASH_TABLE_HEADER
#include <stddef.h>
#include <pthread.h>

typedef struct UserContext UserContext;
typedef struct HashTable HashTable;

typedef struct node {
    char* key;
    UserContext *value;
    struct node *next;
} Node;

typedef Node *LinkedList;

extern const LinkedList EMPTY_LINKED_LIST;
void LinkedList_push(LinkedList *list, char* key, UserContext *value);

typedef struct HashTable{
    size_t size;
    LinkedList *array;
    pthread_mutex_t *locks;
} HashTable;

HashTable HashTable_create(size_t size);
// Consome o buffer de key, um novo string deve ser alocado
// para inserir
UserContext *HashTable_insert(HashTable *table, char* key, UserContext *value);
UserContext *HashTable_remove(HashTable *table, char* key);
UserContext *HashTable_search(HashTable *table, char* key);
void HashTable_print(HashTable *table);

#endif