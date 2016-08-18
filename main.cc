#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ev++.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <errno.h>
#include <list>
#include <resp/all.hpp>
#include <iostream>
#include <redox.hpp>
#include "redisproxy.h"

redox::Redox rediscache;
redox::Redox redisdb;

//
//   A single instance of a non-blocking Echo handler
//
class EchoInstance {
private:
	ev::io io;
	static int total_clients;
	int sfd;

	// Buffers that are pending write
	std::list<Buffer *> write_queue;

	// Generic callback
	void callback(ev::io &watcher, int revents) {
		if (EV_ERROR & revents) {
			perror("got invalid event");
			return;
		}

		if (revents & EV_READ)
			read_cb(watcher);

		if (revents & EV_WRITE)
			write_cb(watcher);

		if (write_queue.empty()) {
			io.set(ev::READ);
		} else {
			io.set(ev::READ | ev::WRITE);
		}
	}

	// Socket is writable
	void write_cb(ev::io &watcher) {
		if (write_queue.empty()) {
			io.set(ev::READ);
			return;
		}

		Buffer *buffer = write_queue.front();

		ssize_t written = write(watcher.fd, buffer->dpos(), buffer->nbytes());
		if (written < 0) {
			perror("read error");
			return;
		}

		buffer->pos += written;
		if (buffer->nbytes() == 0) {
			write_queue.pop_front();
			delete buffer;
		}
	}

	// Receive message from client socket
	void read_cb(ev::io &watcher) {
		char buffer[1024];
		memset(buffer, 0, sizeof(buffer));

		ssize_t nread = recv(watcher.fd, buffer, sizeof(buffer), 0);

		if (nread < 0) {
			perror("read error");
			return;
		}

		if (nread == 0) {
			// Gack - we're deleting ourself inside of ourself!
//			delete this;
		} else {
			// Send message bach to the client
//			write_queue.push_back(new Buffer(buffer, nread));
			// decode redis request
			request_proxy(buffer, nread, write_queue);
		}
	}

	// effictivly a close and a destroy
	virtual ~EchoInstance() {
		// Stop and free watcher if client socket is closing
		io.stop();

		close(sfd);

		printf("%d client(s) connected.\n", --total_clients);
	}

public:
	EchoInstance(int s) : sfd(s) {
		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);

		printf("Got connection\n");
		total_clients++;

		io.set<EchoInstance, &EchoInstance::callback>(this);

		io.start(s, ev::READ);
	}
};

class EchoServer {
private:
	ev::io io;
	ev::sig sio;
	int s;

public:

	void io_accept(ev::io &watcher, int revents) {
		if (EV_ERROR & revents) {
			perror("got invalid event");
			return;
		}

		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_sd = accept(watcher.fd, (struct sockaddr *) &client_addr, &client_len);

		if (client_sd < 0) {
			perror("accept error");
			return;
		}

		EchoInstance *client = new EchoInstance(client_sd);
	}

	static void signal_cb(ev::sig &signal, int revents) {
		signal.loop.break_loop();
	}

	EchoServer(int port) {
		printf("Listening on port %d\n", port);

		struct sockaddr_in addr;

		s = socket(PF_INET, SOCK_STREAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
			perror("bind");
		}

		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);

		listen(s, 5);

		io.set<EchoServer, &EchoServer::io_accept>(this);
		io.start(s, ev::READ);

		sio.set<&EchoServer::signal_cb>();
		sio.start(SIGINT);
	}

	virtual ~EchoServer() {
		shutdown(s, SHUT_RDWR);
		close(s);
	}
};

int EchoInstance::total_clients = 0;

int main(int argc, char **argv) {
	int port = 8192;

	if (argc > 1)
		port = atoi(argv[1]);

	if (!rediscache.connect("localhost", 6379)) return 1;
	if (!redisdb.connect("localhost", 6380))
		return 2;

	ev::default_loop loop;
	EchoServer echo(port);

	loop.run(0);

	rediscache.disconnect();
	return 0;
}

