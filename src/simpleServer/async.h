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



///Provides context of asynchronous operation
/**
 * You need to create this object to provide context of asynchronous operation. You can execute single thread
 * that handles async operations. You can also start multiple threads,each will be used to assign a thread to the callback
 * function. You can also define executor, which receives the callback function to execute
 * and the executor can create/choose a thread where the function is executed
 */
class AsyncDispatcher {
public:

	typedef AbstractAsyncControl::CallbackExecutor CallbackExecutor;


	AsyncDispatcher(PAsyncControl owner):owner(owner) {}

	///Start new async control object
	static AsyncDispatcher start();
	///Start new async control object
	static AsyncDispatcher start(CallbackExecutor executor);
	///Create new async control object, but don't start the thread
	static AsyncDispatcher create();
	///Create fake async control object which performs everthing synchronous
	/** Result object implements interface on reduced resources but all operations are performed in current thread
	 * by synchronous manner
	 *
	 * @param maxTimeout specifies max allowed timeout (even if the caller requests infinity timeout).
	 * If the timeout is reached, standard exception is generated (it is not send through the callback function);
	 */
	static AsyncDispatcher createSync(unsigned int maxTimeout);

	///Get singleton obejct - starts thread if it is necesery
	/**
	 * @note do not stop singleton - there is no way to restart it
	 * @return
	 */
	static AsyncDispatcher getSingleton();

	///run listener (executes worker procedure)
	void run() const {owner->run();}

	///run listener (executes worker procedure)
	void run(const CallbackExecutor &executor) const {owner->run(executor);}


	///stop listener - sets listener to finish state exiting all workers
	void stop() const {owner->stop();}


	~AsyncDispatcher() {
		if (owner != nullptr) stop();
	}

	PAsyncControl getHandle() const {return owner;}
protected:

	PAsyncControl owner;

};





}



