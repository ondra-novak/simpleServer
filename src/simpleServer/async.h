#pragma once
#include "refcnt.h"

namespace simpleServer {



class AbstractAsyncControl: public RefCntObj {
public:


	///run listener (executes worker procedure)
	virtual void run() = 0;

	///stop listener - sets listener to finish state exiting all workers
	virtual void stop() = 0;

	virtual ~AbstractAsyncControl() {}


};

typedef RefCntPtr<AbstractAsyncControl> PAsyncControl;



class AsyncControl {
public:


	AsyncControl(PAsyncControl owner):owner(owner) {}

	///Start new async control object
	static AsyncControl start();
	///Create new async control object, but don't start the thread
	static AsyncControl create();
	///Get singleton obejct - starts thread if it is necesery
	/**
	 * @note do not stop singleton - there is no way to restart it
	 * @return
	 */
	static AsyncControl getSingleton();

	///run listener (executes worker procedure)
	void run() const {owner->run();}

	///stop listener - sets listener to finish state exiting all workers
	void stop() const {owner->stop();}


	~AsyncControl() {
		if (owner != nullptr) stop();
	}

	PAsyncControl getHandle() const {return owner;}
protected:

	PAsyncControl owner;

};





}



