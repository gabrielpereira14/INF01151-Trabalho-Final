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

#include "../util/communication.h"

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
#define OK       0
#define NO_INPUT 1
#define MAX_INPUT_SIZE 128
#define MAX_COMMAND 13
#define MAX_ARGUMENT 115

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



void *start_console_input_thread(){
    char command[MAX_COMMAND] = "\0";
    char path[MAX_ARGUMENT] = "\0";

    printf("Client started!");


    while (strcmp(command, "exit") != 0)
    {
        command[0] = '\0';
        path[0] = '\0';

        get_command(command,path);

        //printf("Command: %s, Argument: %s\n", command, path);
        
        if (strcmp(command, "exit") == 0)
        {
            printf("Client closed\n");
            break;
        }
        else if (strcmp(command, "get_sync_dir") == 0)
        {
            //Tem que ver se tem no server antes de criar

            create_sync_dir();
        }
        else if (strcmp(command, "list_client") == 0)
        {
            printf("TODO: list_client\n");
        }
        else if (strcmp(command, "list_server") == 0)
        {
            printf("TODO: list_server\n");
        }
        else if (strcmp(command, "upload") == 0)
        {
            printf("TODO: upload\n");
        }
        else if (strcmp(command, "delete") == 0)
        {
            printf("TODO: delete\n");
        }
        else if (strcmp(command, "download") == 0)
        {
            printf("TODO: download\n");
        }
        else
        {
            printf("Unknown command: %s\n", command);
        }
      
    }
    pthread_exit(0);
}

void *start_directory_watcher_thread() {
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
        wd = inotify_add_watch(fd, sync_dir_path, IN_CREATE | IN_CLOSE_WRITE);
    }while(wd < 0);
  

    //printf("Watching '%s' for IN_CREATE and IN_CLOSE_WRITE...\n", sync_dir_path);

    while (1) {
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

            if (event->mask & IN_CREATE) {
                //printf("IN_CREATE: %s\n", event->name);
            }

            if (event->mask & IN_CLOSE_WRITE) {
                //printf("IN_CLOSE_WRITE: %s\n", event->name);
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


void *test_send_file(void *arg){
    int port = *((u_int16_t *) arg);
    free(arg);
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
    char *hostname = "localhost";
    
	
	server = gethostbyname(hostname);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        printf("ERROR opening socket\n");
    } 
        
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(port);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
    
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        printf("ERROR connecting\n");

    FILE *file_ptr = fopen("in.pdf", "rb");
    if(file_ptr == NULL)
    {
        printf("Error opening file!");   
        exit(1);             
    }
    send_file(sockfd,file_ptr);
    bzero(buffer,256);
    n = read(sockfd, buffer, 256);
    if (n < 0) 
		printf("ERROR reading from socket\n");

    printf("%s\n",buffer);
    
	close(sockfd);
    pthread_exit(NULL);
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

    if (argc >= 2) {
       username = argv[1]; 
    } else {
        fprintf(stderr,"ERRO deve ser fornecido um nome de usuario\n");
        exit(EXIT_FAILURE);
    }

    if(set_sync_dir_path() != 0){
        return EXIT_FAILURE;
    }

    struct hostent *server;
	if ((server = gethostbyname("localhost")) == NULL) {
        fprintf(stderr,"ERRO servidor nao encontrado\n");
        return 1;
    }

    int sock_interface;
    if (connect_to_server(&sock_interface,server, 4000, username) != 0){
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
    
    struct sockaddr_in send_serv_addr = setup_socket_address(server, 4001);
    struct sockaddr_in receive_serv_addr = setup_socket_address(server, 4002);
	if (connect(sock_receive, (struct sockaddr *) &receive_serv_addr,sizeof(receive_serv_addr)) < 0) {
        fprintf(stderr,"ERRO conectando ao servidor de receive\n");
        exit(EXIT_FAILURE);
    }
	if (connect(sock_send, (struct sockaddr *) &send_serv_addr,sizeof(send_serv_addr)) < 0) {
        fprintf(stderr,"ERRO conectando ao servidor de send\n");
        exit(EXIT_FAILURE);
    }

    pthread_t console_thread, file_watcher_thread, test_thread;
    pthread_create(&console_thread, NULL, start_console_input_thread, NULL);
    pthread_create(&file_watcher_thread, NULL, start_directory_watcher_thread, NULL);




    u_int16_t *port = malloc(sizeof(*port));
    *port = TEST_PORT;
    if(argc >= 3){
        *port = atoi(argv[2]);
    }

    pthread_create(&test_thread, NULL, test_send_file, port);



    pthread_join(console_thread, NULL);

	close(sock_interface);
    return EXIT_SUCCESS;
}