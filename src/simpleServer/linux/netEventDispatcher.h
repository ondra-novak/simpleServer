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

	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) override;

	virtual void runAsync(const CompletionFn &completion) override;


	virtual Task waitForEvent() override;


	///returns true, if the listener doesn't contain any asynchronous task
	virtual bool empty() const override;
	///Move all asynchronous tasks to different listener (must be the same type)
	virtual void moveTo(AsyncProvider target) override;

	virtual void stop() override;

	virtual unsigned int getPendingCount() const override;

protected:

	typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

	typedef AsyncState WaitResult;




	struct TaskInfo {
		CompletionFn taskFn;
		TimePoint timeout;
		int org_timeout;
		void swap(TaskInfo &other) {
			std::swap(taskFn, other.taskFn);
			std::swap(timeout, other.timeout);

		}
	};


	typedef std::vector<pollfd> FDMap;
	typedef std::pair<int,int> RKey;
	struct HashRKey {
	public:
		std::size_t operator()(const RKey &key) const;
	};
	typedef std::unordered_map<RKey, TaskInfo, HashRKey> TaskMap;

	FDMap fdmap;
	TaskMap taskMap;
	TimePoint nextTimeout =  TimePoint::max();
	AsyncProvider moveToProvider;;
	bool exitFlag;


	void removeTask(int index, TaskMap::iterator &it);

	int intrHandle;
	int intrWaitHandle;


	typedef std::pair<pollfd, TaskInfo> TaskAddRequest;

	Task addTask(const TaskAddRequest &req);


	mutable std::mutex queueLock;
	std::queue<TaskAddRequest> queue;


	void sendIntr();

	void addTaskToQueue(int fd, const CompletionFn &fn, int timeout, int event);


	///clears all asynchronous tasks
	void epilog();


	Task runQueue();

	virtual void onRelease() override {
		taskMap.clear();
		moveToProvider = nullptr;
		AbstractStreamEventDispatcher::onRelease();

	}

};

} /* namespace simpleServer */

