
#include "communication.h"
#include <stdio.h>

void print_packet(const Packet* packet) {
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
        case PACKET_RECONNECT:
            type_name = "RECONNECT";
            break;
        case PACKET_ERROR:
            type_name = "RECONNECT";
            break;
    }

    fprintf(stderr, "Packet - type: %s length: %i ",
        type_name,
        packet->length);
        
    if (packet->length > 0) {
        fprintf(stderr,"payload: ");
        for (size_t i = 0; i < packet->length; i++) {
            fprintf(stderr,"%c", packet->payload[i]);
        }
    } else {
        fprintf(stderr,"empty_payload");
    }

    fprintf(stderr,"\n");
}

Packet* create_packet(const PacketTypes type, const uint16_t lenght, const char *payload){
    Packet *packet = (Packet*)calloc(1,sizeof(Packet) + lenght);
    packet->type = type;
    packet->length = lenght;
    if (lenght > 0) {
        if (payload == NULL) {
            fprintf(stderr, "ERRO: tentativa de criacao de pacote com conteúdo inválido\n");
            exit(1);
        }

        memcpy(packet->payload, payload, lenght);
    }

    fprintf(stderr, "Pacote criado: %d\n", type);
    return packet;
}

Packet *read_packet(int newsockfd) {
    Packet *packet = (Packet*)malloc(sizeof(Packet));
    size_t total_read = 0;

    // Read header
    while (total_read < sizeof(Packet)) {
        int bytes_read = read(newsockfd, ((char*)packet) + total_read, sizeof(Packet) - total_read);

        if (bytes_read == 0) {
            free(packet);
            Packet *closed_pkt = create_packet(PACKET_CONNECTION_CLOSED, 0, NULL);
            return closed_pkt;
        }

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "read: No data available (non-blocking).\n");
            } else {
                perror("Error reading packet header");
            }
            free(packet);
            return NULL;
        }

        total_read += bytes_read;
    }

    packet = realloc(packet, sizeof(Packet) + packet->length);

    size_t payload_read = 0;
    while (payload_read < packet->length) {
        int bytes_received = read(newsockfd, ((char*)packet) + sizeof(Packet) + payload_read, packet->length - payload_read);

        if (bytes_received == 0) {
            free(packet);
            Packet *closed_pkt = create_packet(PACKET_CONNECTION_CLOSED, 0, NULL);
            return closed_pkt;
        }

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "read: No data available (non-blocking) during payload.\n");
            } else {
                perror("Error reading packet payload");
            }
            free(packet);
            return NULL;
        }

        payload_read += bytes_received;
    }

    return packet;
}


void write_packets_to_file(char *filepath, uint64_t num_packets, int socket) {
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        fprintf(stderr, "write_packets_to_file: Error opening file '%s': ", filepath);
        perror(NULL);
        return;
    }

    for (size_t i = 0; i < num_packets; i++) {
        Packet *packet = read_packet(socket);
        fwrite(packet->payload, packet->length, sizeof(char), file);
        free(packet);
    }

    fflush(file);  // important!
    if (fsync(fileno(file)) == -1) {
        perror("write_packets_to_file: Error synchronizing file");
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
            perror("Error sending packet"); 
            return -1;
        }
    }
    
    return OK;
}

char *read_file_chunk(FILE *file, size_t chunk_size, size_t *bytes_read) {
    char *buffer = malloc(chunk_size);
    if (!buffer){
        fprintf(stderr, "Malloc falhou\n");
        return NULL;
    }

    *bytes_read = fread(buffer, 1, chunk_size, file);
    if (*bytes_read == 0) {
        if (feof(file)) {
            fprintf(stderr, "Leu todo arquivo\n");
        } else if (ferror(file)) {
            perror("File read error");
        }
        free(buffer);
        return NULL;
    }

    return buffer;
}

void send_file(const int sockfd, char *filename, char *basepath) {
    char *filepath = create_filepath(basepath, filename);
    FILE *file_ptr = fopen(filepath, "rb");
    if(file_ptr == NULL) {
        fprintf(stderr, "send_file: Error opening file '%s': ", filepath);
        perror(NULL);
        return;
    }

    size_t file_size = get_file_size(file_ptr);

    int total_packet_amount = file_size / FILE_CHUNK_SIZE + (file_size % FILE_CHUNK_SIZE ? 1 : 0);

    FileAnnoucement *announcement = (FileAnnoucement*)malloc(sizeof(FileAnnoucement) + strlen(filename));
    announcement->num_packets = total_packet_amount;
    announcement->filename_length = strlen(filename);
    memcpy(announcement->filename, filename, strlen(filename));

    Packet *announcement_packet = create_packet(PACKET_SEND, sizeof(FileAnnoucement) + strlen(filename), (char*)announcement);
    
    if (send_packet(sockfd, announcement_packet) != OK) {
        fprintf(stderr, "send_file: ERROR sending file announcement\n");
        fclose(file_ptr);
        return;
    }

    free(announcement);
    free(announcement_packet);


    size_t total_bytes_read = 0;
    int current_packet = 0;
    while (!feof(file_ptr)) {
        size_t bytes_read;
        char *chunk = read_file_chunk(file_ptr, FILE_CHUNK_SIZE, &bytes_read);
        if (!chunk) {
            fprintf(stderr, "send_file: ERROR reading file chunk\n");
            break;
        }

        Packet *packet = create_packet(PACKET_DATA, bytes_read, chunk);


        if (send_packet(sockfd, packet) != OK) {
            fprintf(stderr, "send_file: ERROR sending file data packet\n");
            free(chunk);
            break;
        }
        free(chunk);
        free(packet);
        
        total_bytes_read += bytes_read;
        current_packet += 1;
    }

    fclose(file_ptr);
    free(filepath);
}

int has_data(int fd, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (result < 0) {
        perror("select failed");
        return -1; 
    }

    if (result > 0 && FD_ISSET(fd, &read_fds)) {
        return 1; 
    } else {
        return 0;
    }
}

char *receive_file(Packet *packet, const char *path, int socketfd){
    FileAnnoucement *announcement = (FileAnnoucement*)(packet->payload);

    char *filename = malloc(announcement->filename_length + 1);
    memcpy(filename, announcement->filename, announcement->filename_length);
    filename[announcement->filename_length] = '\0';

    char *filepath = create_filepath(path, filename);
    write_packets_to_file(filepath, announcement->num_packets, socketfd);
    free(filepath);
    return filename;
}

char *delete_file(Packet *packet, const char *path){
    char *filename = malloc(packet->length + 1);
    memcpy(filename, packet->payload, packet->length);
    filename[packet->length] = '\0';

    char *filepath = create_filepath(path, filename);

    if (remove(filepath) != 0) {
        fprintf(stderr, "Unable to delete file '%s': ", filepath);
        perror(NULL);
    }
    free(filepath);
    return filename;
}

char *handle_send_delete(int socketfd, const char *path, Packet *packet){
    switch (packet->type) {
        case PACKET_SEND: {
            if (packet->length == 0){
                return NULL;
            }
            char *filename = receive_file(packet, path, socketfd);
            return filename;
        }
        case PACKET_DELETE: {
            char *filename = delete_file(packet, path);
            return filename;
        }
        default: {
            return NULL;
        }

    }

}

char *create_filepath(const char *base_path, const char *filename) {
    size_t length = strlen(base_path) + 1 + strlen(filename) + 1;
    if (length > PATH_MAX) {
        fprintf(stderr, "Error path too long\n");
        return NULL;
    }
    char *filepath = malloc(length);
    snprintf(filepath, length, "%s/%s", base_path, filename);

    return filepath;
}
