
#include "communication.h"
#include <stdio.h>

void print_packet(Packet* packet) {
    char *type_name;
    switch (packet->type) {
        case PACKET_DATA:
            type_name = "DATA";
            break;
        case PACKET_SEND:
            type_name = "SEND";
            break;
        case PACKET_LIST:
            type_name = "LIST";
            break;
        case PACKET_DOWNLOAD:
            type_name = "DOWNLOAD";
            break;
        case PACKET_DELETE:
            type_name = "DELETE";
            break;
        case PACKET_EXIT:
            type_name = "EXIT";
            break;
        case PACKET_REPLICA_MSG:
            type_name = "MSG";
            break;
        case PACKET_CONNECTION_CLOSED:
            type_name = "CLOSED";
            break;
    }

    printf("Packet - type: %s length: %i ",
        type_name,
        packet->length);
        
    if (packet->length > 0) {
        printf("payload: %.*s", packet->length, packet->payload);
    } else {
        printf("empty_payload");
    }

    printf("\n");
}

Packet* create_packet(const PacketTypes type, const uint16_t lenght, const char *payload){
    Packet *packet = (Packet*)malloc(sizeof(Packet) + lenght);
    packet->type = type;
    packet->length = lenght;
    if (lenght > 0) {
        if (payload == NULL) {
            fprintf(stderr, "ERRO: tentativa de criacao de pacote com conteúdo inválido\n");
            exit(1);
        }

        memcpy(packet->payload, payload, lenght);
    }

    return packet;
}

Packet *read_packet(int newsockfd) {
    Packet *packet = (Packet*)malloc(sizeof(Packet));
    size_t total_read = 0;

    // Read header
    while (total_read < sizeof(Packet)) {
        int bytes_read = read(newsockfd, packet + total_read, sizeof(packet) - total_read);
        if (bytes_read == 0) {
            free(packet);
            Packet *closed_pkt = create_packet(PACKET_CONNECTION_CLOSED, 0, NULL);
            return closed_pkt;
        }
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "read: No data available (non-blocking).\n");
            } else {
                perror("read error on header");
            }
            free(packet);
            return NULL;
        }
        total_read += bytes_read;
    }

    packet = realloc(packet, sizeof(Packet) + packet->length);

    size_t payload_read = 0;
    while (payload_read < packet->length) {
        int bytes_received = read(newsockfd, packet->payload + payload_read, packet->length - payload_read);
        if (bytes_received == 0) {
            free(packet);
            Packet *closed_pkt = create_packet(PACKET_CONNECTION_CLOSED, 0, NULL);
            return closed_pkt;
        }
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "read: No data available (non-blocking) during payload.\n");
            } else {
                perror("read error on payload");
            }
            free(packet);
            return NULL;
        }
        payload_read += bytes_received;
    }

    print_packet(packet);

    return packet;
}


void write_packets_to_file(char *filename, uint64_t num_packets, int socket) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        printf("\n%s\n", filename);
        return;
    }

    for (size_t i = 0; i < num_packets; i++) {
        Packet *packet = read_packet(socket);
        fwrite(packet->payload, packet->length, sizeof(char), file);
        free(packet);
    }

    fflush(file);  // important!
    if (fsync(fileno(file)) == -1) {
        perror("fsync");
    }
    fclose(file);
}

size_t get_file_size(FILE *file_ptr){
    fseek(file_ptr, 0L, SEEK_END);
    size_t file_size = ftell(file_ptr);
    fseek(file_ptr, 0L, SEEK_SET);
    
    return file_size;
}

int send_packet(int sockfd, const Packet *packet){
    if (packet == NULL) return 0; // TODO código de erro

    ssize_t bytes_sent = send(sockfd, packet, sizeof(Packet) + packet->length, MSG_NOSIGNAL); // Or whatever flags you need
    if (bytes_sent == -1) {
        if (errno == EPIPE) {
            return SOCKET_CLOSED; 
        } else {
            perror("send"); 
            return -1;
        }
    }
    
    return OK;
}

char *read_file_chunk(FILE *file, size_t chunk_size, size_t *bytes_read) {
    char *buffer = malloc(chunk_size);
    if (!buffer) return NULL;

    *bytes_read = fread(buffer, 1, chunk_size, file);
    if (*bytes_read < chunk_size && !feof(file)) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

void send_file(const int sockfd, char *filepath) {
    FILE *file_ptr = fopen(filepath, "rb");
    if(file_ptr == NULL) {
        printf("Error opening file!");   
        return;           
    }

    size_t file_size = get_file_size(file_ptr);

    int total_packet_amount = (file_size + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE;

    size_t total_bytes_read = 0;
    size_t bytes_read;

    char *file_name = basename(filepath); // TODO: modificar para manter diretórios

    FileAnnoucement *announcement = (FileAnnoucement*)malloc(sizeof(FileAnnoucement) + strlen(file_name) - 1);
    announcement->num_packets = total_packet_amount;
    announcement->filename_length = strlen(file_name);
    memcpy(announcement->filename, file_name, strlen(file_name) - 1);

    Packet *announcement_packet = create_packet(PACKET_SEND, strlen(file_name), (char*)announcement);

    if (send_packet(sockfd, announcement_packet) != OK) {
        fprintf(stderr, "ERROR sending file announcement\n");
        fclose(file_ptr);
        return;
    }

    free(announcement);
    free(announcement_packet);


    int current_packet = 0;
    do
    {
        char *chunk = read_file_chunk(file_ptr, FILE_CHUNK_SIZE, &bytes_read);
        Packet *packet = create_packet(PACKET_DATA, bytes_read, chunk);

        if (send_packet(sockfd, packet) != OK) {
            fprintf(stderr, "ERROR sending file data packet\n");
            free(chunk);
            break;
        }
        free(chunk);
        free(packet);
        
        total_bytes_read += bytes_read;
        current_packet += 1;
    } while (total_bytes_read < file_size);

    fclose(file_ptr);
}

char *read_file_from_socket(int socketfd, const char *path_to_save){
    Packet *packet = read_packet(socketfd);
    if (packet->length == 0){
        return NULL;
    }

    FileAnnoucement *announcement = (FileAnnoucement*)packet->payload;
    
    char *filename = malloc(packet->length + 1);
    memcpy(filename, announcement->filename, announcement->filename_length);
    filename[announcement->filename_length] = '\0';
    
    char *filepath = malloc(strlen(path_to_save) + 1 + strlen(filename) + 1);
    sprintf(filepath, "%s/%s", path_to_save, filename); 
    write_packets_to_file(filepath, announcement->num_packets, socketfd);

    free(filename);
    return filepath;
}


