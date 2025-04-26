
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define PACKET_HEADER_SIZE 10
#define PAYLOAD_SIZE 236


typedef struct packet{ 
    uint16_t type;               

    uint16_t seqn;              
    uint32_t total_size;          
    uint16_t length;            
    const char* _payload;        
} Packet;  

unsigned char* serialize_packet(const Packet* pkt, size_t* out_size);
Packet deserialize_packet(unsigned char *serialized_packet, size_t packet_size);
void print_packet(Packet packet);
Packet create_packet(const int16_t type,const uint16_t seqn,const uint32_t total_size,const uint16_t lenght, const char *payload);
Packet read_packet(int newsockfd);
void write_payload_to_file(char *filename, int socket);
void send_file(const int sockfd, FILE *file_ptr);
size_t get_file_size(FILE *file_ptr);