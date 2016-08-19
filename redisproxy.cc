//
// Created by zhanghu on 16/8/18.
//

#include <resp/result.hpp>
#include <iostream>
#include <resp/decoder.hpp>
#include <resp/encoder.hpp>
#include <redox/client.hpp>
#include <redis3m/redis3m.hpp>

#include "redisproxy.h"

extern redox::Redox rediscache;
extern redox::Redox redisdb;
extern redis3m::connection::ptr_t rediscache2;
extern redis3m::connection::ptr_t redisdb2;

resp::encoder<resp::buffer> enc;
resp::decoder dec;

void handle_get(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				const resp::unique_array<resp::unique_value> &array);

void handle_set(std::list<Buffer *, std::allocator<Buffer *>> &list,
				const resp::unique_array<resp::unique_value>& array);

void add_reply_error(std::list<Buffer *, std::allocator<Buffer *>> &write_queue);

void add_reply(std::list<Buffer *, std::allocator<Buffer *>> &write_queue);

void add_reply_succ(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
					const std::string &get_from_redis);

void do_get_from_backend_db(std::string basic_string, std::list<Buffer *,
	std::allocator<Buffer *>> &list);

void handle_sadd(std::list<Buffer *, std::allocator<Buffer *>> &list,
				 resp::unique_array<resp::unique_value>& array);

bool check_if_key_in_cache_scard(std::string &key);

int do_sadd_to_db(resp::unique_array<resp::unique_value>& array);

int do_sadd(resp::unique_array<resp::unique_value>& array, redox::Redox &cache_or_db);

int do_sadd_to_cache(resp::unique_array<resp::unique_value>& array);

void dump_key_from_db_to_cache(std::string key);

void DEBUG(const char *string);

void add_reply(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, int ret);

void do_set_cache(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &key,
				  const std::string &value);

void
do_set(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &key, const std::string &value,
	   redox::Redox &cache_or_db);

void do_set_db(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, std::string key, std::string value);

void handle_scard(std::list<Buffer *, std::allocator<Buffer *>> &list, resp::unique_array<resp::unique_value> array);

int do_scard(std::string& key, redox::Redox& cache_or_db);

void add_reply_int(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, int reply);

void handle_smembers(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
					 resp::unique_array<resp::unique_value> array);

const std::vector<std::string> do_smembers(std::string &key, redox::Redox &cache_or_db);

void add_reply_array(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
					 const std::vector<std::string>& reply);

void handle_zadd(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				 resp::unique_array<resp::unique_value> &array);

redis3m::reply do_zadd(resp::unique_array<resp::unique_value> &array, redis3m::connection::ptr_t ptr);

bool check_if_key_in_cache_zcard(std::string& key) ;

char temp_buffer[BUFFERLEN];

void request_proxy(const char *buffer, ssize_t nread,
				   std::list<Buffer *> &write_queue) {
	resp::result result = dec.decode(buffer, nread);
	resp::unique_value resp_value = result.value();
//			std::cout << "decode: " << result.type() << std::endl;

	if (resp_value.type() == resp::ty_array) {
		resp::unique_array<resp::unique_value> array = resp_value.array();

		const resp::buffer &command = array[0].bulkstr();
		if (command == "get") {
			handle_get(write_queue, array);
		} else if (command == "set") {
			handle_set(write_queue, array);
		} else if (command == "sadd") {
			handle_sadd(write_queue, array);
		} else if (command == "scard") {
			handle_scard(write_queue, array);
		} else if (command == "smembers") {
			handle_smembers(write_queue, array);
		} else if (command == "zadd") {
			handle_zadd(write_queue, array);
		}
	}
}

void handle_zadd(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				 resp::unique_array<resp::unique_value> &array) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());

	// 1. check if the key is in cache
	bool existed_in_cache = check_if_key_in_cache_zcard(key);

	// 2. if the key existed in cache, send command to cache and db
	;
	if (existed_in_cache) {
		redis3m::reply reply = do_zadd(array, rediscache2);
		if (reply.integer() > 0) {
			reply = do_zadd(array, redisdb2);
		}
		add_reply_int(write_queue, reply.integer());
	} else {
		redis3m::reply reply = do_zadd(array, redisdb2);
		dump_key_from_db_to_cache(key);
		add_reply_int(write_queue, reply.integer());
	}

}

redis3m::reply do_zadd(resp::unique_array<resp::unique_value> &array, redis3m::connection::ptr_t cache_or_db) {
	redis3m::command command("ZADD");
	for (int i = 1; i < array.size(); i++) {
		std::__1::string element(array[i].bulkstr().data(), array[i].bulkstr().size());
		command << element;
	}

	redis3m::reply reply = cache_or_db->run(command);
	std::__1::cout << reply.str() << std::__1::endl;
	return reply;
}

void handle_smembers(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
					 resp::unique_array<resp::unique_value> array) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());

	// 1. check if the key is in cache
	bool existed_in_cache = check_if_key_in_cache_scard(key);

	if (existed_in_cache) {
		// 2. read from cache
		auto reply = do_smembers(key, rediscache);
		add_reply_array(write_queue, reply);
	} else {
		dump_key_from_db_to_cache(key);
		auto reply = do_smembers(key, rediscache);
		add_reply_array(write_queue, reply);
	}

}


int forward_position(const char *pbuf, int position) {
	position += strlen(pbuf);
	if (position >= BUFFERLEN) {
		assert(false);
	}
	return position;
}

void add_reply_array(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
					 const std::vector<std::string>& reply) {
	// form a reply buffer
	char buffer[BUFFERLEN]; // FIXME: dynamic buffer
	char *pbuf = buffer;
	int position = 0;
	snprintf(pbuf + position, BUFFERLEN - position - 1, "*%d\r\n", (int)reply.size());

	position = forward_position(pbuf, position);

	for (auto it = reply.begin(); it != reply.end(); it++) {
		snprintf(pbuf + position, BUFFERLEN - position - 1,
				 "$%d\r\n%s\r\n", (int)it->length(), it->c_str());
		position = forward_position(pbuf + position, position);
	}

	// push the buffer to write queue
	write_queue.push_back(new Buffer(buffer, strlen(buffer)));
}

const std::vector<std::string> do_smembers(std::string &key, redox::Redox &cache_or_db) {
	redox::Command<std::vector<std::string>> &c =
		cache_or_db.commandSync<std::vector<std::string>>({"SMEMBERS", key});

	const std::vector<std::string> &reply = c.reply();
//	for (auto it = reply.begin(); it != reply.end(); it++) {
//		std::cout << *it << std::endl;
//	}

	return reply;
}

void handle_scard(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				  resp::unique_array<resp::unique_value> array) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());
	int scard = 0;

	// 1. check if the key is existed in cache
	bool existed_in_cache = check_if_key_in_cache_scard(key);

	// 2. if the key is in cache, get scard from cache
	if (existed_in_cache) {
		scard = do_scard(key, rediscache);
	} else {
		scard = do_scard(key, redisdb);
	}

	if (scard < 0) {
		// error handling

		scard = 0;
	}

	add_reply_int(write_queue, scard);
}

void add_reply_int(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, int reply) {
	char buffer[128];

	snprintf(buffer, 128, ":%d\r\n", reply);

	write_queue.push_back(new Buffer(buffer, strlen(buffer)));
}

int do_scard(std::string& key, redox::Redox& cache_or_db) {
	redox::Command<int> &c =
		cache_or_db.commandSync<int>({"SCARD", key});
	if (c.ok()) {
		return c.reply();
	}

	return -1;
}

void handle_sadd(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				 resp::unique_array<resp::unique_value>& array) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());

	// 1. check if the key is existed in cache
	bool existed_in_cache = check_if_key_in_cache_scard(key);

	// 2. if the key is existed in cache, sadd to db and cache
	if (existed_in_cache) {
		DEBUG("existed_in_cache");
		int ret = do_sadd_to_cache(array);

		// if the value is in the set already, it's no need to add to db anymore
		// it will gain better performance if we ignore the add operation to db
		if (ret > 0) {
			ret = do_sadd_to_db(array);
		}

		add_reply(write_queue, ret);
		return;
	}

	// 3. if the key is not existed in cache, sadd to db first, then dump the data to cache
	int ret = do_sadd_to_db(array);
	dump_key_from_db_to_cache(key);

	add_reply(write_queue, ret);
}

void add_reply(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, int ret) {
	char buf[128];
	snprintf(buf, 128, ":%d\r\n", ret);
	write_queue.push_back(new Buffer(buf, strlen(buf)));
}

void DEBUG(const char * info) {
//	std::cout << info << std::endl;
}

void dump_key_from_db_to_cache(std::string key) {
	DEBUG("dump_key_from_db_to_cache");
	redox::Command<std::string> &c =
		redisdb.commandSync<std::string>({"DUMP", key});
	if (c.ok()) {
		rediscache.commandSync<std::string>({"RESTORE", key, "0", c.reply()});
	}
}

int do_sadd_to_cache(resp::unique_array<resp::unique_value>& array) {
	DEBUG("do_sadd_to_cache");
	return do_sadd(array, rediscache);
}

int do_sadd_to_db(resp::unique_array<resp::unique_value>& array) {
	DEBUG("do_sadd_to_db");
	return do_sadd(array, redisdb);
}

int do_sadd(resp::unique_array<resp::unique_value>& array, redox::Redox& cache_or_db) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());
	std::string value(array[2].bulkstr().data(), array[2].bulkstr().size());

	redox::Command<int> &c =
		cache_or_db.commandSync<int>({"SADD", key, value});
	if (c.ok()) {
		// return
		return c.reply();
	} else {
		// return error
	}

	return 0;
}

bool check_if_key_in_cache_scard(std::string &key) {
	// FIXME:
	redox::Command<int> &c =
		rediscache.commandSync<int>({"SCARD", key});
	if (c.ok()) {
		if (c.reply() > 0) {
			return true;
		}
	}
	return false;
}

bool check_if_key_in_cache_zcard(std::string& key) {
	// FIXME:
	redox::Command<int> &c =
		rediscache.commandSync<int>({"ZCARD", key});
	if (c.ok()) {
		if (c.reply() > 0) {
			return true;
		}
	}
	return false;
}

void handle_get(std::list<Buffer *, std::allocator<Buffer *>> &write_queue,
				const resp::unique_array<resp::unique_value> &array) {
	std::string key(array[1].bulkstr().data(), array[1].bulkstr().size());

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
		do_set_db(write_queue, key, value);

		// 2. write to redis
		do_set_cache(write_queue, key, value);

		// async manner
		// 1. write to redis

		// 2. enqueue write request to backend db

	} catch (std::runtime_error e) {
		add_reply_error(write_queue);
	}
}

void do_set_db(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, std::string key, std::string value) {
	do_set(write_queue, key, value, redisdb);
}

void do_set_cache(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &key,
				  const std::string &value) {
	do_set(write_queue, key, value, rediscache);
}

void
do_set(std::list<Buffer *, std::allocator<Buffer *>> &write_queue, const std::string &key, const std::string &value,
	   redox::Redox &cache_or_db) {
	bool result = cache_or_db.set(key, value);
	if (result) {
			add_reply_ok(write_queue);
		} else {
			add_reply_error(write_queue);
		}
}
