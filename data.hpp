#ifndef DATA_HPP
#define DATA_HPP

#include <string>
#include <iostream>
#include <exception>
#include <vector>
#include <unordered_map>
#include <set>

#define DEFAULT_PORT "5001"

class channel;

class peer
{
	private:
		std::string nick;

		channel *chan;

		static std::unordered_map<std::string, peer*> nick_to_peer;

		static std::unordered_map<int, std::set<peer*>> socket_to_peers;
	public:
		const int route;

		peer(const int route_to_next_hop, std::string name);

		~peer();

		std::string get_nick() const;

		bool set_nick(std::string nick_name);

		void set_channel(channel *chan);

		channel* get_channel() const;

		static peer* get(std::string nick_name);

		static std::set<peer*> get_peers(int sock);
};

std::ostream& operator <<(std::ostream& outs, const peer &a);

class channel
{
	private:
		std::string topic;

		std::set<int> subscribers = {};

		static std::unordered_map<std::string, channel*> name_to_channel;
	public:
		const std::string name;
		const peer *op;

		channel(const std::string channel_name, const peer *channel_op);

		~channel();

		std::string get_topic() const;

		void set_topic(std::string topic);

		std::set<int> get_subscribers() const;

		void subscribe(int sockfd);

		void unsubscribe(int sockfd);

		bool check_subscribed(int sockfd);

		static channel* get(std::string);

		static std::vector<std::string> get_channel_list();
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
	nick_not_set = 303,
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
	LIST,
	LISTRES,
	GETTOPIC,
	SETTOPIC,
	MSG,
	PRIVMSG,
	QUIT,
	HELP,
	STATUS,
	CHANNEL,
	TOPIC,
	NICKRES,
};

static std::vector<std::string> command_names = {
		"CONNECT",
		"DISCONNECT",
		"NICK",
		"JOIN",
		"LEAVE",
		"LIST",
		"LISTRES",
		"GETTOPIC",
		"SETTOPIC",
		"MSG",
		"PRIVMSG",
		"QUIT",
		"HELP",
		"STATUS",
		"TOPIC",
		"NICKRES",
		"CHANNEL",
		};

std::ostream &operator<<(std::ostream &out, const msg_type &cmd);

std::istream &operator>>(std::istream &in, msg_type &cmd);


#endif /* DATA_HPP */
