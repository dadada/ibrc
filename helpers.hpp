#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <istream>
#include <unistd.h>
#include <queue>
#include <set>
#include <sys/epoll.h>
#include <unordered_map>

#define MAX_EVENTS 30
#define BUFLEN 2056 // max size of message
#define EPOLLFLAGS EPOLLIN | EPOLLET | EPOLLRDHUP

int set_socket_opt(int sockfd, int opt);

int unset_socket_opt(int sockfd, int opt);

int set_socket_blocking(int sockfd);

int set_socket_non_blocking(int sockfd);

/* read socket to msgbuf and process messages */
ssize_t sockfd_in(int sock, std::queue<std::string> &in_queue);

/* read socket to msgbuf and process messages */
int sockfd_out(int sock, std::queue<std::string> &out_queue);

/* manages connections with epoll */
class connection_manager
{
	private:
		/* input messages from network per socket */
		std::unordered_map<int, std::queue<std::string>> in_queues;

		/* output messages to network per socket */
		std::unordered_map<int, std::queue<std::string>> out_queues;

		/* set of all fds */
		std::set<int> socks;

		int epollfd;

		struct epoll_event* events;

		int count_events;

		int current_event;

		int next_event_pos;

		bool epoll_mod(int sock, uint32_t event_flags);

	public:
		connection_manager();

		~connection_manager();

		/* wait for next event and write events to *events*
		 * returns number of events */
		int wait_events();

		/* gets next event from events */
		bool next_event(struct epoll_event &ev);

		/* adds a socket on wich to listen for incomming connections
		 * returns the new socket
		 */
		int add_accepting(std::string port);

		/* opens a new connection to host:port */
		int create_connection(std::string host, std::string port);

		/* accepts and adds a client socket, returns the new socket*/
		int accept_client(int sock);

		/* removes a client socket */
		bool remove_socket(int sock);

		/* stops polling socket for write */
		bool pause_write(int socket);

		/* resumes polling socket for write */
		bool continue_write(int socket);

		/* add message to the output queue for socket */
		bool add_message(int sock, std::string message);

		/* fetch next message from incoming queue of socket */
		bool fetch_message(int sock, std::string &msg);

		/* receives new messages from sock and places them in the queue */
		bool receive_messages(int sock);

		/* sends as many messages as possible */
		bool send_messages(int sock);

		/* adds a socket to the poll set */
		int add_socket(int sockfd, int flags);
};
#endif /* HELPERS_HPP */
