#ifndef SERVER_ROLES_H
#define SERVER_ROLES_H

#include <pthread.h> 
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <sys/socket.h>  
#include <netinet/in.h>



#include "../util/communication.h"
#include "./replica.h"
#include "./election.h" 

void *replica_listener_thread(void *arg);

#endif 
