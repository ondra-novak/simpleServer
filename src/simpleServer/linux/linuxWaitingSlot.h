#pragma once

#include "linuxWaitingSlot.h"

#include <poll.h>
#include <chrono>
#include <mutex>
#include <queue>
#include <utility>



#include "../asyncProvider.h"
#include "../stringview.h"
#include "async.h"



namespace simpleServer {

class LinuxEventListener: public AbstractStreamEventDispatcher {
public:
	LinuxEventListener();
	virtual ~LinuxEventListener();

	virtual void receive(const AsyncResource &resource,
			MutableBinaryView buffer,
			int timeout,
			Callback completion) override;


	virtual void send(const AsyncResource &resource,
			BinaryView buffer,
			int timeout,
			Callback completion) override;

	virtual Task waitForEvent() override;


	virtual void cancelWait() override;

protected:

	typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;


	enum WaitResult {
		wrEvent,
		wrTimeout,
	};

	enum Command {
		cmdExit,
		cmdQueue
	};
	typedef std::function<void(WaitResult)> TaskFunction;


	struct TaskInfo {
		TaskFunction taskFn;
		TimePoint timeout;
		void swap(TaskInfo &other) {
			std::swap(taskFn, other.taskFn);
			std::swap(timeout, other.timeout);

		}
	};


	typedef std::vector<pollfd> FDMap;
	typedef std::vector<TaskInfo> TaskMap;

	FDMap fdmap;
	TaskMap taskMap;
	TimePoint nextTimeout =  TimePoint::max();
	int nextTimeoutPos = -1;


	void removeTask(int index);

	int intrHandle;
	int intrWaitHandle;


	typedef std::pair<pollfd, TaskInfo> TaskAddRequest;

	void addTask(const TaskAddRequest &req);


	std::mutex queueLock;
	std::queue<TaskAddRequest> queue;


	void sendIntr(Command cmd);

	template<typename Fn>
	void addTaskToQueue(int fd, const Fn &fn, int timeout, int event);





};

} /* namespace simpleServer */


