#ifndef SERVER_HPP
#define SERVER_HPP

#include "data.hpp"
#include "helpers.hpp"
#include <string>
#include <queue>
#include <set>

int main(int argc, char* argv[]);

class server
{
	private:
		/* is root */
		bool root;

		/* socket accepts clients */
		int accepting;

		/* default route, to parent server */
		int parent;

		connection_manager *conman;

		void process_message(std::string message, int source);

		bool test_nick(std::string nick);

		void send_nick_res(peer *dest, std::string &nick);


		void send_topic(channel *chan, peer *dest);

		void do_connect(std::istringstream &smsg, int source);

		void do_disconnect(std::istringstream &smsg, int source);

		void do_nick(std::istringstream &smsg, int source);

		void do_join(std::istringstream &smsg, int source);

		void do_leave(std::istringstream &smsg, int source);

		void do_list(std::istringstream &smsg, int source);

		void do_gettopic(std::istringstream &smsg, int source);

		void do_settopic(std::istringstream &smsg, int source);

		void do_msg(std::istringstream &smsge, int source);

		void do_privmsg(std::istringstream &smsg, int source);

		void do_status(std::istringstream &smsg, int source);

		void do_topic(std::istringstream &smsg, int source);

		void do_nickres(std::istringstream &smsg, int source);

		void do_listres(std::istringstream &smsg, int source);

		void do_channel(std::istringstream &smsg, int source);

		void do_quit(std::istringstream &smsg, int source);

		void do_delchannel(std::istringstream &smsg, int source);

		void send_status(const peer *dest, status_code code);

		void send_channel(const peer *scr, const channel *chan);

		void send_to_channel(channel *chan, std::string msg, int source);

		void send_channel_list(peer *dest);

		void send_delete_channel(channel *chan, int source);

		void close_route(int sock);

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
