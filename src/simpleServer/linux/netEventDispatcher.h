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

	virtual void runAsync(const CustomFn &completion) override;


	virtual Task waitForEvent() override;


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
	Task checkEvents(const TimePoint &now);

	int intrHandle;
	int intrWaitHandle;

	bool exitFlag;
	TimePoint nextTimeout =  TimePoint::max();
	int last_checked = 0;


	mutable std::mutex queueLock;
	std::queue<RegReq> queue;
	void sendIntr();

	Task cleanup();

/*




	struct TaskInfo {
		CompletionFn taskFn;
		TimePoint timeout;
		int org_timeout;
		void swap(TaskInfo &other) {
			std::swap(taskFn, other.taskFn);
			std::swap(timeout, other.timeout);

		}
	};


	struct HashRKey {
	public:
		std::size_t operator()(const RKey &key) const;
		bool operator()(const RKey &key,const RKey &key) const;
	};
	typedef std::unordered_map<AsyncResource, TaskInfo, HashRKey, HashRKey> TaskMap;

	FDMap fdmap;
	TaskMap taskMap;
	TimePoint nextTimeout =  TimePoint::max();
	bool exitFlag;


	void removeTask(int index, TaskMap::iterator &it);



	typedef std::pair<pollfd, TaskInfo> TaskAddRequest;

	Task addTask(const TaskAddRequest &req);


	mutable std::mutex queueLock;
	std::queue<TaskAddRequest> queue;



	void addTaskToQueue(int fd, const CompletionFn &fn, int timeout, int event);


	///clears all asynchronous tasks
	void epilog();


	Task runQueue();

*/
};

} /* namespace simpleServer */

