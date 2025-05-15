
#include "communication.h"

unsigned char* serialize_packet(const Packet* pkt, size_t* out_size) {
    if (!pkt || (pkt->length > 0 && !pkt->_payload)) {
        printf("Invalid packet for serialization\n");
        return NULL;
    }

    size_t total_size = sizeof(uint16_t) * 3 + sizeof(uint32_t) + pkt->length;
    uint8_t* buffer = (uint8_t*)malloc(total_size);
    if (!buffer) return NULL;

    size_t offset = 0;

    // Write type
    buffer[offset++] = pkt->type & 0xFF;
    buffer[offset++] = (pkt->type >> 8) & 0xFF;

    // Write seqn
    buffer[offset++] = pkt->seqn & 0xFF;
    buffer[offset++] = (pkt->seqn >> 8) & 0xFF;

    // Write total_size
    buffer[offset++] = pkt->total_size & 0xFF;
    buffer[offset++] = (pkt->total_size >> 8) & 0xFF;
    buffer[offset++] = (pkt->total_size >> 16) & 0xFF;
    buffer[offset++] = (pkt->total_size >> 24) & 0xFF;

    // Write length
    buffer[offset++] = pkt->length & 0xFF;
    buffer[offset++] = (pkt->length >> 8) & 0xFF;

    // Write payload (se existir)
    if (pkt->length > 0) {
        memcpy(buffer + offset, pkt->_payload, pkt->length);
        offset += pkt->length;
    }

    if (out_size) *out_size = offset;
    return buffer;
}

Packet deserialize_packet(unsigned char *serialized_packet, size_t packet_size){
    Packet packet = {0}; 

    if (!serialized_packet) return packet;

    size_t offset = 0;

    packet.type = serialized_packet[offset];
    offset++;
    packet.type |= serialized_packet[offset] << 8;
    offset++;

    packet.seqn = serialized_packet[offset];
    offset++;
    packet.seqn |= serialized_packet[offset] << 8;
    offset++;

    packet.total_size = serialized_packet[offset];
    offset++;
    packet.total_size |= serialized_packet[offset] << 8;
    offset++;
    packet.total_size |= serialized_packet[offset] << 16;
    offset++;
    packet.total_size |= serialized_packet[offset] << 24;
    offset++;

    packet.length = serialized_packet[offset];
    offset++;
    packet.length |= serialized_packet[offset] << 8;
    offset++;

    if (packet.length > 0) {
        if (offset + packet.length > packet_size) {
            packet._payload = NULL;
            return packet;
        }
        char *buffer = malloc(packet.length);
        if (!buffer) {
            packet._payload = NULL;
            return packet;
        }
        memcpy(buffer, serialized_packet + offset, packet.length);
        packet._payload = buffer; 
    } else {
        packet._payload = NULL;
    }
    

    return packet;
}


void print_packet(Packet packet) {
    printf("Packet - seqn: %i total_size: %i type: %i length: %i payload: %.*s\n",
           packet.seqn,
           packet.total_size,
           packet.type,
           packet.length,
           packet.length,
           packet._payload ? packet._payload : "");
}


Packet create_data_packet(const uint16_t seqn,const uint32_t total_size,const uint16_t lenght, const char *payload){
    Packet packet;
    packet.seqn = seqn;
    packet.total_size = total_size;
    packet.type = PACKET_DATA;
    packet._payload = payload;
    packet.length = lenght;

    return packet;
}

Packet create_control_packet(const int type, const uint16_t lenght, const char *payload){
    Packet packet;
    packet.seqn = 0;
    packet.total_size = 1;
    packet.type = type;
    packet._payload = payload;
    packet.length = lenght;

    return packet;
}



Packet read_packet(int newsockfd) {
    unsigned char header[PACKET_HEADER_SIZE];
    ssize_t n = 0;
    size_t total_read = 0;

    while (total_read < PACKET_HEADER_SIZE) {
        n = read(newsockfd, header + total_read, PACKET_HEADER_SIZE - total_read);
        if (n <= 0) {
            perror("read failed or closed");
            Packet empty = {0};
            return empty;
        }
        total_read += n;
    }
    uint16_t payload_length = header[8] | (header[9] << 8);
    size_t total_packet_size = PACKET_HEADER_SIZE + payload_length;

    unsigned char *buffer = malloc(total_packet_size);
    if (!buffer) {
        perror("malloc failed");
        Packet empty = {0};
        return empty;
    }

    memcpy(buffer, header, PACKET_HEADER_SIZE);

    size_t payload_read = 0;
    while (payload_read < payload_length) {
        n = read(newsockfd, buffer + PACKET_HEADER_SIZE + payload_read, payload_length - payload_read);
        if (n <= 0) {
            perror("read failed or closed");
            free(buffer);
            Packet empty = {0};
            return empty;
        }
        payload_read += n;
    }

    Packet pkt = deserialize_packet(buffer, total_packet_size);
    free(buffer);
    return pkt;
}


void write_payload_to_file(char *filename, int socket) {
    remove(filename);
    

    FILE *file = fopen(filename, "a+");
    if (file == NULL) {
        printf("Error opening file!\n");
        printf("\n%s",filename);
        exit(1);
    }

    Packet packet;
    do {
        packet = read_packet(socket);
        fwrite(packet._payload, packet.length, 1, file);
    } while (packet.seqn < packet.total_size - 1);

    fclose(file);
}



size_t get_file_size(FILE *file_ptr){
    fseek(file_ptr, 0L, SEEK_END);
    size_t file_size = ftell(file_ptr);
    fseek(file_ptr, 0L, SEEK_SET);
    
    return file_size;
}

int send_packet(int sockfd, const Packet *packet){
    size_t serialized_packet_len = 0;
    unsigned char * serialized_packet = serialize_packet(packet, &serialized_packet_len);

    if (!serialized_packet) return 0;

    int n = write(sockfd, serialized_packet, serialized_packet_len);
    
    return n >= 0;
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
void send_file(const int sockfd, char *file_path){

    FILE *file_ptr = fopen(file_path, "rb");
    if(file_ptr == NULL)
    {
        printf("Error opening file!");   
        exit(1);             
    }

    size_t file_size = get_file_size(file_ptr);

    int total_packet_amount = (file_size + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;
    int current_packet = 0;

    size_t total_bytes_read = 0;
    size_t bytes_read;

    char *file_name = basename(file_path);
    Packet control_packet = create_control_packet(PACKET_SEND, strlen(file_name), file_name);

    if (!send_packet(sockfd, &control_packet)) {
        fprintf(stderr, "ERROR sending control packet\n");
        fclose(file_ptr);
        return;
    }

    do
    {
        char *data = read_file_chunk(file_ptr, PAYLOAD_SIZE, &bytes_read);
        Packet packet = create_data_packet(current_packet,total_packet_amount, bytes_read, data);

        if (!send_packet(sockfd, &packet)) {
            fprintf(stderr, "ERROR writing to socket\n");
            free(data);
            break;
        }
        free(data);
        
        total_bytes_read += bytes_read;
        current_packet += 1;
    } while (total_bytes_read < file_size);

    fclose(file_ptr);
}

void receive_file(int socketfd, const char *path_to_save){
    Packet packet = read_packet(socketfd);
    char filename[packet.length + 1];
    memcpy(filename, packet._payload, packet.length);
    filename[packet.length] = '\0';
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", path_to_save, filename);  
    write_payload_to_file(filepath,socketfd);
}


Context *create_context(int socketfd, char *username){
    Context *ctx = malloc(sizeof(Context));
    ctx->username = malloc(strlen(username) + 1);
    strcpy(ctx->username,username);
    ctx->socketfd = socketfd;

    return ctx;
}

void free_context(Context *ctx){
    free(ctx->username);
    free(ctx);
}
