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
#include <linux/limits.h>

#define FILE_CHUNK_SIZE 236
#define NOTIFICATION_PORT 12345
#define MAX_SESSIONS 2

enum SendPacketErrors{
    SOCKET_CLOSED, OK, 
};

typedef enum PacketTypes{
    PACKET_DATA,
    PACKET_SEND,
    PACKET_LIST,
    PACKET_DOWNLOAD,
    PACKET_DELETE,
    PACKET_EXIT,
    PACKET_REPLICA_MSG,
    PACKET_CONNECTION_CLOSED,
    PACKET_ERROR,
    PACKET_RECONNECT
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
void send_file(const int sockfd, char *filename, char *basepath);
char *handle_send_delete(int socketfd, const char *path, PacketTypes *result);
size_t get_file_size(FILE *file_ptr);
char *create_filepath(const char *base_path, const char *filename);
int has_data(int socketfd, int timeout_ms);

#endif