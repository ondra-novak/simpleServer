#pragma once

namespace simpleServer {


class AsyncProvider;

class IGeneralStreamAsync {
public:


	typedef std::function<void()> ExceptionCallback;

	///Sets asynchornous provider
	/**
	 * Before any asynchronous operation can be requested, the stream need to be assigned to an
	 * asynchronous provider. It is responsible to running asynchronous tasks and executing callback
	 * when the asynchronous task is complete
	 *
	 * @param provider pointer to asynchronous provider.
	 * @param asyncTimeout specifies a timeout for all asynchronous operations.
	 * @param expCb reference to function which is called in case of exception happened during
	 *     any asynchronous operation. The function is called in catch branch, so current
	 *     exception is available. If another exception is thrown from this function, std::terminate
	 *     is executed.
	 *
	 *
	 */
	virtual void setAsyncProvider(AsyncProvider *provider, int asyncTimeout, ExceptionCallback expCb) = 0;



protected:

	///Function which is called when readBufferAsync or writeBufferAsync completes
	typedef std::function<bool(const BinaryView &)> AsyncCallback;


	///Perform asynchornous read.
	/**
	 * @param cb a callback function which is executed when asynchronous reading completes. Function
	 * receives a buffer with read data. If empty buffer is received, then EOF has been reached.
	 *
	 * @note The function is called in a context of thread belongs to AsyncProvider. You should transfer
	 * the execution to other thread in order to avoid blocking the AsyncProvider from the execution
	 *
	 * @note only one pending asynchronous reading is possible. Starting two or more asynchronous reading
	 * can results to upredictable behaviour. Also reading synchronously while there is an other asynchronous reading
	 * already pending can results to upredictable behaviour
	 */
	virtual void readBufferAsync(const AsyncCallback &cb) = 0;


	///Performs asynchronous write
	/**
	 * @param data data to write
	 * @param cb a callback function which is executed when asynchronous reading completes. Function
	 * receives a buffer with not yet processed data. Function can retry the attempt.
	 *
	 * @note the data reference by the argument 'data' must not be released until the writing
	 * is complete.

	 * @note only one pending asynchronous writing is possible. Starting two or more asynchronous writing
	 * can results to upredictable behaviour. Also writing synchronously while there is an other asynchronous write
	 * already pending can results to upredictable behaviour

	 */
	virtual void writeBufferAsync(const BinaryView &data, const AsyncCallback &cb) = 0;



	virtual ~IGeneralStreamAsync() {}


	template<typename Fn>
	class WriteAll {
	public:
		WriteAll(const Fn &fn, IGeneralStreamAsync *owner):fn(fn),owner(owner) {}
		void operator()(const BinaryView &b) const {
			if (b.empty()) fn();
			else owner->writeBufferAsync(b, WriteAll(fn, owner));
		}

	protected:
		Fn fn;
	   IGeneralStreamAsync * owner;

};


class AbstractStreamAsync: public IGeneralStreamAsync, public AbstractStream {
public:

	typedef IGeneralStreamAsync::AsyncCallback Callback;


	template<typename Fn>
	void readAsync(const Fn &fn) {
		if (!rdBuff.empty()) {
			fn(rdBuff);
		} else {
			readBufferAsync([=](const BinaryView &b) {
			   rdBuff = b;
			   fn(rdBuff);
			});
		}
	}


	template<typename Fn>
	void writeAsync(const BinaryView &buffer, const Fn &complet) {

		BinaryView b = write(buffer, writeNonBlock);
		if (b == buffer) {
			flushAsync([=] {
				if (b.length > wrBuff.size) {
					writeBufferAsync(b, WriteAll<Fn>(fn, this));
				} else {
					writeAsync(b, complet);
				}
			});
		} else if (b.empty()) {
			complet();
		} else {
			writeAsync(b, complet);
		}

	}

	template<typename Fn>
	void flushAsync(const Fn &fn) {
		writeBufferAsync(wrBuff.getView(), WriteAll<Fn>(fn, this));
	}


};

}
