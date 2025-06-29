#ifndef ELECTION_H
#define ELECTION_H
#include "./serverCommon.h"
void *run_election(void *my_id);

void *election_listener_thread(void *arg);
void start_election(int id);

ElectionEvent *create_election_event(ElectionEvent *event, int sender_id);
ElectionEvent *create_election_answer_event(ElectionEvent *event, int sender_id);
ElectionEvent *create_coordinator_event(ElectionEvent *event, int leader_id, struct sockaddr_in leader_address);

void send_coordinator_msg(int my_id, struct sockaddr_in new_manager_address);
int send_election_event(ElectionEvent* event, int socketfd);

#endif