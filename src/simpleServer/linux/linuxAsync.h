#pragma once
#include <functional>
#include <utility>
#include <vector>
#include "../async.h"

namespace simpleServer {


class LinuxAsync: public AbstractAsyncControl {
public:

	enum EventType {
		///Detected event on the socket
		etReadEvent,
		///Detected event on the socket
		etWriteEvent,
		///No event detected before timeout
		etTimeout,
		///Error event detected on the socket
		/** Called in exception handler, you can receive exception as std::current_exception() */
		etError
	};

	enum WaitFor {
		wfRead=0,
		wfWrite=1
	};


	typedef std::function<void(EventType)> CallbackFn;
	typedef std::pair<CallbackFn, EventType> CallbackWithArg;
	typedef std::vector<CallbackWithArg> CBList;

	virtual void asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) = 0;
	virtual bool cancelWait(WaitFor wf, unsigned int fd) = 0;

	static void checkSocketError(int fd);


};

}
