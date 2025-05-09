#include <hashTable.h>

void push(LinkedList *list, Value value) {
    LinkedList new_head = malloc(sizeof(Node));
    new_head->value = value;
    new_head->next = *list;

    list = new_head;
}

HashTable newHashTable(size_t size) {

}

void insertCell(HashTable table, Key key, Value value) {

}

Value *getCell(HashTable table, Key key) {

}