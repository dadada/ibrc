#ifndef SERVER_HPP
#define SERVER_HPP

#include "data.hpp"
#include <string>

class server
{
	private:
		/* own hostname */
		std::string host;

		/* listening port number */
		std::string port;
};

#endif /* SERVER_HPP */
