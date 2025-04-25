#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT_INTERFACE 4000
#define PORT_TRANSFER 4001
#define MAX_USERNAME_LENGTH 32
const int ANSWER_OK = 1;

void perror_exit(const char *msg); // Escreve a mensagem de erro e termina o programa com falha
void *interface(void* sock); // Recebe e executa os comandos do usuário
void *push(void* sock); // Envia os arquivos para o cliente
void *receive(void* sock); // Recebe os arquivos do cliente

int main() {
    // Cria o socket
	int sock_interface_listen;
	
	if ((sock_interface_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera de coneccoes pela interface: ");

    // Faz bind no port de conecção da interface
	struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT_INTERFACE);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

    if (bind(sock_interface_listen, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		perror_exit("ERRO vinculando o socket de espera de coneccoes pela interface: ");
	
	listen(sock_interface_listen, 5);

    while (1) {
        // Espera uma requisição de conecção
        int sock_interface;
        struct sockaddr_in cli_addr;
	    socklen_t clilen;

        clilen = sizeof(struct sockaddr_in);
	    if ((sock_interface = accept(sock_interface_listen, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		    perror_exit("ERRO ao aceitar coneccoes da interface: ");

        // Verifica se a conecção é válida e responde para o cliente
	    char request[256];

        int request_size = read(sock_interface, request, MAX_USERNAME_LENGTH);
	    if (request_size < 0) 
		    perror_exit("ERRO lendo o pedido de coneccao do usuário: ");

        // TODO: verificar se o usuário já tem dois dispositivos conectados

        // Se válida, cria o socket de transferência
	    struct sockaddr_in transfer_serv_addr;
        int sock_transfer_listen, sock_transfer;
		struct sockaddr_in cli_transfer_addr;
	    socklen_t cli_transfer_len;

    	if ((sock_transfer_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
            perror_exit("ERRO abrindo o socket de espera por coneccao de transferencia: ");

        transfer_serv_addr.sin_family = AF_INET;
	    transfer_serv_addr.sin_port = htons(PORT_TRANSFER);
	    transfer_serv_addr.sin_addr.s_addr = INADDR_ANY;
	    bzero(&(transfer_serv_addr.sin_zero), 8);

		if (bind(sock_transfer_listen, (struct sockaddr *) &transfer_serv_addr, sizeof(transfer_serv_addr)) < 0) 
			perror_exit("ERRO vinulando o socket de espera da coneccao de transferencia: ");
	
		listen(sock_transfer_listen, 1);

 	    if ((sock_transfer = accept(sock_transfer_listen, (struct sockaddr *) &cli_transfer_addr, &cli_transfer_len)) == -1) 
		    perror_exit("ERRO aceitando a coneccao de transferencia");

		// Comunica pra o usuário fazer a conecção
		if (write(sock_interface, &ANSWER_OK, sizeof(ANSWER_OK)) != sizeof(ANSWER_OK))
			perror_exit("ERRO respondendo para o cliente");

        // Lança as threads
		pthread_t interface_thread, push_thread, receive_thread;

		pthread_create(&interface_thread, NULL, interface, (void*)sock_interface);
		pthread_create(&push_thread, NULL, push, (void*)sock_transfer);
		pthread_create(&receive_thread, NULL, receive, (void*)sock_transfer);

		// Guarda os dados da conecção
    }
    
    return 0;
}

void perror_exit(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

// Recebe e executa os comandos do usuário
void *interface(void* sock) {
	return NULL;
}

// Envia os arquivos para o cliente
void *push(void* sock) {
	return NULL;
}

// Recebe os arquivos do cliente
void *receive(void* sock) {
	return NULL;
}