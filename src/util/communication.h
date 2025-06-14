#ifndef COMMUNICATION_HEADER
#define COMMUNICATION_HEADER

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <sys/socket.h>

#define PACKET_HEADER_SIZE 10
#define PAYLOAD_SIZE 236
#define TEST_PORT 4003

#define MAX_SESSIONS 2

enum SendPacketErrors{
    SOCKET_CLOSED, OK, 
};

enum PacketTypes{
    PACKET_DATA, PACKET_SEND, PACKET_LIST, PACKET_DOWNLOAD, PACKET_DELETE, PACKET_EXIT, PACKET_REPLICA_MSG, PACKET_CONNECTION_CLOSED
};

typedef struct packet{ 
    uint16_t type;               
    uint16_t seqn;              
    uint32_t total_size;          
    uint16_t length;            
    const char* _payload;        
} Packet;  

struct context;


/*
typedef struct sentFile
{
    int sender_socket;
    char *username;
    char *filepath;
} SentFile;
*/

unsigned char* serialize_packet(const Packet* pkt, size_t* out_size);
Packet deserialize_packet(unsigned char *serialized_packet, size_t packet_size);
void print_packet(Packet packet);
Packet create_data_packet(const uint16_t seqn,const uint32_t total_size,const uint16_t lenght, const char *payload);
Packet create_control_packet(const int type, const uint16_t lenght, const char *payload);
Packet read_packet(int newsockfd);
int send_packet(int sockfd, const Packet *packet);
void write_payload_to_file(char *filename, int socket);
void send_file(const int sockfd, char *filepath);
char *read_file_from_socket(int socketfd, const char *path_to_save);
size_t get_file_size(FILE *file_ptr);

#endif