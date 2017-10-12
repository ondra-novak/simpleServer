#include <functional>

#pragma once

namespace simpleServer {

class AsyncResource;

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


class IAsyncProvider {
public:



	typedef std::function<void(AsyncState, BinaryView)> Callback;


	///Performs asynchronous read operation
	/**
	 * @param resource identifies resource. Resources are not available directly on
	 * the public interface, they are provided by the stream providers
	 * @param buffer mutable buffer, where to result will be put
	 * @param timeout total timeout
	 * @param completion function called for completion result.
	 */
	virtual void read(const AsyncResource &resource,
			MutableBinaryView buffer,
			int timeout,
			Callback completion) = 0;

	///Performs asynchronous write operation
	/**
	 *
	 * @param resource identifies resource. Resources are not available directly on
	 * the public interface, they are provided by the stream providers
	 * @param buffer buffer to write.
	 * @param timeout total timeout
	 * @param completion function called for completion result
	 */
	virtual void write(const AsyncResource &resource,
			BinaryView buffer,
			int timeout,
			Callback completion) = 0;




	///Assigns the thread to the asynchronous provider
	/** function performs one cycle of the serving
	 *
	 *  - acquires waiting slot
	 *  - waits for event
	 *  - perform tasks associated with th event
	 *  - exit
	 */
	virtual void serve() = 0;

	///Releases all threads in waiting state
	/** it releases thread that waiting for acquire waiting slot or for event
	 */
	virtual void releaseThreads() = 0;

	virtual ~IAsyncProvider() {}
};


class AbstractWaitingSlot: public RefCntObj, public IAsyncProvider {
public:

	class Task {
	public:
		Task(Callback cb, AsyncState state, BinaryView data):cb(cb), state(state), data(data) {}

		void run() throw() {
			cb(state, data);
		}
	protected:
		Callback cb;
		AsyncState state;
		BinaryView data;
	};

	virtual Callback waitForEvent() = 0;


	virtual void cancelWait() = 0;



	virtual void serve() {
		Task task = waitForEvent();
		task.run();
	}

	virtual void releaseThreads() {
		cancelWait();
	}

};


}
