#pragma once

#include "../asyncProvider.h"

namespace simpleServer {

class LinuxWaitingSlot: public AbstractWaitingSlot {
public:
	LinuxWaitingSlot();
	virtual ~LinuxWaitingSlot();

	virtual void read(const AsyncResource &resource,
			MutableBinaryView buffer,
			int timeout,
			Callback completion) override;


	virtual void write(const AsyncResource &resource,
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
	typedef std::function<void(TaskState)> TaskFunction;


	struct TaskInfo {
		TaskFunction taskFn;
		TimePoint timeout;
	};

	typedef std::vector<pollfd> FDMap;
	typedef std::vector<TaskInfo> TaskMap;

	FDMap fdmap;
	TaskMap taskMap;
	TimePoint nextTimeout =  TimePoint::max;
	int nextTimeoutPos = -1;


	void removeTask(int index);

	int intrHandle;
	int intrWaitHandle;


	typedef std::pair<pollfd, TaskInfo> TaskAddRequest;

	void addTask(const TaskAddReques &req);


	std::mutex queueLock;
	std::queue<TaskAddRequest> queue;


	void addServiceTask(int fd);
	void sendIntr(Command cmd);

	template<typename Fn>
	void addTaskToQueue(int fd, const Fn &fn, int timeout, int event);





};

} /* namespace simpleServer */


