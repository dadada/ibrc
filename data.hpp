#ifndef DATA_HPP
#define DATA_HPP

#include <string>

#define DEFAULT_SERVER_PORT "5001"

class address
{
	public:
		const std::string host;
		const std::string port;

		address(std::string hostname, std::string portnum);
};

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

#endif /* DATA_HPP */
