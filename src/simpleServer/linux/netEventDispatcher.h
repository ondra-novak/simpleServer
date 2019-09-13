#pragma once

#include <poll.h>
#include <chrono>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <utility>



#include "../asyncProvider.h"
#include "../stringview.h"
#include "async.h"



namespace simpleServer {

class LinuxEventDispatcher: public AbstractStreamEventDispatcher {
public:
	LinuxEventDispatcher();
	virtual ~LinuxEventDispatcher() noexcept;

	virtual void runAsync(const AsyncResource &resource, int timeout, CompletionFn &&complfn) override;

	virtual void runAsync(CustomFn &&completion) override;

	virtual void cancel(const AsyncResource &resource) override;


	virtual Task wait() override;


	///returns true, if the listener doesn't contain any asynchronous task
	virtual bool empty() const override;

	virtual void stop() override;

	virtual unsigned int getPendingCount() const override;

protected:

	typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;
	typedef AsyncState WaitResult;
	typedef std::vector<pollfd> FDMap;


	struct FDExtra {
		CompletionFn completionFn;
		TimePoint timeout;
	};

	struct RegReq {
		AsyncResource ares;
		FDExtra extra;
	};

	typedef std::vector<FDExtra> FDExtraMap;
	FDMap fdmap;
	FDExtraMap fdextramap;


	void addResource(const RegReq &req);
	void deleteResource(int index);
	void addIntrWaitHandle();
	Task checkEvents(const TimePoint &now, bool finish);
	Task findAndCancel(const AsyncResource &res);

	int intrHandle;
	int intrWaitHandle;

	bool exitFlag;
	TimePoint nextTimeout =  TimePoint::max();
	int last_checked = 0;


	mutable std::mutex queueLock;
	std::queue<RegReq> queue;
	void sendIntr();

	Task cleanup();
	Task runQueue();

};

} /* namespace simpleServer */

