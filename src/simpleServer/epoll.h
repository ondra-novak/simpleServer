
#pragma once
#include <queue>



namespace simpleServer {


class EPoll {
public:

	EPoll();
	~EPoll();




	struct Events {
		unsigned int events;
		union {
			  void *ptr;
			  int fd;
			  uint32_t u32;
			  uint64_t u64;
		} data;
		bool isTimeout() const;
		bool isInput() const;
		bool isOutput() const;
		bool isError() const;

		int getDataFd() const;
		void *getDataPtr() const;
		uint32_t getData32() const;
		uint64_t getData64() const;

		Events():events(0) {}
		Events(unsigned int events, void *data);
	};

	static const unsigned int input;
	static const unsigned int output;
	static const unsigned int error;
	static const unsigned int edgeTrigger;
	static const unsigned int oneShot;

	Events getNext(unsigned int timeout);

	bool add(int fd, unsigned int events, void *ptr);
	bool add(int fd, unsigned int events);
	bool add(int fd, unsigned int events, uint32_t u32);
	bool add(int fd, unsigned int events, uint64_t u64);

	bool update(int fd, unsigned int events, void *ptr);
	bool update(int fd, unsigned int events);
	bool update(int fd, unsigned int events, uint32_t u32);
	bool update(int fd, unsigned int events, uint64_t u64);

	bool remove(int fd);


protected:
	int fd;
	std::queue<Events> events;


};





}
