
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>


#define PACKET_HEADER_SIZE 10
#define PAYLOAD_SIZE 236
#define TEST_PORT 4003

enum PacketTypes{
    PACKET_DATA, PACKET_SEND, PACKET_LIST,
};

typedef struct packet{ 
    uint16_t type;               
    uint16_t seqn;              
    uint32_t total_size;          
    uint16_t length;            
    const char* _payload;        
} Packet;  

typedef struct context
{
    int socketfd;
    char *username;
} Context;


unsigned char* serialize_packet(const Packet* pkt, size_t* out_size);
Packet deserialize_packet(unsigned char *serialized_packet, size_t packet_size);
void print_packet(Packet packet);
Packet create_data_packet(const uint16_t seqn,const uint32_t total_size,const uint16_t lenght, const char *payload);
Packet create_control_packet(const int type, const uint16_t lenght, const char *payload);
Packet read_packet(int newsockfd);
int send_packet(int sockfd, const Packet *packet);
void write_payload_to_file(char *filename, int socket);
void send_file(const int sockfd, char *file_name);
void receive_file(int socketfd, const char *path_to_save);
size_t get_file_size(FILE *file_ptr);
Context *create_context(int socketfd, char *username);
void free_context(Context *ctx);