//
// Created by zhanghu on 16/8/18.
//

#include <resp/result.hpp>
#include <iostream>
#include <resp/decoder.hpp>
#include <resp/encoder.hpp>
#include <redox/client.hpp>

#include "redisproxy.h"

extern redox::Redox rediscache;
extern redox::Redox redisdb;

resp::encoder<resp::buffer> enc;
resp::decoder dec;

void handle_get(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				const resp::unique_array<resp::unique_value> &arr);

void handle_set(std::list<Buffer *, std::allocator<Buffer *>> &list,
				const resp::unique_array<resp::unique_value>& array);

void add_reply_error(std::list<Buffer *, std::allocator<Buffer *>> &write_queue);

void add_reply(std::list<Buffer *, std::allocator<Buffer *>> &write_queue);

void add_reply_succ(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &get_from_redis);

void do_get_from_backend_db(std::string basic_string, std::list<Buffer *, std::allocator<Buffer *>> &list);

char temp_buffer[1024];

void request_proxy(const char *buffer, ssize_t nread,
				   std::list<Buffer *> &write_queue) {
	resp::result result = dec.decode(buffer, nread);
	resp::unique_value resp_value = result.value();
//			std::cout << "decode: " << result.type() << std::endl;

	if (resp_value.type() == resp::ty_array) {
		resp::unique_array<resp::unique_value> arr = resp_value.array();

		if (arr[0].bulkstr() == "get") {
			handle_get(write_queue, arr);
		} else if (arr[0].bulkstr() == "set") {
			handle_set(write_queue, arr);
		}
	}
}

void handle_get(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				const resp::unique_array<resp::unique_value> &arr) {
	std::string key(arr[1].bulkstr().data(), arr[1].bulkstr().size());

	try {
		// read from redis
		std::string get_from_redis = rediscache.get(key);
		add_reply_succ(write_queue, get_from_redis);

	} catch (std::runtime_error e) {
//		std::cerr << "get from cache: " << e.what() << std::endl;
		// TODO: read failed, try to read from backend db
		do_get_from_backend_db(key, write_queue);
//		add_reply_error(write_queue);
	}
}

void do_get_from_backend_db(std::string key, std::list<Buffer *, std::allocator<Buffer *>> &write_queue) {
	try {
		std::string get_from_backend_db = redisdb.get(key);
		add_reply_succ(write_queue, get_from_backend_db);
	} catch (std::runtime_error e) {
//		std::cerr << "get from backend db: " << e.what() << std::endl;
		add_reply_error(write_queue);
	}

}

void add_reply_succ(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &get_from_redis) {// format reply
	snprintf(temp_buffer, sizeof(temp_buffer),
				 "$%d\r\n%s\r\n",
				 (int) get_from_redis.size(),
				 get_from_redis.c_str());

//		std::__1::cout << "reply: " << temp_buffer << std::__1::endl;

	// append to reply queue
	add_reply(write_queue);
}

void add_reply(std::list<Buffer *, std::allocator<Buffer *>> &write_queue) {
	write_queue.push_back(
			new Buffer((const char *) temp_buffer, (ssize_t) strlen(temp_buffer)));
}

void add_reply_error(std::list<Buffer *, std::allocator<Buffer *>> &write_queue) {
	write_queue.push_back(
			new Buffer((const char *) "-Error not found\r\n", (ssize_t) 18));
}

void add_reply_ok(std::list<Buffer *, std::allocator<Buffer *>> &write_queue) {
	write_queue.push_back(
		new Buffer((const char *) "+OK\r\n", (ssize_t) 5));
}

void handle_set(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				const resp::unique_array<resp::unique_value> &array) {
	try {
		std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());
		std::string value(array[2].bulkstr().data(), array[2].bulkstr().size());

		// sync manner
		// 1. write to backend db first

		// 2. write to redis
		bool result = rediscache.set(key, value);
		if (result) {
			add_reply_ok(write_queue);
		} else {
			add_reply_error(write_queue);
		}

		// async manner
		// 1. write to redis

		// 2. enqueue write request to backend db

	} catch (std::runtime_error e) {
		add_reply_error(write_queue);
	}
}
