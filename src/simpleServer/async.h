#pragma once
#include "refcnt.h"

namespace simpleServer {



class AbstractAsyncControl: public RefCntObj {
public:

	typedef std::function<void()> Callback;
	typedef std::function<void(Callback)> CallbackExecutor;

	///run listener (executes worker procedure)
	virtual void run() = 0;


	////run listener (executes worker procedure) specify function which executes callbacks
	virtual void run(CallbackExecutor executor) = 0;

	///stop listener - sets listener to finish state exiting all workers
	virtual void stop() = 0;

	virtual ~AbstractAsyncControl() {}


};

typedef RefCntPtr<AbstractAsyncControl> PAsyncControl;

enum AsyncState {
	///asynchronous operation completted successfuly
	asyncOK,
	///asynchronous operation completted successfuly, result is EOF
	asyncEOF,
	///asynchronous operation timeouted
	asyncTimeout,
	///asynchronous operation failed with and error
	/**
	 * The error state is stored as current exeception. You can use std::current_exception to determine, which error happened
	 */
	asyncError
};



class AsyncControl {
public:

	typedef AbstractAsyncControl::CallbackExecutor CallbackExecutor;


	AsyncControl(PAsyncControl owner):owner(owner) {}

	///Start new async control object
	static AsyncControl start();
	///Start new async control object
	static AsyncControl start(CallbackExecutor executor);
	///Create new async control object, but don't start the thread
	static AsyncControl create();
	///Create fake async control object which performs everthing synchronous
	/** Result object implements interface on reduced resources but all operations are performed in current thread
	 * by synchronous manner
	 */
	static AsyncControl createSync();

	///Get singleton obejct - starts thread if it is necesery
	/**
	 * @note do not stop singleton - there is no way to restart it
	 * @return
	 */
	static AsyncControl getSingleton();

	///run listener (executes worker procedure)
	void run() const {owner->run();}

	///run listener (executes worker procedure)
	void run(const CallbackExecutor &executor) const {owner->run(executor);}


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



