#include "server.hpp"
#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sstream>
#include <sys/epoll.h>

int main(int argc, char* argv[])
{
	std::string usage = "usage: ibrcd [-p <listen_port>] [-h <peer_host>] [-k <peer_port>]";
	std::string peer_port = DEFAULT_SERVER_PORT;
	std::string peer_host;
	std::string listen_port = DEFAULT_SERVER_PORT;

	// TODO getopt
	int opt = 0;
	bool wants_connect = false;

	while ((opt = getopt(argc, argv, "p:h:k:")) != -1) {
		switch (opt) {
			case 'p':
				listen_port = optarg;
				break;
			case 'h':
				wants_connect = true;
				peer_host = optarg;
				break;
			case 'k':
				wants_connect = true;
				peer_port = optarg;
				break;
			default:
				std::cerr << usage << std::endl;
				exit(EXIT_FAILURE);
				break;
		}
	}

	try {
		server s = server(listen_port);

		if (wants_connect) {
			if (peer_host != "") {
				if (s.connect_server(peer_host, peer_port) != 0) {
					std::cerr << "failed to connect" << std::endl;
					exit(EXIT_FAILURE);
				}
			} else {
				std::cerr << "usage: -k makes -h mandatory" << std::endl; 
			}
		}

		if (s.run() != 0) { // run the server
			exit(EXIT_FAILURE);
		}

	} catch (server_exception e) {
		std::cerr << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

server::server(std::string port)
{
	sockfd = STDIN_FILENO;
	default_route = STDOUT_FILENO;
	if (bind_server(port) != 0) {
		throw server_exception();
	}
}

server::~server()
{
	if (default_route != STDOUT_FILENO) {
		close(default_route);
		default_route = STDOUT_FILENO;
	}

	if (sockfd != STDIN_FILENO) {
		close(default_route);
		sockfd = STDIN_FILENO;
	}
}

int server::bind_server(std::string port)
{
	if (sockfd != 0) {
		return -1;
	}

	struct addrinfo *ainfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = 0;
	if ((status = getaddrinfo(NULL, port.c_str(), &hints, &ainfo)) != 0) {
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
		reset_socket(sockfd, STDIN_FILENO);
		return -1;
	}

	if (bind(sockfd, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
		perror("bind");
		reset_socket(sockfd, STDIN_FILENO);
		return -1;
	}

	if (listen(sockfd, 5) != 0) {
		perror("listen");
		reset_socket(sockfd, STDIN_FILENO);
		return -1;
	}

	return 0;
}

int server::connect_server(std::string host, std::string port)
{
	if (default_route != STDOUT_FILENO) {
		return -1;
	}

	struct addrinfo *ainfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int status;
	if ((status = getaddrinfo(host.c_str(), port.c_str(), &hints, &ainfo)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return -1;
	}

	default_route = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (default_route == -1) {
		perror("socket");
		reset_socket(default_route, STDOUT_FILENO);
		return -1;
	}

	int yes = 1;
	if (setsockopt(default_route, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		reset_socket(default_route, STDOUT_FILENO);
		return -1;
	}
	
	if (connect(default_route, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
		perror("connect");
		reset_socket(default_route, STDOUT_FILENO);
		return -1;
	}

	return 0;
}

void server::reset_socket(int sock, int def)
{
	close(sock);
	sock = def;
}

int server::run()
{
	while (true) {
		char buf[2056];
		ssize_t bytes_read = recv(default_route, &buf, 2055, 0);
		if (bytes_read < 0) {
			perror("recv");
		}
		buf[bytes_read] = '\0';
		std::cout << buf;
	}
}

const char* server_exception::what() const throw()
{
	return "server: failed to create a server";
}

