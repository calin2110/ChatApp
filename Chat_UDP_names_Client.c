#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#define SERVER_PORT 6667
#define IP_ADDRESS "10.152.4.78"
#define MAX_USERS 100
#include <stdbool.h>

typedef struct user{
	struct sockaddr_in address;
	char name[32];
}user;

bool is_found(struct user* users, struct user current_user){
	struct sockaddr_in address = current_user.address;
	for (int i = 0; i < 100; i++){
		struct sockaddr_in cur_addr = users[i].address;
		if (cur_addr.sin_port == address.sin_port && cur_addr.sin_addr.s_addr == address.sin_addr.s_addr){
			return true;
		}
	}
	return false;
}

void remove_user(struct user* users, int size, struct user current_user){
	struct sockaddr_in address = current_user.address;
	for (int i = 0; i < size; i++){
		struct sockaddr_in cur_addr = users[i].address;
		if (cur_addr.sin_port == address.sin_port && cur_addr.sin_addr.s_addr == address.sin_addr.s_addr){
			users[i] = users[size - 1];
			memset(&users[size - 1], 0, sizeof(users[size - 1]));
			return;
		}

	}
	//perror("remove");

}


struct user find_user_by_name(user* users, char* username){
		unsigned int current_ip = 1, current_port = 1;
		int i = 0;

		while ((current_ip != 0 || current_port != 0) && i < 100){
			struct sockaddr_in cur_addr = users[i].address;
			current_ip = cur_addr.sin_addr.s_addr;
			current_port = cur_addr.sin_port;
			char* name = users[i].name;

			if (strcmp(username, name) == 0){
				return users[i];
			}

			i += 1;

		}

		struct user current_user;
		memset(&current_user, 0, sizeof(current_user));
		return current_user;

}

char* find_user_by_address(user* users, struct sockaddr_in address){
	unsigned int current_ip = address.sin_addr.s_addr, current_port = address.sin_port;
	int i = 0;
	
	while ((current_ip != 0 || current_port != 0) && i <  100){
		struct sockaddr_in cur_addr = users[i].address;
		
		if (cur_addr.sin_port == current_port && cur_addr.sin_addr.s_addr == current_ip){
			return users[i].name;
		}
		i += 1;
	}
	return NULL;

}

void process_string(char* buf, struct user* users, int udp_socket){
		char** save_point = (char**) malloc(sizeof(char*));

		char* command = strtok_r(buf, " ",  save_point);
		char* user_name = strtok_r(*save_point, ":", save_point);
		char* message = *save_point;	
		free(save_point);
		

		char whisper_command[3] = "\\w\0";
		if (strcmp(command, whisper_command) != 0){
			printf("Invalid command!\n");
			return;
		}
		

		struct user current_user = find_user_by_name(users, user_name);
		if (*((unsigned long int*) &current_user.address) == 0){
			printf("We couldn't find the user %s\n", user_name);
			return;
		}

		int send_status;
		struct sockaddr_in current_address = current_user.address;


		send_status = sendto(udp_socket, message, 32, 0, (struct sockaddr*) &current_address, sizeof(current_address));
		
		if (send_status < 0){
			printf("Message couldn't be sent. Please retry\n");
		}
		else{
			printf("Message sent succesfully\n");
		}

}


int main(int argc, char** argv){
	struct hostent* h;
	h = gethostbyname(IP_ADDRESS);
	// we create an UDP socket for the clients to communicate in
	int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket < 0){
		perror("udp socket");
		exit(1);
	}
	
	// we find the address of the client in UDP
	struct sockaddr_in cli_addr;
	socklen_t addr_len = sizeof(cli_addr);
	getsockname(udp_socket, (struct sockaddr*)  &cli_addr, &addr_len);
	
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0){
		perror("tcp socket");
		exit(1);
	}
	struct sockaddr_in serv_addr;

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = h->h_addrtype;
	memcpy((char*) &serv_addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	serv_addr.sin_port = htons(SERVER_PORT);

	int connect_status;
	connect_status = connect(tcp_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (connect_status < 0){
		perror("connect");
		exit(1);
	}
	
	char user_name[32] = {0};
	printf("Your username is:\n");
	scanf("%s", user_name);

	sendto(udp_socket, user_name, 32, 0, (struct sockaddr*) &serv_addr, sizeof(serv_addr));

	int number_of_clients;
	struct user users[MAX_USERS];
	for (int i = 0; i < MAX_USERS; i++){
		memset(&users[i], 0, sizeof(serv_addr));		
	}

	read(tcp_socket, &number_of_clients, sizeof(number_of_clients));
	for (int i = 0; i < number_of_clients; i++){
		read(tcp_socket, &users[i], sizeof(users[i]));
		printf("I have just read user %s\n", users[i].name);
	}

	fd_set read_fds, master;

	FD_ZERO(&read_fds);
	FD_ZERO(&master);
	FD_SET(tcp_socket, &master);
	FD_SET(0, &master);
	FD_SET(udp_socket, &master);

	int fd_max;
	if (tcp_socket > udp_socket){
		fd_max = tcp_socket;
	}
	else{
		fd_max = udp_socket;
	}

	printf("\\w <name>:<message>\n");

	for (;;){
		read_fds = master;
		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(1);
		}
		// make this do background work, we can use threads for ex	
		
		if (FD_ISSET(tcp_socket, &read_fds)){
			struct user new_user;
			int read_status;
			read_status = read(tcp_socket, &new_user, sizeof(new_user));
			
			if (read_status > 0){
			if (!is_found(users, new_user)){
				printf("I have added user %s\n", new_user.name);
				users[number_of_clients] = new_user;
				number_of_clients++;
			}
			else{
				printf("I have removed user %s\n", new_user.name);
				remove_user(users, number_of_clients, new_user);
				number_of_clients--;	
			}
			}
			
			else{
				printf("Server has closed connection...\n");
				break;
			}
		}


		if (FD_ISSET(0, &read_fds)){
			char buf[256];
			memset(buf, 0, sizeof(buf));
			read(0, buf, sizeof(buf));
			process_string(buf, users, udp_socket);
		}

		if (FD_ISSET(udp_socket, &read_fds)){
			char buf[32] = {0};
			struct sockaddr_in sender_addr;
			socklen_t addr_len = sizeof(sender_addr);
			recvfrom(udp_socket, buf, 32, 0, (struct sockaddr*) &sender_addr, &addr_len); 
			char* user_name = find_user_by_address(users, sender_addr);
			
			if (user_name == NULL){
				printf("Invalid user!\n");
			}
			else{
				printf("from %s:%s", user_name, buf);
			}
		}

	}
	
	close(udp_socket);
	close(tcp_socket);

	return 0;
}
