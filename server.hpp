#ifndef SERVER_HPP
#define SERVER_HPP

#include "data.hpp"
#include "helpers.hpp"
#include <string>
#include <queue>

int main(int argc, char* argv[]);

class server
{
	private:
		/* socket accepts clients */
		int accepting;

		/* default route, to parent server */
		int parent;

		connection_manager *conman;

		void process_message(std::string message, int source);

		void do_connect(std::istringstream &smsg, int source);

		void do_disconnect(std::istringstream &smsg, int source);

		void do_nick(std::istringstream &smsg, int source);

		void do_join(std::istringstream &smsg, int source);

		void do_leave(std::istringstream &smsg, int source);

		void do_gettopic(std::istringstream &smsg, int source);

		void do_settopic(std::istringstream &smsg, int source);

		void do_msg(std::istringstream &smsge, int source);

		void do_privmsg(std::istringstream &smsg, int source);

		void do_status(std::istringstream &smsg, int source);

		void do_topic(std::istringstream &smsg, int source);

		void do_nickres(std::istringstream &smsg, int source);

		void send_status(const address *dest, status_code code);

	public:
		/* creates a new server */
		server(std::string port);

		/* close the server */
		~server();

		bool run();

		bool connect_parent(std::string host, std::string port);
};

class server_exception : public std::exception
{
	public:
		const char* what() const throw();
};

#endif /* SERVER_HPP */
