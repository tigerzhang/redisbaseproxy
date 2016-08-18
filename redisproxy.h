//
// Created by zhanghu on 16/8/18.
//

#ifndef REDISBASEPROXY_REDISPROXY_H
#define REDISBASEPROXY_REDISPROXY_H

#include <list>

class Buffer;

void request_proxy(const char *buffer, ssize_t nread,
				   std::list<Buffer *>& write_queue);

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//
class Buffer {
public:
	char *data;
	ssize_t len;
	ssize_t pos;

	Buffer(const char *bytes, ssize_t nbytes) {
		pos = 0;
		len = nbytes;
		data = new char[nbytes];
		memcpy(data, bytes, nbytes);
	}

	virtual ~Buffer() {
		delete[] data;
	}

	char *dpos() {
		return data + pos;
	}

	ssize_t nbytes() {
		return len - pos;
	}
};


#endif //REDISBASEPROXY_REDISPROXY_H
