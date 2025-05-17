#ifndef CONNECTION_MANEGEMENT_HEADER
#define CONNECTION_MANEGEMENT_HEADER

#include "./communication.h"
#include "./hashTable.h"


int add_session_to_context(HashTable table, Session* session, char *username);

#endif