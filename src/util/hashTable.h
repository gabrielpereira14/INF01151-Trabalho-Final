#include <stddef.h>

// Temporário, no futuro trocar pela estrutura dos dados de sessão
typedef int Value;
typedef char *Key;

typedef struct node {
    Key key;
    Value value;
    struct node *next;
} Node;

typedef Node *LinkedList;

const LinkedList EMPTY_LINKED_LIST;
void push(LinkedList *list, Key key, Value value);

typedef struct {
    size_t size;
    LinkedList *array;
} HashTable;

HashTable newHashTable(size_t size);
// Consome o buffer de key, um novo string deve ser alocado
// para inserir
void insertCell(HashTable table, Key key, Value value);
Value *getCell(HashTable table, Key key);