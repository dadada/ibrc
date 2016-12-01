#ifndef DATA_HPP
#define DATA_HPP

#include <string>
#include <iostream>
#include <exception>
#include <vector>
#include <unordered_map>
#include <set>

#define DEFAULT_SERVER_PORT "5001"

class client_data;

class address
{
	private:
		/* host + " " + port */
		static std::unordered_map<std::string, address*> host_port_to_addr;
	public:
		const std::string host;
		const std::string port;
		const int route;
		const client_data *peer;

		address(const std::string hostname, const std::string portnum, 
				int route_to_next_hop, const client_data *peername);

		static address* get(std::string);
};

std::ostream& operator <<(std::ostream& outs, const address &a);

class channel
{
	private:

		const std::string name;
		std::string topic;
		const address op;
		std::vector<address> subscribers = {};

		static std::unordered_map<std::string, channel*> name_to_channel;
	public:
		channel(const std::string channel_name, const address channel_op);

		std::string get_topic() const;

		void set_topic(std::string topic);

		std::string get_name() const;

		address get_op() const;

		std::vector<address> get_subscribers() const;

		void subscribe(address addr);

		static channel* get(std::string);
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

std::ostream &operator<<(std::ostream &out, const status_code &code);

std::istream &operator>>(std::istream &in, status_code &status);

class receive_error : public std::exception
{
	public:
		virtual const char* what() const throw();
};

enum msg_type
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
	STATUS,
	TOPIC,
	NICKRES,
};

static std::vector<std::string> command_names = {
		"CONNECT",
		"DISCONNECT",
		"NICK",
		"JOIN",
		"LEAVE",
		"GETTOPIC",
		"SETTOPIC",
		"MSG",
		"PRIVMSG",
		"QUIT",
		"HELP",
		"STATUS",
		"TOPIC",
		"NICKRES"
		};

std::ostream &operator<<(std::ostream &out, const msg_type &cmd);

std::istream &operator>>(std::istream &in, msg_type &cmd);

class client_data
{
	private:
		address addr;
		std::string nick;

		static std::unordered_map<std::string, client_data*> nick_to_client;
	public:
		client_data(address a, std::string n);

		address get_addr() const;

		std::string get_nick() const;

		void set_nick(std::string nick_name);

		static client_data* get(std::string name);
};

#endif /* DATA_HPP */
