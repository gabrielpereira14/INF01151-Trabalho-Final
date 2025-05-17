
#ifndef HASH_TABLE_HEADER
#define HASH_TABLE_HEADER
#include <stddef.h>
#include "./communication.h"

typedef struct node {
    char* key;
    UserContext *value;
    struct node *next;
} Node;

typedef Node *LinkedList;

extern const LinkedList EMPTY_LINKED_LIST;
void LinkedList_push(LinkedList *list, char* key, UserContext *value);

typedef struct {
    size_t size;
    LinkedList *array;
} HashTable;

HashTable HashTable_create(size_t size);
// Consome o buffer de key, um novo string deve ser alocado
// para inserir
UserContext *HashTable_insert(HashTable table, char* key, UserContext *value);
UserContext *HashTable_search(HashTable table, char* key);

#endif