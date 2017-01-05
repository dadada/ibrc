#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <exception>
#include <queue>
#include <istream>
#include <sstream>
#include <unistd.h>
#include "data.hpp"
#include "helpers.hpp"

#define BUFLEN 2056

int main(int argc, char* argv[]);

class client
{
	private:
		/* if true the client will be closed */
		bool quit_bit;

		/* The client tcp socket, connected to one server max.*/
		int sockfd;

		/* manages network */
		connection_manager *conman;

		/* hostname */
		std::string hostname;

		/* globally unique nickname */
		std::string nick;

		/* null if no channel was joined */
		std::string current_channel;

		/* receive queue */
		std::queue<std::string> net_input;

		/* send queue */
		std::queue<std::string> net_output;

	public:
		client();

		/* run the client */
		bool run();

		/* connects to a server */
		bool connect_client(std::string host, std::string port);

		/* sends a message */
		bool send_message(std::string msg_name, std::string msg_payload);

		/* disconnects from the current server (socket)*/
		bool disconnect();

		/* sets or changes the nick */
		bool set_nick(std::string new_nick);

		/* joins a channel, fails if already joined a channel */
		bool join_channel(std::string chan_name);

		/* leaves a channel */
		bool leave_channel(std::string chan_name);

		/* gets the topic for a channel */
		bool get_topic(std::string chan_name);

		/* sets the current topic for a channel, must be op */
		bool set_topic(std::string chan_name, std::string new_channel_topic);

		/* sends a message to the channel */
		bool send_channel_message(std::string channel_name, std::string message);

		/* sends a private message to a client (nick) in a channel */
		bool send_private_message(std::string recipient, std::string channel, std::string message);

		/* sends a request for a list of all channels */
		bool send_list();

		/* leaves the current channel, disconnects from the newtwork, stops the client */
		void quit();

		/* checks if client is connected */
		bool connected();

		/* processes the command encoded in msg */
		bool process_command(std::string &command);

		/* processes a message received from the network */
		void process_message(std::string &msg);

		static void cleanup(int signum);
};

client *the_client;

class client_exception: public std::exception
{
	public:
		virtual const char* what() const throw();
};

#endif /* CLIENT_HPP */
