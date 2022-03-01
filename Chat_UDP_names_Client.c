#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#define SERVER_PORT 6667
#define USERNAME_LENGTH 32
#define MESSAGE_SIZE 32
#define IP_ADDRESS "10.152.4.78"
//#define IP_ADDRESS "192.168.1.2"
#define MAX_USERS 100
#include <stdbool.h>

// an user has its address and a string representing its name
typedef struct user{
	struct sockaddr_in address;
	char name[USERNAME_LENGTH];
}user;

// check whether in a list of users, a given user is found
bool is_found(struct user* users, struct user current_user){
	struct sockaddr_in address = current_user.address;
	// store here the address we are looking for
	for (int i = 0; i < MAX_USERS; i++){
		struct sockaddr_in cur_addr = users[i].address;
		// store the current address in this struct
		if (cur_addr.sin_port == address.sin_port && cur_addr.sin_addr.s_addr == address.sin_addr.s_addr){
			return true;
		}
		// if the current address has the same port and ip address as the one we are looking for, we have found it, return true
	}
	return false;
	// if we can't find it, return true
}

// remove a user from a vector of users if we can find it, or do nothing if we don't find it
void remove_user(struct user* users, int size, struct user current_user){
	struct sockaddr_in address = current_user.address;
	// store here the address we are looking forward to remove
	for (int i = 0; i < size; i++){
		struct sockaddr_in cur_addr = users[i].address;
		// store here the current address
		if (cur_addr.sin_port == address.sin_port && cur_addr.sin_addr.s_addr == address.sin_addr.s_addr){
			// if the current address has the same port and ip address as the one we are looking to remove
			users[i] = users[size - 1];
			// the last user in the vector is moved into the current position
			memset(&users[size - 1], 0, sizeof(users[size - 1]));
			// and it is zero'd so we know there's nothing there
			return;
		}

	}
}

// return a user by its username, or a zeroed one if we don't find it
struct user find_user_by_name(user* users, char* username){
		unsigned int current_ip = 1, current_port = 1;
		// we store the current port and the current ip in those variables
		// so we know to break out of the while when they are zero
		// we can do this due to how we manipulate the vector
		int i = 0;
		// set i to be the first position

		while ((current_ip != 0 || current_port != 0) && i < MAX_USERS){
			struct sockaddr_in cur_addr = users[i].address;
			// store here the current address
			current_ip = cur_addr.sin_addr.s_addr;
			current_port = cur_addr.sin_port;
			// we store the current ip and port
			char* name = users[i].name;

			// if the names coincide stringwise, return the current user
			if (strcmp(username, name) == 0){
				return users[i];
			}
			// otherwise go forward
			i += 1;

		}

		// if we reached this point, we haven't found a user so we return a zero'd one
		struct user current_user;
		memset(&current_user, 0, sizeof(current_user));
		return current_user;

}

// return a user's username by its address which uniquely identifies it, or return NULL if we don't find
char* find_user_by_address(user* users, struct sockaddr_in address){

	// this works similarly to the above function	
	unsigned int current_ip = address.sin_addr.s_addr, current_port = address.sin_port;
	int i = 0;
	
	while ((current_ip != 0 || current_port != 0) && i <  MAX_USERS){
		struct sockaddr_in cur_addr = users[i].address;
		
		// if the addresses coincide, we return the name of the user
		if (cur_addr.sin_port == current_port && cur_addr.sin_addr.s_addr == current_ip){
			return users[i].name;
		}
		i += 1;
	}

	// otherwise we return NULL
	return NULL;

}

// this gives a string and processes it 
void process_string(char* buf, struct user* users, int udp_socket){
		// this will be the save point for our strtok
		char** save_point = (char**) malloc(sizeof(char*));

		// this goes until the first space, exclusively
		// save point will point to the place after the space
		char* command = strtok_r(buf, " ",  save_point);
		
		// this goes until the first semi-colon, exclusively
		// save point will point to the place after the semi-colon
		char* user_name = strtok_r(*save_point, ":", save_point);
		
		// the message will be everything after the semi-colon
		char* message = *save_point;	

		// we may free the save point as we don't need it anymore
		free(save_point);
		
		// this should be the whisper command, the only one implemented as of yet
		char whisper_command[3] = "\\w\0";

		// if the given command is not a valid command, we let them know and return
		if (strcmp(command, whisper_command) != 0){
			printf("Invalid command!\n");
			return;
		}
		
		// now we find the user to whom we want to send the message 
		// if we don't find it, we print a message and return
		struct user current_user = find_user_by_name(users, user_name);
		if (*((unsigned long int*) &current_user.address) == 0){
			printf("We couldn't find the user %s\n", user_name);
			return;
		}

		// send status will contain the status of our message
		int send_status;
		struct sockaddr_in current_address = current_user.address;
		
		// we send to the user our message
		send_status = sendto(udp_socket, message, 32, 0, (struct sockaddr*) &current_address, sizeof(current_address));
		
		// depending on the status, we send a message
		if (send_status < 0){
			printf("Message couldn't be sent. Please retry\n");
		}
		else{
			printf("Message sent succesfully\n");
		}

}

void read_string_safely(char* string, int maximum_length){
	int current_position = 0;
	int current_character;
	do{
	    current_character = getc(stdin);
	    if (current_character != '\n'){
	        string[current_position] = current_character;
	        current_position++;
	    }
	}
	while (current_character != '\n' && current_position < maximum_length);
}

int main(int argc, char** argv){
	// we read the username of our current user
	char user_name[USERNAME_LENGTH] = {0};
	printf("Your username is:\n");
	read_string_safely(user_name, USERNAME_LENGTH - 1);
	
	// get the hostent which has our ip address
	struct hostent* h = gethostbyname(IP_ADDRESS);

	// we create an UDP socket for the clients to communicate in
	int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	// if it failed, we exit
	if (udp_socket < 0){
		perror("udp socket");
		exit(1);
	}
	
	// we find the address of the client in UDP
	struct sockaddr_in cli_addr;
	socklen_t addr_len = sizeof(cli_addr);
	getsockname(udp_socket, (struct sockaddr*)  &cli_addr, &addr_len);
	
	// we create a tcp socket with which we will communicate with the server
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	// if it failed, we exit
	if (tcp_socket < 0){
		perror("tcp socket");
		exit(1);
	}
	
	// this will store the address of the server
	// our life was made easier by finding the host
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = h->h_addrtype;
	memcpy((char*) &serv_addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	serv_addr.sin_port = htons(SERVER_PORT);

	// we try to connect through tcp to the server
	int connect_status;
	connect_status = connect(tcp_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	// if we couldn't connect, we exit
	if (connect_status < 0){
		perror("connect");
		exit(1);
	}
	
	// we send the username through the udp_socket, which will give the server our address as well
	sendto(udp_socket, user_name, USERNAME_LENGTH, 0, (struct sockaddr*) &serv_addr, sizeof(serv_addr));

	// we create an array of users of size at most MAX_USERS and we set everything to 0
	int number_of_clients, clients_netw;
	struct user users[MAX_USERS];
	for (int i = 0; i < MAX_USERS; i++){
		memset(&users[i], 0, sizeof(serv_addr));		
	}

	int status_netw, status;
	read(tcp_socket, &status_netw, sizeof(status_netw));
	status = ntohl(status_netw);

	if (status == 1){
	    printf("Chatroom is full!\n");
	    close(udp_socket);
	    close(tcp_socket);
	    exit(1);
	}

	if (status == -1){
	    printf("Name is already is taken!\n");
        close(udp_socket);
        close(tcp_socket);
	    exit(-1);
	}


	// we read from the tcp_socket 
	read(tcp_socket, &clients_netw, sizeof(clients_netw));
	number_of_clients = ntohl(clients_netw);
	for (int i = 0; i < number_of_clients; i++){
		read(tcp_socket, &users[i], sizeof(users[i]));
		printf("I have just read user %s\n", users[i].name);
	}

	// file descriptors for the select function
	fd_set read_fds, master;

	FD_ZERO(&read_fds);
	FD_ZERO(&master);
	FD_SET(tcp_socket, &master);
	FD_SET(0, &master);
	FD_SET(udp_socket, &master);

	// this will store the maximum out of the file descriptors
	int fd_max = (tcp_socket < udp_socket) ? udp_socket : tcp_socket;
	
	// we print the format of the message so the user knows
	printf("\\w <name>:<message>\n");

	// we do an infinite loop
	for (;;){
		// we try to check for new users
		read_fds = master;
		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(1);
		}
		
		// if this is true, then we have a new user from the server
		// because only with the server we communicate on TCP
		if (FD_ISSET(tcp_socket, &read_fds)){
			struct user new_user;
			int read_status;
			read_status = read(tcp_socket, &new_user, sizeof(new_user));
			
			// depending on whether the user already exists, we want to add or remove it
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
			
			// if we can't read, that means server has closed connection
			else{
				printf("Server has closed connection...\n");
				break;
			}
		}

		// if we can read from the console, that means a message is trying to be sent
		// so we try to process it
		if (FD_ISSET(0, &read_fds)){
			char buffer[MESSAGE_SIZE] = {0};
			read_string_safely(buffer, MESSAGE_SIZE - 1);
			process_string(buffer, users, udp_socket);
		}

		// if we can read from the UDP sockets, then a message was received from another client
		// so we try to see the message and print it to the console
		if (FD_ISSET(udp_socket, &read_fds)){
			char buf[MESSAGE_SIZE] = {0};
			struct sockaddr_in sender_addr;
			socklen_t addr_len = sizeof(sender_addr);
			recvfrom(udp_socket, buf, MESSAGE_SIZE, 0, (struct sockaddr*) &sender_addr, &addr_len);
			char* user_name = find_user_by_address(users, sender_addr);
			
			if (user_name == NULL){
				printf("Invalid user!\n");
			}
			else{
				printf("from %s:%s\n", user_name, buf);
			}
		}

	}
	
	// we close the sockets, but due to the structure we can't reach there
	// we will try to change that
	close(udp_socket);
	close(tcp_socket);

	return 0;
}
