#include <stdio.h>
#include <sys/socket.h>
#include <string.h> //strlen
#include <arpa/inet.h> //htons
#include <unistd.h> //fd operators
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#define PORT 420
#define BACKLOG 5

int connect_server(int socket_desc,struct sockaddr_in server, int server_len){
	if(bind(socket_desc,(struct sockaddr *)&server,server_len)<0){
		perror("[-] Failed to bind socket");
		exit(EXIT_FAILURE);
	}
	
	printf("[+] Binded socket on %s:%d\n",inet_ntoa(server.sin_addr),PORT);

	if(listen(socket_desc,BACKLOG)<0){
		perror("[+] Failed to listen for connections");
		exit(EXIT_FAILURE);
	}
	printf("[+] Listening with a backlog of %d\n",BACKLOG);
	return 1;
}

int clean_fd(int fd){
	char ch = ' ';
	while(ch != '\n'){
	  	read(fd,&ch,sizeof(ch));
	}
	return 1;
}

int broadcast_msg(int max_fds, int fd, char *input, char ip[max_fds][13], int port[], int msg_size){
	char msg[msg_size+10], temp[msg_size+10];
	for(int j=4; j<max_fds; j++){
		//wtf why
		sprintf(temp,"%s:%d> %s",ip[fd],port[fd],input);
		strncpy(msg,temp,sizeof(msg));
		write(j,msg,sizeof(msg));
	}
	memset(temp, 0, sizeof(temp));
	memset(msg, 0, sizeof(msg));
	return 1;
}

int handle_disconnect(int* max_fds, int fd, fd_set* current_fds, char ip[*max_fds][13], int port[]){
	printf("[-] CLIENT %s:%d DISCONNECTED\n",ip[fd],port[fd]);
	if(fd+1 == *max_fds){
		//might want to change the way we check for max fd...
		*max_fds -= 1;
	}
	close(fd);
	FD_CLR(fd, current_fds);
	return *max_fds;
}

int accept_connection(int socket_desc, int* max_fds, char ip[*max_fds][13], int port[], fd_set* current_fds){
	
	struct sockaddr_in client;
	int client_sock, client_len;
	client_len = sizeof(client);
	
	client_sock = accept(socket_desc, (struct sockaddr *)&client,&client_len);
	printf("[+] Accepting connections from %s:%d\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port)); //inet_ntoa extracts ip from the struct
	strncpy(ip[client_sock],inet_ntoa(client.sin_addr),13);
	port[client_sock] = ntohs(client.sin_port);
	if(client_sock < 0){
		perror("[-] Failed to accept connections\n");
		exit(EXIT_FAILURE);
	}
	
	//very bad way to check for max file descriptor. will implement this in a different way in the future
	if(client_sock+1 >= *max_fds){
		*max_fds = client_sock+1;

	}

	FD_SET(client_sock, current_fds);

	return 1;
}

void connect_client(int socket_desc){
	int port[FD_SETSIZE];
	int max_fds = 5;
	fd_set current_fds,read_fds;
	char ip[FD_SETSIZE][13];
	FD_ZERO(&current_fds);
	FD_SET(socket_desc, &current_fds);
	
	for (;;){
		read_fds = current_fds;
		int selected = select(FD_SETSIZE+1,&read_fds,NULL,NULL,NULL);
		if(selected==-1){
			perror("select");
			exit(EXIT_FAILURE);
		}	

		//split to handle_connection function
		for(int i=0; i<max_fds;i++){
			if(FD_ISSET(i, &read_fds)){
				if(i == socket_desc){
					//split to accept_connection function
					accept_connection(socket_desc, &max_fds, ip, port, &current_fds);
				}		
				else{
					char input[100];
					int len;
					len = recv(i,input,sizeof(input), MSG_PEEK);
					//split to hand_disconnect function
					if(len<1){
						handle_disconnect(&max_fds,i,&current_fds,ip,port);
						break;
					}
					if(len>0){
						//broadcast to all file descriptors
						broadcast_msg(max_fds,i,input,ip, port, sizeof(input));
						//clean up
						clean_fd(i);
						memset(input, 0, sizeof(input));

					}
				}
		 	}
		 } 
		 
	}
}

int kill_binding(int socket_desc){
	int tr=1;
	// kill "Address already in use" error message
	if (setsockopt(socket_desc,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}
	return 1;
}

int main(){
	int socket_desc, server_len, client_len;
	socket_desc = socket(AF_INET,SOCK_STREAM, 0); //AF_INT is ipv4, SOCK_STREAM is for TCP, 0 is for the IP protocol
	kill_binding(socket_desc);
	
	printf("Socket status: %d\n",socket_desc);
	if(socket_desc == -1){
		perror("[-] Socket failed");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY); //127.0.0.1, htonl for converting inaddr_loopback as a valid address 
	
	server_len = sizeof(server);
	
	connect_server(socket_desc,server,server_len);
	connect_client(socket_desc);
	
	
	
	close(socket_desc);
	
	return 0;
}
