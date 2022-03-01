#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>

#define MAX_SIZE 100
#define MESSAGE_SIZE 32
#define USERNAME_SIZE 32
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

// an user has an address and a string which represents its name
typedef struct user{
	struct sockaddr_in address;
	char name[32];
}user;

// we define a key value pair which will be stored by the map
typedef struct key_value_pair{
	int key;
	struct user value;
}key_value_pair;

// we store the map as an array of key value pairs because I am too lazy to
// implement a hash map at this point
typedef struct map{
	key_value_pair items[MAX_SIZE];
	int size;
}map;

// this global variable will represent all of our users
// the keys are the respective TCP sockets
// the values are their user structs
map elements;

// we initialise our map by zero'ing each element
void initialise_map(map* current_map){
	for (int i = 0; i < MAX_SIZE; i++){
		memset(&current_map->items[i], 0, sizeof(current_map->items[i]));
	}
}

// we add a pair to the end of the vector
// 0 is success, 1 means chatroom is full, -1 means name already taken
int add_pair(map* current_map, int current_key, struct user current_value){
    if (current_map->items[MAX_SIZE - 1].key != 0){
        printf("Chat is full! Wait for someone to leave...\n");
        return 1;
    }
    for (int i = 0; i < current_map->size; i++){
        if (strcmp(current_value.name, current_map->items[i].value.name) == 0){
            printf("This name is already taken!\n");
            return -1;
        }
    }
	struct key_value_pair pair;
	pair.key = current_key;
	pair.value = current_value;
	// add the pair at the end of our map
	current_map->items[current_map->size] = pair;
	current_map->size++;
	return 0;
}

// we remove a key value pair from the map
void remove_key(map* current_map, int current_key){
	for (int i = 0; i < current_map->size; i++){
		if (current_key == current_map->items[i].key){
			// if we found our current element
			struct user zero_addr;
			memset(&zero_addr, 0, sizeof(zero_addr));
			
			// we set our current element to be the last element
			current_map->items[i] = current_map->items[current_map->size - 1];

			// we set the last element to be zero'd
			current_map->items[current_map->size - 1].key = 0;
			current_map->items[current_map->size - 1].value = zero_addr;
			current_map->size--;
			return;
		}
	}

}

// we find the value represented by a value
struct user find_value(map* current_map, int current_key){
	// we zero the current value in case we don't find anything
	struct user current_value;
	memset(&current_value, 0, sizeof(current_value));
	for (int i = 0; i < current_map->size; i++){
		if (current_map->items[i].key == current_key){
			// if we found a match, we return it
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
	// we send to our new client the number of clients bar him
    int total_clients = client_count - 1;
        // I actually don't know why the next line worked
	// it totally should not
	// send(new_fd, &total_clients, sizeof(total_clients), 0);
	int clients_netw = htonl(total_clients);
	send(fdn, &clients_netw, sizeof(clients_netw), 0);

	int cur_index = 0;
	unsigned int cur_ip = elements.items[cur_index].value.address.sin_addr.s_addr;
	unsigned int cur_port = elements.items[cur_index].value.address.sin_port;
	int cur_key;

	while ((cur_ip != 0 || cur_port != 0) && cur_index < MAX_SIZE){
		struct user cur_user = elements.items[cur_index].value;
		cur_ip = cur_user.address.sin_addr.s_addr;
		cur_port = cur_user.address.sin_port;
		cur_key = elements.items[cur_index].key;

		// send the current_user if we haven't reached the end and if we're not at our current client
		if ((cur_ip != 0 || cur_port != 0) && cur_key != fdn)
			send(fdn, &cur_user, sizeof(cur_user), 0);
		cur_index++;
	}


}

void send_new_address_to_all_clients(int fdn){
	// we send to all clients the address of our new client
	int cur_index = 0;
	// this will contain our new user
	struct user cur_user = find_value(&elements, fdn);
	int cur_key = 1;	

	while (cur_key != 0 && cur_index < MAX_SIZE){
		cur_key = elements.items[cur_index].key;
		// if we haven't reached the end and if we're not at our current user, we send it
		if (cur_key != 0 && cur_key != fdn)
			send(cur_key, &cur_user, sizeof(cur_user), 0);
		cur_index++;
	}	


}


// we handle a new client here
void handle_new_client(){
	// accept a new connection
	addr_len = sizeof(remote_addr);
	if ((new_fd = accept(listener, (struct sockaddr*) &remote_addr, (socklen_t*) &addr_len)) == -1){
		perror("accept");
	}
	else{

		client_count += 1;
		struct sockaddr_in current_address;
		socklen_t length = sizeof(current_address);
		
		// we receive the name of our client
		char msg[MESSAGE_SIZE] = {0};
		recvfrom(udp_socket, msg, MESSAGE_SIZE, 0, (struct sockaddr*) &current_address, &length);
			
		// we create our current struct
		struct user new_user;
		new_user.address = current_address;
		memset(new_user.name, 0, USERNAME_SIZE);
		memcpy(new_user.name, msg, strlen(msg));

		// we add it to the elements and we do the sends both ways
		int status = add_pair(&elements, new_fd, new_user);
		int sent_status = htonl(status);

		send(new_fd, &sent_status, sizeof(sent_status), 0);
		if (status == 0) {
            // add a new fd to the set
            FD_SET(new_fd, &master);
            // if it's the maximum descriptor, we set it as such
            if (new_fd > fd_max){
                fd_max = new_fd;
            }
            printf("selectserver: new connection from %s on socket %d\n", get_IP_address(new_fd, false), new_fd);
            // we increment the client count
            send_all_addresses_to_new_client(new_fd);
            send_new_address_to_all_clients(new_fd);
        }
		else{
		    close(new_fd);
		    printf("Server failed to connect\n");
		}
	}
}

// we handle the exit of a client there
void handle_exit(int socket){
	// we see how many bytes we receive as answer
	nbytes = recv(socket, buf, sizeof(buf), 0);

	// 0 bytes means the client forcibly hung up
	if (nbytes == 0){
		printf("<selectserver>: client %d forcibly hung up\n", socket);
	}
	
	// <0 bytes means there was a problem with the receive function
	if (nbytes < 0){
		perror("recv");
	}
	
	// >0 bytes means our client told us goodbye nicely
	if (nbytes > 0){
		printf("<selectserver>: client %d told us nicely he's leaving\n", socket);
	}

	// we tell all clients that the client does not exist anymore
	send_new_address_to_all_clients(socket);	
	// use free
	//
	// and we remove the key
	remove_key(&elements, socket);
	
	// decrement the client count, close the socket and clear it from the master set
	client_count--;
	close(socket);
	FD_CLR(socket, &master);
}



int main(int argc, char** argv){
	// we zero the master set and read_fds
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	// if we couldn't create a listener TCP socket, we exit
	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(1);
	}
	
	// if we can't set the options to reuse the address, then we exit 
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
		perror("setsockopt");
		exit(1);
	}

	// we create the server address
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SERVER_PORT);

	// we bind the listener to our address
	if (bind(listener, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		perror("bind");
		exit(1);
	}

	// we create a udp socket and if we fail we exit
	if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("socket");
		exit(1);
	}

	// we bind the udp socket to the server address as well
	if (bind(udp_socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		perror("bind");
		exit(1);
	}

	// and we listen for 10 connections, and if it fails we exit
	if (listen(listener, 10) == -1){
		perror("listen");
		exit(1);
	}

	// we have only one element in the master set, that is the listener, thus it is the max
	FD_SET(listener, &master);
	fd_max = listener;

	// infinite loop
	for (;;){
		// save the master set
		read_fds = master;

		// if we don't manage to select, there was an error and we exit
		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(1);
		}

		// if there's a new message on the listener, that means there's a new client and we need to handle it
		// otherwise, we need to handle its exit 
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
