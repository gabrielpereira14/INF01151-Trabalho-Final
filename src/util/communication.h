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

#define FILE_CHUNK_SIZE 236

#define MAX_SESSIONS 2

enum SendPacketErrors{
    SOCKET_CLOSED, OK, 
};

typedef enum PacketTypes{
    PACKET_DATA, PACKET_SEND, PACKET_LIST, PACKET_DOWNLOAD, PACKET_DELETE, PACKET_EXIT, PACKET_REPLICA_MSG, PACKET_CONNECTION_CLOSED
} PacketTypes;

typedef struct packet{ 
    PacketTypes type;
    uint16_t length;
    char payload[];
} Packet;  

// Anuncia que um arquivo vai ser enviado nos próximos num_packets pacotes
typedef struct fileAnnoucement {
    uint64_t num_packets, filename_length;
    char filename[];
} FileAnnoucement;

/*
typedef struct sentFile
{
    int sender_socket;
    char *username;
    char *filepath;
} SentFile;
*/

void print_packet(Packet* packet);
Packet *create_packet(const PacketTypes type, const uint16_t lenght, const char *payload);
Packet *read_packet(int newsockfd);
int send_packet(int sockfd, const Packet *packet);
void write_packets_to_file(char *filename, uint64_t num_packets, int socket);
void send_file(const int sockfd, char *filepath);
char *read_file_from_socket(int socketfd, const char *path_to_save);
size_t get_file_size(FILE *file_ptr);

#endif