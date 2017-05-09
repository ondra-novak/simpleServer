#include <functional>
#include <mutex>
#include <chrono>
#include <queue>
#include <unordered_map>
#include "async.h"


namespace simpleServer {


class EPollAsync: public AbstractAsyncControl {
public:

	EPollAsync();

	///run listener (executes worker procedure)
	virtual void run();

	///stop listener - sets listener to finish state exiting all workers
	virtual void stop();

	~EPollAsync();


	enum EventType {
		///Detected event on the socket
		etNetEvent,
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

	void asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn);


protected:
	int epollfd;
	bool stopped;
	unsigned int regCounter;

	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;

	typedef std::chrono::steady_clock::time_point TimePt;

	struct FdReg {
		CallbackFn cb[2];
		unsigned int regCounter[2];

	};

	typedef std::unordered_map<unsigned int, FdReg> FdMap;

	struct TmReg {
		TimePt time;
		WaitFor op;
		unsigned int fd;

		TmReg(TimePt time,WaitFor op,unsigned int fd):time(time),op(op),fd(fd) {}
	};

	struct TmRegCmp {
		bool operator()(const TmReg &a, const TmReg &b) const {
			return a.time > b.time;
		}
	};

	typedef std::priority_queue<TmReg, std::vector<TmReg>, TmRegCmp> TimeoutQueue;



	TmReg popTimeout();
	void pushTimeout(const TmReg &tm);



};


}
