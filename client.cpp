#include "client.hpp"
#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sstream>

#define USAGE "usage: ibrcc <hostname> <port>"

int main(int argc, char* argv[])
{
	std::string port = DEFAULT_SERVER_PORT;
	std::string host;
	if (argc < 2) {
		std::cerr << "ibrcc: too few arguments provided" << USAGE << std::endl;
		exit(EXIT_FAILURE);
	} else if (argc > 3) {
		std::cerr << "ibrcc: excess arguments" << std::endl << USAGE << std::endl;
	} else {
		host = argv[1];
		if (argc > 2) {
			port = argv[2];
		}
	}
	address server_addr (host, port);

	try {
		client c = client(server_addr);
	} catch (client_exception& e) {
		std::cerr << e.what() << std::endl;
	}

	exit(EXIT_SUCCESS);
}

client::client(address server)
{
	sockfd = 0;
	if (connect_client(server) != 0) {
		throw client_exception();
	}
}

client::~client()
{
	//leave_channel(current_channel);
	//disconnect();
}

const char* client_exception::what() const throw()
{
	return "client: failed to create a client";
}

int client::connect_client(address server_addr)
{
	if (sockfd != 0) {
		return -1;
	}

	struct addrinfo *ainfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int status;
	if ((status = getaddrinfo(server_addr.host.c_str(), server_addr.port.c_str(), &hints, &ainfo)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return -1;
	}

	sockfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (sockfd == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		return -1;
	}
	
	if (connect(sockfd, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
		perror("connect");
		return -1;
	}

	char hostn[1024];
	hostn[1023] = '\0';
	gethostname(hostn, 1023);
	hostname = std::string(hostn);

	socklen_t len;
	struct sockaddr_storage addr;

	if (getpeername(sockfd, (struct sockaddr*)&addr, &len) != 0) {
		perror("getpeername");
		return -1;
	}
	
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		port = ntohs(s->sin_port);
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		port = ntohs(s->sin6_port);
	}

	if (send_message("CONNECT", "") != 0) {
		return -1;
	}

	if (recv_status() != 0) {
		return -1;
	}

	return 0;
}

int client::send_message(std::string msg_name, std::string msg_payload)
{
	std::stringstream msg;

	msg << msg_name << " " << hostname << " " << port << " " << msg_payload;

	const std::string& msg_str = msg.str();
	const char* sendbuf = msg_str.c_str();
	if (send(sockfd, sendbuf, strlen(sendbuf), 0) < 0) {
		perror("send");
		return -1;
	}

	int status;

	while ((status = recv_status()) < 0) {
		continue;
	}

	print_status(static_cast<status_code>(status));

	return 0;
}

int client::recv_status()
{
	char buf[2056];
	int status = 0;
	ssize_t byte_count;

	// one byte for string terminator
        byte_count= recv(sockfd, buf, 2055, 0);
	if (byte_count == -1) {
		perror("recv");
		return -1;
	} else if (byte_count == 0) {
		disconnect();
		return 0;
	}

	// terminate the string byte_count < 2056 ?
	buf[byte_count] = '\0';
	std::string token;
	std::stringstream input;
	input << buf;
	std::string dummy;
	std::string msg_name;

	if ((input >> msg_name >> dummy >> status) && msg_name == "STATUS") {
		return status;
	} else {
		return -1;
	}
}

void client::print_status(status_code code)
{
	std::cout << "server: ";
	switch (code) {
		case connect_success:
			std::cout << "connected successfully";
			break;
		case connect_error:
			std::cout << "connection failed";
			break;
		case disconnect_success:
			std::cout << "disconnected successfully";
			break;
		case disconnect_error:
			std::cout << "failed to disconnect";
			break;
		case nick_unique:
			std::cout << "nick unique, success";
			break;
		case nick_not_authorized:
			std::cout << "not authorized to set nick";
			break;
		case nick_not_unique:
			std::cout << "nick not unique, pick another nick";
			break;
		case join_known_success:
			std::cout << "channel exists, joined channel";
			break;
		case join_new_success:
			std::cout << "new channel created, joined channel, channel op";
			break;
		case not_in_channel:
			std::cout << "must be joined to channel for this command";
			break;
		case no_such_channel:
			std::cout << "no such channel";
			break;
		case already_in_channel:
			std::cout << "client can only be joined to one channel at a time";
			break;
		case leave_successful:
			std::cout << "left channel successfully";
			break;
		case client_not_channel_op:
			std::cout << "must be channel op to perform this action";
			break;
		case msg_delivered:
			std::cout << "message delivered successfully";
			break;
		case no_such_client:
			std::cout << "client not found in network";
			break;
		case no_such_client_in_channel:
			std::cout << "client is joined to different channel";
			break;
		case privmsg_delivered:
			std::cout << "successfully delivered private message";
			break;
		default:
			std::cout << "an unknown error occurred";
			break;
	}
	std::cout << std::endl;
}


int client::disconnect()
{
	if (send_message("DISCONNECT", "") != 0) {
		return -1;
	}
	return 0;
}
