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

	virtual Callback waitForEvent() override;


	virtual void cancelWait() override;

protected:

	struct TaskInfo {
		BinaryView


	};




};

} /* namespace simpleServer */


