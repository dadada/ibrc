#ifndef DATA_HPP
#define DATA_HPP

#include <string>
#include <iostream>
#include <exception>
#include <unordered_map>

#define DEFAULT_SERVER_PORT "5001"

class address
{
	public:
		const std::string host;
		const std::string port;

		address(std::string hostname, std::string portnum);
};

std::ostream& operator <<(std::ostream& outs, const address &a);

class channel
{
	private:
		const std::string name;
		std::string topic;
		const address op;

	public:
		channel(std::string channel_name, address channel_op);
		const std::string get_topic() const;
		void set_topic(std::string topic);
		const std::string get_name() const;
		const address get_op();
};

enum status_code
{
	connect_success = 100,
	connect_error = 101,
	disconnect_success = 200,
	disconnect_error = 201,
	nick_unique = 300,
	nick_not_authorized = 301,
	nick_not_unique = 302,
	join_known_success = 400,
	join_new_success = 401,
	not_in_channel = 402,
	no_such_channel = 403,
	already_in_channel = 404,
	leave_successful = 405,
	client_not_channel_op = 502,
	msg_delivered = 600,
	no_such_client = 601,
	no_such_client_in_channel = 603,
	privmsg_delivered = 604,
};

class receive_error : public std::exception
{
	public:
		virtual const char* what() const throw();
};

enum ibrc_command
{
	CONNECT,
	DISCONNECT,
	NICK,
	JOIN,
	LEAVE,
	GETTOPIC,
	SETTOPIC,
	MSG,
	PRIVMSG,
	QUIT,
	HELP,
};

static std::unordered_map<ibrc_command, std::string> command_names = {
		{CONNECT, "CONNECT"},
		{DISCONNECT, "DISCONNECT"},
		{NICK, "NICK"},
		{JOIN, "JOIN"},
		{LEAVE, "LEAVE"},
		{GETTOPIC, "GETTOPIC"},
		{SETTOPIC, "SETTOPIC"},
		{MSG, "MSG"},
		{PRIVMSG, "PRIVMSG"},
		{QUIT, "QUIT"},
		{HELP, "HELP"}
};

std::ostream &operator<<(std::ostream &out, const ibrc_command &cmd);

std::istream &operator>>(std::istream &in, ibrc_command &cmd);

#endif /* DATA_HPP */
