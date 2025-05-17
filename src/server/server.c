#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include "../util/communication.h"
#include "../util/connectionManagement.h"
#include "../util/contextHashTable.h"

#define MAX_USERNAME_LENGTH 32

#define USER_FILES_FOLDER "user files"
const int ANSWER_OK = 1;


HashTable contextTable;

void perror_exit(const char *msg); // Escreve a mensagem de erro e termina o programa com falha
void *interface(void* arg); // Recebe e executa os comandos do usuário
void *send_f(void* arg); // Envia os arquivos para o cliente
void *receive(void* arg); // Recebe os arquivos do cliente
void termination(int sig);


// Variáveis definidas globalmente para poderem ser fechadas na função de terminação
// -1 sempre é um fd inválido, e pode ser fechado sem problemas
int sock_interface_listen = -1, sock_send_listen = -1, sock_receive_listen = -1;

int create_folder_if_not_exists(const char *path, const char *folder_name) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, folder_name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        } else {
            fprintf(stderr, "Error: %s exists but is not a directory\n", full_path);
            return -1;
        }
    }

    if (mkdir(full_path, 0755) != 0) {
        perror("mkdir failed");
        return -1;
    }

    return 0;
}

int main() {
	// Define a função de terminação do programa
	signal(SIGINT, termination);

	contextTable = HashTable_create(30); 
            
	
	if(create_folder_if_not_exists("./", USER_FILES_FOLDER) != 0){
		fprintf(stderr, "Failed to create \"user files\" folder");
	}

    // Cria os sockets de espera de conecção
	if ((sock_interface_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera de coneccoes de interface: ");

	if ((sock_send_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera por coneccao de send: ");
		
	if ((sock_receive_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera por coneccao de receive: ");


    // Faz bind nos ports
	struct sockaddr_in interface_serv_addr, send_serv_addr, receive_serv_addr;

	uint16_t interface_socket_port = 4000;
	

    interface_serv_addr.sin_family = AF_INET;
	interface_serv_addr.sin_port = htons(interface_socket_port);
	interface_serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(interface_serv_addr.sin_zero), 8);

   

   do {
        interface_socket_port = (rand() % 30000) + 2000;
        interface_serv_addr.sin_port = htons(interface_socket_port);
    } while (bind(sock_interface_listen, (struct sockaddr *) &interface_serv_addr, sizeof(interface_serv_addr)) < 0);

	fprintf(stderr, "Server running on port %d\n", interface_socket_port);

	
	uint16_t receive_socket_port = interface_socket_port + 1;
	uint16_t send_socket_port = interface_socket_port + 2;


	receive_serv_addr.sin_family = AF_INET;
    receive_serv_addr.sin_port = htons(receive_socket_port);
    receive_serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(receive_serv_addr.sin_zero), 8);

	send_serv_addr.sin_family = AF_INET;
    send_serv_addr.sin_port = htons(send_socket_port);
    send_serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(send_serv_addr.sin_zero), 8);

	if (bind(sock_receive_listen, (struct sockaddr *) &receive_serv_addr, sizeof(receive_serv_addr)) < 0){ 
			perror_exit("ERRO vinulando o socket de espera da coneccao de receive: ");
	}

	if (bind(sock_send_listen, (struct sockaddr *) &send_serv_addr, sizeof(send_serv_addr)) < 0){ 
		perror_exit("ERRO vinulando o socket de espera da coneccao de send: ");
	}
	
	// Começa a esperar pedidos de conecção
	listen(sock_interface_listen, 5);
	listen(sock_receive_listen, 1);
	listen(sock_send_listen, 1);
	

    while (1) {
        // Espera uma requisição de conecção
        int sock_interface;
        struct sockaddr_in cli_addr;
	    socklen_t clilen;

        clilen = sizeof(struct sockaddr_in);
	    if ((sock_interface = accept(sock_interface_listen, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		    perror_exit("ERRO ao aceitar coneccoes da interface: ");

        // Verifica se a conecção é válida e responde para o cliente
	    char username[MAX_USERNAME_LENGTH + 1];

        int request_size = read(sock_interface, username, MAX_USERNAME_LENGTH);
	    if (request_size < 0) 
		    perror_exit("ERRO lendo o pedido de coneccao do usuário: ");

		username[request_size] = '\0';
		printf("Usuario: ");
		printf("%s", username);
		printf("\n");
		fflush(stdout);

        // TODO: verificar se o usuário já tem dois dispositivos conectados

		// Comunica pra o cliente fazer a conecção
		if (write(sock_interface, &ANSWER_OK, sizeof(ANSWER_OK)) != sizeof(ANSWER_OK))
			perror_exit("ERRO respondendo para o cliente: ");

		// Cria os socket de transferência
	    int sock_send, sock_receive;
		struct sockaddr_in cli_send_addr, cli_receive_addr;
    	socklen_t cli_send_addr_len = sizeof(struct sockaddr_in);
		socklen_t cli_receive_addr_len = sizeof(struct sockaddr_in);
		
		// Aceita as conecções
		listen(sock_receive_listen,5);
		if ((sock_receive = accept(sock_receive_listen, (struct sockaddr *) &cli_receive_addr, &cli_receive_addr_len)) == -1) 
		    perror_exit("ERRO aceitando a coneccao de receive: ");

		listen(sock_send_listen,5);
		if ((sock_send = accept(sock_send_listen, (struct sockaddr *) &cli_send_addr, &cli_send_addr_len)) == -1) 
		    perror_exit("ERRO aceitando a coneccao de send: ");
		
        // Lança as threads
		pthread_t interface_thread, send_thread, receive_thread;

		Session *user_session = create_session(sock_interface, sock_receive, sock_send);

		if(add_session_to_context(&contextTable, user_session, strdup(username)) != 0){
			// MAXIMO DE SESSAO ATINGIDA, AVISA O USER
			fprintf(stderr, "Maximo de sessoes atingidas para o user %s!\n", username);
			free(user_session);
			continue;
		}

		pthread_create(&interface_thread, NULL, interface, user_session);
		pthread_create(&send_thread, NULL, send_f, &sock_send);
		pthread_create(&receive_thread, NULL, receive, user_session);

    }

    return 0;
}

void perror_exit(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

int receive_command(int socketfd){
	Packet packet = read_packet(socketfd);
	return packet.type;
}

char *get_user_folder(const char *username){
    size_t len = strlen(USER_FILES_FOLDER) + strlen(username) + 2;

    char *path = malloc(len);
    if (path == NULL) {
        perror("malloc");
        return NULL;
    }

    snprintf(path, len, "%s/%s", USER_FILES_FOLDER, username);

	return path;
}

char* list_files(const char *folder_path) {
    struct dirent *entry;
    DIR *dir = opendir(folder_path);
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    size_t buffer_size = 1024;
    char *result = malloc(buffer_size);
    if (result == NULL) {
        perror("malloc");
        closedir(dir);
        return NULL;
    }
    result[0] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && 
           (entry->d_name[1] == '\0' || 
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        size_t needed = strlen(result) + strlen(entry->d_name) + 4;
        if (needed > buffer_size) {
            buffer_size *= 2;
            char *new_result = realloc(result, buffer_size);
            if (new_result == NULL) {
                perror("realloc");
                free(result);
                closedir(dir);
                return NULL;
            }
            result = new_result;
        }

		strcat(result, "  ");
        strcat(result, entry->d_name);
        strcat(result, "\n");
    }

    closedir(dir);
    return result;
}

// Recebe e executa os comandos do usuário
void *interface(void* arg) {
	Session session = *((Session *) arg);

	while (1)
	{
		int command = receive_command(session.interface_socketfd);

		//fprintf(stderr, "Command: %d\n", command);

		switch (command)
		{
		case PACKET_LIST:
			char *folder_path = get_user_folder(session.user_context->username);
			char *files = list_files(folder_path);

			int seqn = 0;
			int total_packets = 1;
			Packet packet = create_data_packet(seqn,total_packets,strlen(files),files);

			if(!send_packet(session.interface_socketfd,&packet)){
				fprintf(stderr, "ERROR responding to list_server");
				free(folder_path);
				free(files);
			}

			free(folder_path);
			free(files);
			break;

		case PACKET_DOWNLOAD: {
			//printf("DOWNLOAD requisitado\n");

    		Packet packet = read_packet(session.interface_socketfd);
    		if (packet.length == 0 || !packet._payload) {
        		fprintf(stderr, "ERROR invalid file name.\n");
        		break;
    		}

    		printf("Requested file: '%.*s'\n", packet.length, packet._payload);

    		char *folder = get_user_folder(session.user_context->username);
    		char filepath[512];
    		snprintf(filepath, sizeof(filepath), "%s/%.*s", folder, packet.length, packet._payload);

    		if (access(filepath, F_OK) != 0) {
        		fprintf(stderr, "ERROR file not found: %s\n", filepath);
        		free(folder);
        		break;
    		}

    		printf("Sending file: %s\n", filepath);
    		send_file(session.interface_socketfd, filepath);

    		free(folder);
    		break;
		}

		case PACKET_DELETE: {
    		//printf("DELETE requisitado\n");
			
    		Packet packet = read_packet(session.interface_socketfd);
    		if (packet.length == 0 || !packet._payload) {
        		fprintf(stderr, "ERROR invalid file name\n");
        		break;
    		}

    		char *folder = get_user_folder(session.user_context->username);
    		char filepath[512];
    		snprintf(filepath, sizeof(filepath), "%s/%.*s", folder, packet.length, packet._payload);

    		if (access(filepath, F_OK) != 0) {
        		fprintf(stderr, "ERROR file not found: %s\n", filepath);
    		} else if (remove(filepath) == 0) {
        		printf("Arquivo excluído: %s\n", filepath);
    		} else {
        		perror("[Servidor] ERROR deleting file");
    		}

    		free(folder);
    		break;
		}
		
		default:
			break;
		}
	}

	free(arg);
	pthread_exit(NULL);
}

// Envia os arquivos para o cliente
void *send_f(void* arg) {
	int socketfd = *((int *) arg);
		
	pthread_exit(NULL);
}

// Recebe os arquivos do cliente
void *receive(void* arg) {
	Session session = *((Session *) arg);
	
	while (1)
	{
		char *folder_path = get_user_folder(session.user_context->username);

		printf("%s", folder_path);

		create_folder_if_not_exists(USER_FILES_FOLDER,session.user_context->username);
		receive_file(session.receive_socketfd, folder_path);

		free(folder_path);
	}
	pthread_exit(NULL);
}

void termination(int sig) {
	close(sock_interface_listen);
	close(sock_send_listen);
	close(sock_receive_listen);

	// TODO: fechar todas as sessões abertas

	exit(EXIT_SUCCESS);
}
