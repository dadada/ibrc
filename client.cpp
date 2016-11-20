#include "client.hpp"
#include <iostream>
#include <stdlib.h>

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

client::client(address server) : addr(server)
{
	throw client_exception();
}

const char* client_exception::what() const throw()
{
	return "client: failed to create a client";
}
