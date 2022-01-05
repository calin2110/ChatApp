#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>


#define SERVER_PORT 6667

fd_set master;
fd_set read_fds;
struct sockaddr_in serv_addr, remote_addr;
int fd_max;
int listener;
int new_fd;
char buf[256], tempbuf[256];
int nbytes, ret;
int yes = 1; 
socklen_t addr_len;
int j, crt, client_count = 0;
int udp_socket;

typedef struct user{
	struct sockaddr_in address;
	char name[32];
}user;

typedef struct key_value_pair{
	int key;
	struct user value;
}key_value_pair;

typedef struct map{
	key_value_pair items[100];
	int size;
}map;

map elements;

void initialise_map(map* current_map){
	for (int i = 0; i < 100; i++){
		memset(&current_map->items[i], 0, sizeof(current_map->items[i]));
	}
}

void add_pair(map* current_map, int current_key, struct user current_value){
	struct key_value_pair pair;
	pair.key = current_key;
	pair.value = current_value;
	current_map->items[current_map->size] = pair;
	current_map->size++;
	// check for an update
}


void remove_key(map* current_map, int current_key){
	for (int i = 0; i < 100; i++){
		if (current_key == current_map->items[i].key){
			struct user zero_addr;
			memset(&zero_addr, 0, sizeof(zero_addr));
			
			current_map->items[i] = current_map->items[current_map->size - 1];
			current_map->items[current_map->size - 1].key = 0;
			current_map->items[current_map->size - 1].value = zero_addr;
			current_map->size--;
			return;
		}
	}

}

struct user find_value(map* current_map, int current_key){
	struct user current_value;
	memset(&current_value, 0, sizeof(current_value));
	for (int i = 0; i < current_map->size; i++){
		if (current_map->items[i].key == current_key){
			return current_map->items[i].value;
		}
	}
	return current_value;
}

struct sockaddr_in get_socket_name(int socket, bool is_client){
	struct sockaddr_in addr;
	addr_len = sizeof(addr);
	int return_value;

	memset(&addr, 0, sizeof(addr));
	if (is_client){
		return_value = getsockname(socket, (struct sockaddr*) &addr, &addr_len);		
	}
	else{
		return_value = getpeername(socket, (struct sockaddr*) &addr, &addr_len);
	}

	if (return_value < 0)
		perror("getsock(peer)name");
	return addr;
}

 char* get_IP_address(int socket, bool is_client){
	struct sockaddr_in addr;
	addr = get_socket_name(socket, is_client);
	return inet_ntoa(addr.sin_addr);
 }

int get_port(int socket, bool is_client){
	struct sockaddr_in addr;
	addr = get_socket_name(socket, is_client);
	return addr.sin_port;
}

void send_all_addresses_to_new_client(int fdn){
        int total_clients = client_count - 1;
        send(new_fd, &total_clients, sizeof(total_clients), 0);
	
	int cur_index = 0;
	unsigned int cur_ip = elements.items[cur_index].value.address.sin_addr.s_addr;
	unsigned int cur_port = elements.items[cur_index].value.address.sin_port;
	int cur_key;

	while ((cur_ip != 0 || cur_port != 0) && cur_index < 100){
		struct user cur_user = elements.items[cur_index].value;
		cur_ip = cur_user.address.sin_addr.s_addr;
		cur_port = cur_user.address.sin_port;
		cur_key = elements.items[cur_index].key;

		if ((cur_ip != 0 || cur_port != 0) && cur_key != fdn)
			send(fdn, &cur_user, sizeof(cur_user), 0);
		cur_index++;
	}


}

void send_new_address_to_all_clients(int fdn){

	int cur_index = 0;
	struct user cur_user = find_value(&elements, fdn);
	int cur_key = 1;	

	while (cur_key != 0 && cur_index < 100){
		cur_key = elements.items[cur_index].key;
		if (cur_key != 0 && cur_key != fdn)
			send(cur_key, &cur_user, sizeof(cur_user), 0);
		cur_index++;
	}	


}



void handle_new_client(){
	addr_len = sizeof(remote_addr);
	if ((new_fd = accept(listener, (struct sockaddr*) &remote_addr, (socklen_t*) &addr_len)) == -1){
		perror("accept");
	}
	else{
		FD_SET(new_fd, &master);
		if (new_fd > fd_max){
			fd_max = new_fd;
		}
		printf("selectserver: new connection from %s on socket %d\n", get_IP_address(new_fd, false), new_fd);
		client_count += 1;
		struct sockaddr_in current_address;
		socklen_t length = sizeof(current_address);
		
		char msg[32] = {0};
		recvfrom(udp_socket, msg, 32, 0, (struct sockaddr*) &current_address, &length);
			
		struct user new_user;
		new_user.address = current_address;
		memset(new_user.name, 0, 32);
		memcpy(new_user.name, msg, strlen(msg));

		add_pair(&elements, new_fd, new_user);	
		send_all_addresses_to_new_client(new_fd);
		send_new_address_to_all_clients(new_fd);
	}
}

void handle_exit(int socket){
	nbytes = recv(socket, buf, sizeof(buf), 0);
	if (nbytes == 0){
		printf("<selectserver>: client %d forcibly hung up\n", socket);
	}
	if (nbytes < 0){
		perror("recv");
	}
	if (nbytes > 0){
		printf("<selectserver>: client %d told us nicely he's leaving\n", socket);
	}
	send_new_address_to_all_clients(socket);	
	// use free
	remove_key(&elements, socket);
	
	client_count--;
	close(socket);
	FD_CLR(socket, &master);
}



int main(int argc, char** argv){
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(1);
	}
	
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
		perror("setsockopt");
		exit(1);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SERVER_PORT);
	if (bind(listener, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		perror("bind");
		exit(1);
	}

	if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket");
		exit(1);
	}

	if (bind(udp_socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		perror("bind");
		exit(1);
	}

	if (listen(listener, 10) == -1){
		perror("listen");
		exit(1);
	}

	FD_SET(listener, &master);

	fd_max = listener;

	for (;;){
		read_fds = master;
		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(1);
		}

		for (int i = 0; i <= fd_max; i++){
			if (FD_ISSET(i, &read_fds)){
				crt = i;
				if (i == listener){
					handle_new_client();
					}
				else{
					handle_exit(i);
				}
			}

		}


	}

	return 0;
}
