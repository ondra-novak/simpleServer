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
	typedef std::vector<std::pair<CallbackFn, EventType> > CBList;

	void asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn);
	void cancelWait(WaitFor wf, unsigned int fd);


protected:
	int epollfd;
	bool stopped;
	unsigned int regCounter;

	std::mutex workerLock;
	std::mutex regLock;
	typedef std::lock_guard<std::mutex> Sync;
	typedef std::chrono::steady_clock Clock;
	typedef Clock::time_point TimePt;
	typedef std::chrono::milliseconds Millis;

	struct TmReg;

	struct FdReg {
		CallbackFn cb[2];
		TmReg *tmPos[2];
		unsigned int fd;
		bool used;
		FdReg():used(false) {}
	};

	struct TmReg {
		TimePt time;
		TmReg **t;

		TmReg(TimePt time, TmReg **fdPos):time(time),t(t) {}
		TmReg(const TmReg &) = delete;
		TmReg(TmReg &&reg):time(reg.time),t(reg.t) {*t = this;reg.t = nullptr;}
		~TmReg() {if (t) *t = nullptr;}
	};

	struct TmRegCmp {
		bool operator()(const TmReg &a, const TmReg &b) const {
			return a.time > b.time;
		}
	};


	typedef std::priority_queue<TmReg, std::vector<TmReg>, TmRegCmp> TimeoutQueue;
	typedef std::vector<FdReg> FdRegMap;

	FdReg *findReg(unsigned int fd);
	FdReg *addReg(unsigned int fd);
	void removeTimeout(TmReg *reg);
	void addTimeout(TmReg &&reg);

private:
	void updateTimeout(FdReg* rg, unsigned int timeout, WaitFor wf);
	int epollUpdateFd(unsigned int fd, FdReg* rg);
	unsigned int calcTimeout();
	void onTimeout(CBList &cblist);
	void onEvent(int events, int fd, CBList &cblist);
	void clearNotify();
	void sendNotify();

};


}
