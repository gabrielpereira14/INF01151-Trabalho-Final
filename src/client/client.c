#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <errno.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <dirent.h>
#include <time.h>

#include "../util/communication.h"

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
#define OK       0
#define NO_INPUT 1
#define MAX_INPUT_SIZE 128
#define MAX_COMMAND 13
#define MAX_ARGUMENT 115

int signal_shutdown = 0;

char sync_dir_path[PATH_MAX];

int create_sync_dir(){
    if (mkdir(sync_dir_path, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir() error");
            return 1;
        }
    }
    return 0;
}

int copy_file(const char *source_path, const char *dest_path) {
    FILE *src = fopen(source_path, "rb");
    if (src == NULL) {
        perror("Error opening source file");
        return 1;
    }

    FILE *dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        perror("Error opening destination file");
        fclose(src);
        return 1;
    }

    char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(src);
    fclose(dest);

    return 0;
}

char *build_destination_path(const char *source_path) {
    char *path_copy = strdup(source_path);
    if (!path_copy) return NULL;

    const char *file_name = basename(path_copy);

    size_t len = strlen(sync_dir_path) + 1 + strlen(file_name) + 1;
    char *full_path = malloc(len);
    if (full_path) {
        snprintf(full_path, len, "%s/%s", sync_dir_path, file_name);
    }

    free(path_copy);
    return full_path;
}

int upload(const char *source_path){
    char *dest_path = build_destination_path(source_path);
    if (!dest_path) {
        fprintf(stderr, "ERROR: Failed to construct destination path.\n");
        return 1;
    }
    

    if (copy_file(source_path, dest_path) != 0) {
        fprintf(stderr,"ERROR: File copy failed.\n");
        return 1;
    }

    return 0;
}


int get_command(char* command, char* arg)
{
    char input[MAX_INPUT_SIZE];
    
    fflush(stdout);
    if(fgets(input, sizeof(input), stdin) == NULL)
        return NO_INPUT;


    input[strcspn(input, "\n")] = 0;
    
    sscanf(input,"%12s %114s", command, arg);
    
    return OK;
}

void list_server(int socketfd){
    Packet control_packet = create_control_packet(PACKET_LIST, 1, NULL);

    if (!send_packet(socketfd, &control_packet)) {
        fprintf(stderr, "ERROR sending control packet (list_server)\n");
        return;
    }

    Packet packet = read_packet(socketfd);

    fprintf(stderr, "Server files: \n%s\n", packet._payload);
}

void list_client() {
    DIR *dir = opendir(sync_dir_path);
    if (!dir) {
        perror("ERROR opening sync_dir");
        return;
    }

    struct dirent *entry;
    struct stat info;
    char path[PATH_MAX];
    char time_str[20];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && 
           (entry->d_name[1] == '\0' || 
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", sync_dir_path, entry->d_name);
        if (stat(path, &info) == 0) {
            printf("\nFile: %s\n", entry->d_name);

            strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", localtime(&info.st_mtime));
            printf("    Modification time: %s\n", time_str);

            strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", localtime(&info.st_atime));
            printf("          Access time: %s\n", time_str);

            strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", localtime(&info.st_ctime));
            printf(" Change/creation time: %s\n\n", time_str);
        }
    }

    closedir(dir);
}

void download(const char *filename, int socketfd) {
    //printf("Download do arquivo: %s\n", filename);

    // Envia o comando para sinalizar um download
    Packet command = create_control_packet(PACKET_DOWNLOAD, 0, NULL);
    if (!send_packet(socketfd, &command)) {
        fprintf(stderr, "ERROR sending download command\n");
        return;
    }

    // Envia o nome do arquivo requisitado
    Packet name = create_control_packet(PACKET_SEND, strlen(filename), filename);
    if (!send_packet(socketfd, &name)) {
        fprintf(stderr, "ERROR sending file name\n");
        return;
    }

    // SAlva o arquivo no diretorio atual
    read_file_from_socket(socketfd, ".");
    printf("Arquivo '%s' salvo.\n", filename);
}

void delete(const char *filename, int socketfd) {
    //printf("Exclusão do arquivo: %s\n", filename);

    // Envia o comando para sinalizar a exclusao
    Packet command = create_control_packet(PACKET_DELETE, 0, NULL);
    if (!send_packet(socketfd, &command)) {
        fprintf(stderr, "ERROR sending delete command\n");
        return;
    }

    // Envia o nome do arquivo a ser deletado
    Packet name = create_control_packet(PACKET_SEND, strlen(filename), filename);
    if (!send_packet(socketfd, &name)) {
        fprintf(stderr, "ERROR sending file name\n");
        return;
    }

    printf("Exclusão solicitada.\n");
}
void close_client(int socketfd){
    signal_shutdown = 1;

    Packet control_packet = create_control_packet(PACKET_EXIT, 1, NULL);

    if (!send_packet(socketfd, &control_packet)) {
        fprintf(stderr, "ERROR sending control packet (close client)\n");
        return;
    }
}

void *start_console_input_thread(void *arg){
    int socketfd = *((int*) arg);
    char command[MAX_COMMAND] = "\0";
    char path[MAX_ARGUMENT] = "\0";

    printf("Client started!\n");


    while (strcmp(command, "exit") != 0)
    {
        command[0] = '\0';
        path[0] = '\0';

        get_command(command,path);

        if (strcmp(command, "exit") == 0){
            close_client(socketfd);
            printf("Client closed\n");
            break;
        }
        else if (strcmp(command, "get_sync_dir") == 0){
            create_sync_dir();
        }
        else if (strcmp(command, "list_client") == 0){   
            list_client();
        }
        else if (strcmp(command, "list_server") == 0){
            list_server(socketfd);
        }
        else if (strcmp(command, "upload") == 0){
            if (upload(path) != 0)
            {
                fprintf(stderr, "ERROR: Failed to upload file.");
            }
        }
        else if (strcmp(command, "delete") == 0){
            delete(path, socketfd);
        }
        else if (strcmp(command, "download") == 0){
            download(path, socketfd);
        }
        else{
            printf("Unknown command: %s\n", command);
        }
      
    }
    pthread_exit(0);
}

void *start_file_receiver_thread(void* arg) {
    int socket = *(int*)arg;

    while (signal_shutdown)
	{
		char *filepath = read_file_from_socket(socket, sync_dir_path);
        fprintf(stderr,"File received!\n");
		free(filepath);
	}
	pthread_exit(NULL);
}


void *start_directory_watcher_thread(void* arg) {
    int socket = *(int*)arg;
    int fd, wd;
    char buffer[EVENT_BUF_LEN];

    fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        perror("inotify_init1");
        pthread_exit(NULL);
    }

    wd = -1;

    do{
        usleep(200);
        wd = inotify_add_watch(fd, sync_dir_path, 
           // IN_CREATE 
           // | 
            IN_CLOSE_WRITE);
    }while(wd < 0);
  

    while (!signal_shutdown) {
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN);

        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);
                continue;
            } else {
                perror("read");
                break;
            }
        }

        ssize_t i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            int path_size = strlen(sync_dir_path) + strlen(event->name) + 2;
            char event_file_path[path_size];

            snprintf(event_file_path, path_size, "%s/%s", sync_dir_path, event->name);

            if (event->mask & IN_CLOSE_WRITE) {
                sleep(1);
                send_file(socket, event_file_path);
            }

            i += sizeof(struct inotify_event) + event->len;
        } 
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    pthread_exit(NULL);
}

int set_sync_dir_path(){
    if (getcwd(sync_dir_path, sizeof(sync_dir_path)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (strlen(sync_dir_path) + strlen("/sync_dir") >= sizeof(sync_dir_path)) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }
    strcat(sync_dir_path, "/sync_dir");
    return 0;
}


struct sockaddr_in setup_socket_address(struct hostent *server, int port){
    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;     
	sockaddr.sin_port = htons(port);    
	sockaddr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(sockaddr.sin_zero), 8);  

    return sockaddr;
}

int connect_to_server(int *sockfd, struct hostent *server, int port, char *username){
    if (((*sockfd) = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "ERRO abrindo o socket da interface\n");
        return 1;
    }

    struct sockaddr_in interface_serv_addr = setup_socket_address(server, port);

	if (connect((*sockfd),(struct sockaddr *) &interface_serv_addr,sizeof(interface_serv_addr)) < 0) {
        fprintf(stderr,"ERRO conectando ao servidor\n");
        return 1;
    }

	if (write((*sockfd), username, strlen(username)) < 0) {
		fprintf(stderr, "ERRO mandando o pedido de coneccao para o servidor\n");
        return 1;
    }

    int resposta;
    if (read((*sockfd), &resposta, sizeof(resposta)) < 0) {
		fprintf(stderr, "ERRO lendo a resposta do servidor\n");
        return 1;
    }

    if (resposta != 1) {
		fprintf(stderr, "ERRO server negou a coneccao\n");
        
    }

    return 0;
}

int main(int argc, char* argv[]){ 
    char *username;
    char hostname[20];

    strcpy(hostname,"localhost");
    uint16_t console_socket_port = 4000;

    if (argc >= 2) {
       username = argv[1]; 
    } else {
        fprintf(stderr,"ERRO deve ser fornecido um nome de usuario\n");
        exit(EXIT_FAILURE);
    }

    if(argc >= 3){
        strcpy(hostname,argv[2]);
    }
    

    if(argc >= 4){
        console_socket_port = atoi(argv[3]);
    }



    uint16_t send_socket_port = console_socket_port + 1;
    uint16_t receive_socket_port = console_socket_port + 2;


    if(set_sync_dir_path() != 0){
        return EXIT_FAILURE;
    }

    struct hostent *server;
	if ((server = gethostbyname(hostname)) == NULL) {
        fprintf(stderr,"ERRO servidor nao encontrado\n");
        return 1;
    }

    int sock_interface;
    if (connect_to_server(&sock_interface,server, console_socket_port, username) != 0){
        exit(EXIT_FAILURE);
    }
    
    int sock_send, sock_receive;
    if ((sock_send = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "ERRO abrindo o socket se send\n");
        exit(EXIT_FAILURE);
    }
    if ((sock_receive = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "ERRO abrindo o socket de receive\n");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in send_serv_addr = setup_socket_address(server, send_socket_port);
    struct sockaddr_in receive_serv_addr = setup_socket_address(server, receive_socket_port);
	if (connect(sock_receive, (struct sockaddr *) &receive_serv_addr,sizeof(receive_serv_addr)) < 0) {
        fprintf(stderr,"ERRO conectando ao servidor de receive\n");
        exit(EXIT_FAILURE);
    }
	if (connect(sock_send, (struct sockaddr *) &send_serv_addr,sizeof(send_serv_addr)) < 0) {
        fprintf(stderr,"ERRO conectando ao servidor de send\n");
        exit(EXIT_FAILURE);
    }

    pthread_t console_thread, file_watcher_thread, receive_files_thread; 
    pthread_create(&console_thread, NULL, start_console_input_thread, (void *) &sock_interface);
    pthread_create(&file_watcher_thread, NULL, start_directory_watcher_thread, (void*) &sock_send);
    pthread_create(&receive_files_thread, NULL, start_file_receiver_thread, (void*) &sock_receive);

    pthread_join(console_thread, NULL);
    pthread_join(file_watcher_thread, NULL);
    pthread_join(receive_files_thread, NULL);

	close(sock_interface);
    return EXIT_SUCCESS;
}
