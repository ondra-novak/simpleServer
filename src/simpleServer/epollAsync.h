#include <functional>
#include <mutex>
#include <chrono>
#include <queue>

#include <unordered_map>
#include "async.h"
#include "epoll.h"
#include "prioqueue.h"


namespace simpleServer {




class EPollAsync: public AbstractAsyncControl {
public:

	EPollAsync();

	///run listener (executes worker procedure)
	virtual void run() {run(nullptr);}

	///run listener (executes worker procedure)
	virtual void run(CallbackExecutor executor);

	///stop listener - sets listener to finish state exiting all workers
	virtual void stop();


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

	void asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn);
	bool cancelWait(WaitFor wf, unsigned int fd);

protected:

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
		FdReg():used(false) {
			tmPos[0] = nullptr;
			tmPos[1] = nullptr;
		}
	};

	struct TmReg {
		TimePt time;
		FdReg *fdreg;
		WaitFor wf;

		TmReg():fdreg(0) {}
		TmReg(TimePt time, FdReg *fdreg,WaitFor wf):time(time),fdreg(fdreg),wf(wf) {}
		TmReg(const TmReg &) = delete;
		TmReg(TmReg &&reg):time(reg.time),fdreg(reg.fdreg),wf(reg.wf) {
			if (fdreg) fdreg->tmPos[wf] = this;reg.fdreg = nullptr;
		}
		TmReg &operator=(TmReg &&x) {
			time = x.time;
			fdreg = x.fdreg;
			wf = x.wf;
			fdreg->tmPos[wf]= this;
			x.fdreg = nullptr;
			return *this;

		}

	};

	struct TmRegCmp {
		bool operator()(const TmReg &a, const TmReg &b) const {
			return a.time > b.time;
		}
	};


	typedef PrioQueue<TmReg,TmRegCmp> TimeoutQueue;
	typedef std::vector<FdReg> FdRegMap;

	FdReg *findReg(unsigned int fd);
	FdReg *addReg(unsigned int fd);
	void removeTimeout(TmReg *&reg);
	void addTimeout(TmReg &&reg);

private:
	void updateTimeout(FdReg* rg, unsigned int timeout, WaitFor wf);
	bool epollUpdateFd(FdReg* rg);
	unsigned int calcTimeout();
	void onTimeout(CBList &cblist);
	void onEvent(const EPoll::Events &ev, CBList &cblist);
	void clearNotify();
	void sendNotify();

	EPoll epoll;
	bool stopped;
	TimeoutQueue tmoutQueue;
	FdRegMap fdRegMap;
	int notifyPipe[2];
};


}
