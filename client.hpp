#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <exception>
#include "data.hpp"

#define BUFLEN 1024

int main(int argc, char* argv[]);

class client
{
	private:
		/* The client socket, connected to one server max.*/
		int socket;

		/* the client address, changes upon connect */
		address addr;

		/* globally unique nickname */
		std::string nick;

		/* null if no channel was joined */
		std::string current_channel;

		/* displays the status of the last command to the user */
		void status(int status_code);

		/* encodes and sends a message */
		int send_message(std::string msg_name, std::string msg_payload);

	public:
		client(address server);

		/* connects to a server */
		int connect(address server_addr);

		/* disconnects from the current server (socket)*/
		int disconnect();

		/* sets or changes the nick */
		int set_nick(std::string new_nick);

		/* joins a channel, fails if already joined a channel */
		int join_channel(std::string chan_name);

		/* leaves a channel */
		int leave_channel(std::string chan_name);

		/* gets the topic for a channel */
		int get_topic(std::string chan_name);

		/* sets the current topic for a channel, must be op */
		int set_topic(std::string chan_name, std::string new_channel_topic);

		/* sends a message to the channel */
		int send_channel_message(std::string channel_name, std::string message);

		/* sends a private message to a client (nick) in a channel */
		int send_private_message(std::string recipient, std::string channel, std::string message);

		/* leaves the current channel, disconnects from the newtwork, stops the client */
		int quit();

};

class client_exception: public std::exception
{
	public:
		virtual const char* what() const throw();
};

#endif /* CLIENT_HPP */
