#ifndef SERVER_HPP
#define SERVER_HPP

#include "data.hpp"
#include <string>

int main(int argc, char* argv[]);

class server
{
	private:
		/* listening socket */
		int sockfd;

		/* default route */
		int default_route;

		int bind_server(std::string port);

		void reset_socket(int sock, int def);

	public:
		/* creates a new server */
		server(std::string port);

		/* close the server */
		~server();

		int run();

		int connect_server(std::string host, std::string port);
};

class server_exception : public std::exception
{
	public:
		const char* what() const throw();
};

#endif /* SERVER_HPP */
