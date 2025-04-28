#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT_INTERFACE 4000
#define PORT_PUSH 4001
#define PORT_RECEIVE 4002
#define MAX_USERNAME_LENGTH 32
const int ANSWER_OK = 1;

void perror_exit(const char *msg); // Escreve a mensagem de erro e termina o programa com falha
void *interface(void* sock); // Recebe e executa os comandos do usuário
void *push(void* sock); // Envia os arquivos para o cliente
void *receive(void* sock); // Recebe os arquivos do cliente

int main() {
    // Cria os sockets de espera de conecção
	int sock_interface_listen, sock_push_listen, sock_receive_listen;
	
	if ((sock_interface_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera de coneccoes de interface: ");

	if ((sock_push_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera por coneccao de push: ");
		
	if ((sock_receive_listen = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        perror_exit("ERRO abrindo o socket de espera por coneccao de receive: ");

    // Faz bind nos ports
	struct sockaddr_in interface_serv_addr, push_serv_addr, receive_serv_addr;

    interface_serv_addr.sin_family = AF_INET;
	interface_serv_addr.sin_port = htons(PORT_INTERFACE);
	interface_serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(interface_serv_addr.sin_zero), 8);

    push_serv_addr.sin_family = AF_INET;
    push_serv_addr.sin_port = htons(PORT_PUSH);
    push_serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(push_serv_addr.sin_zero), 8);

	receive_serv_addr.sin_family = AF_INET;
    receive_serv_addr.sin_port = htons(PORT_RECEIVE);
    receive_serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(receive_serv_addr.sin_zero), 8);

    if (bind(sock_interface_listen, (struct sockaddr *) &interface_serv_addr, sizeof(interface_serv_addr)) < 0) 
		perror_exit("ERRO vinculando o socket de espera de coneccoes pela interface: ");

	if (bind(sock_push_listen, (struct sockaddr *) &push_serv_addr, sizeof(push_serv_addr)) < 0) 
		perror_exit("ERRO vinulando o socket de espera da coneccao de push: ");
		
	if (bind(sock_receive_listen, (struct sockaddr *) &receive_serv_addr, sizeof(receive_serv_addr)) < 0) 
			perror_exit("ERRO vinulando o socket de espera da coneccao de receive: ");
	
	// Começa a esperar pedidos de conecção
	listen(sock_interface_listen, 5);
	listen(sock_push_listen, 1);
	listen(sock_receive_listen, 1);

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

		// Comunica pra o cliente fazer a conecção
		if (write(sock_interface, &ANSWER_OK, sizeof(ANSWER_OK)) != sizeof(ANSWER_OK))
			perror_exit("ERRO respondendo para o cliente: ");

		// Cria os socket de transferência
	    int sock_push, sock_receive;
		struct sockaddr_in cli_push_addr, cli_receive_addr;
    	socklen_t cli_push_addr_len, cli_receive_addr_len;

		// Aceita as conecções
		if ((sock_push = accept(sock_push_listen, (struct sockaddr *) &cli_push_addr, &cli_push_addr_len)) == -1) 
		    perror_exit("ERRO aceitando a coneccao de push: ");

		if ((sock_receive = accept(sock_receive_listen, (struct sockaddr *) &cli_receive_addr, &cli_receive_addr_len)) == -1) 
		    perror_exit("ERRO aceitando a coneccao de receive: ");

		// TODO: Guarda os dados da conecção

        // Lança as threads
		pthread_t interface_thread, push_thread, receive_thread;

		pthread_create(&interface_thread, NULL, interface, (void*)sock_interface);
		pthread_create(&push_thread, NULL, push, (void*)sock_push);
		pthread_create(&receive_thread, NULL, receive, (void*)sock_receive);
    }
    
	close(sock_interface_listen);
	close(sock_push_listen);
	close(sock_receive_listen);

    return 0;
}

void perror_exit(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

// Recebe e executa os comandos do usuário
void *interface(void* arg) {
	int sock = (int)arg;

	pthread_exit(NULL);
}

// Envia os arquivos para o cliente
void *push(void* arg) {
	int sock = (int)arg;

	pthread_exit(NULL);
}

// Recebe os arquivos do cliente
void *receive(void* arg) {
	int sock = (int)arg;

	pthread_exit(NULL);
}