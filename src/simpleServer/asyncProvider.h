#include <functional>

#pragma once

namespace simpleServer {

class AsyncResource;

enum CompletionStatus {
	///request processed without errors.
	statusOK = 0,
	///request processed, but EOF has been reached
	statusEof = 1,
	///request did not completed in time (timeout error)
	statusTimeout = 2,
	///request did not processed, because error.
	/** In this case, function called in exception handler.
	 * You can rethrow exception to receive more informations
	 */
	statusError = 3

};


class IAsyncProvider {
public:



	typedef std::function<void(CompletionStatus, BinaryView)> Callback;


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

};




}
