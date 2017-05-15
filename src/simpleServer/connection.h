#pragma once

#include <cstring>
#include <functional>

#include "address.h"
#include "exceptions.h"
#include "refcnt.h"
#include "stringview.h"
#include "async.h"

namespace simpleServer {

class IConnection: public RefCntObj {
public:
	virtual ~IConnection() {}

	virtual BinaryView readData(unsigned int prevRead) = 0;
	virtual void writeData(const BinaryView &data) = 0;
	virtual void flush() = 0;
	virtual void closeOutput() = 0;
	virtual void closeInput() = 0;

	typedef std::function<void(AsyncState, BinaryView)> Callback;

	virtual unsigned int getIOTimeout() const = 0;
	virtual void setIOTimeout(unsigned int t) = 0;
	virtual NetAddr getPeerAddr() const  = 0;
	virtual void asyncRead(AsyncDispatcher cntr, Callback callback, unsigned int timeoutOverride = 0) = 0;
	virtual void asyncWrite(BinaryView data, AsyncDispatcher cntr, Callback callback, unsigned int timeoutOverride = 0) = 0;
	virtual void asyncFlush(AsyncDispatcher cntr, Callback callback, unsigned int timeoutOverride = 0) = 0;
	virtual bool cancelAsyncRead(AsyncDispatcher cntr) = 0;
	virtual bool cancelAsyncWrite(AsyncDispatcher cntr) = 0;
};

typedef RefCntPtr<IConnection> PConnection;

class Connection;


static const unsigned int infinity = (unsigned int)-1;

typedef std::function<Connection(NetAddr, std::uintptr_t)> ConnectionFactory;

class ConnectParams {
public:
	unsigned int waitTimeout;
	ConnectionFactory factory;


	ConnectParams():waitTimeout(infinity),factory(nullptr) {}
	ConnectParams(unsigned int timeout):waitTimeout(timeout),factory(nullptr) {}
	ConnectParams(unsigned int timeout, const ConnectionFactory &f):waitTimeout(timeout),factory(f) {}
};

class Connection {
public:

	typedef IConnection::Callback Callback;
	typedef std::function<void(AsyncState, const Connection *)> ConnectCallback;


	explicit Connection(PConnection conn):conn(conn) {}

	void flush() const {conn->flush();}
	void closeOutput() const {conn->closeOutput();}
	void closeInput() const {conn->closeInput();}


	BinaryView operator()(unsigned int prevRead) const {
		return conn->readData(prevRead);
	}

	void operator()(const BinaryView &data) const {
		return conn->writeData(data);
	}

	NetAddr getPeerAddr() const {return conn->getPeerAddr();}

	unsigned int getIOTimeout() const {return conn->getIOTimeout();}
	void setIOTimeout(unsigned int t) {conn->setIOTimeout(t);}

	PConnection getHandle() const {return conn;}

	static Connection connect(const NetAddr &addr,const ConnectParams &params = ConnectParams());

	static void connect(const NetAddr &addr, AsyncDispatcher cntr, ConnectCallback callback,const ConnectParams &params = ConnectParams());

	bool operator==(const Connection &c) const {
		return conn == c.conn;
	}
	bool operator!=(const Connection &c) const {
		return conn != c.conn;
	}

	///Performs asynchronous read
	/**
	 * @param cntr object which provides asynchronous operation
	 * @param callback function which is called when asynchronous operation is finished
	 * @param timeoutOverride overrides default timeout. You can override with timeout other then zero, otherwise
	 * default timeout is used.
	 *
	 * @note Function cannot be used to commit read data. Use readData() to commit, because that operation cannot block
	 *
	 * @note there must be only one pending reading active at time. Running multiple
	 * reading operations leads to undefined behaviour
	 *
	 */
	void asyncRead(const AsyncDispatcher &cntr, Callback callback,unsigned int timeoutOverride = 0) {
		conn->asyncRead(cntr, callback,timeoutOverride);
	}
	///Performs asynchronous write
	/**
	 * @param data data to write. Note, buffer referenced by this view must not be
	 * released before the operation completes. Also note that internal buffers are still in effect, so operation can immediatelly complete if there is room in output
	 * buffer. Also check for asyncFlush() function.
	 * Sending empty buffer is equivalent to closing the output but it can be perfromed asynchronously
	 * @param cntr object which provides asynchronous operation
	 * @param callback function which is called when asynchronous operation is finished
	 * @param timeoutOverride overrides default timeout. You can override with timeout other then zero, otherwise
	 * default timeout is used.
	 *
	 * @note there must be only one pending writting active at time. Running multiple
	 * writing operations leads to undefined behaviour
	 */
	void asyncWrite(const BinaryView &data, const AsyncDispatcher &cntr, Callback callback, unsigned int timeoutOverride = 0) {
		conn->asyncWrite(data,cntr, callback,timeoutOverride);
	}

	///Performs asynchronous flush of the internal buffers
	/**
	 * @param cntr object which provides asynchronous operation
	 * @param callback function which is called when asynchronous operation is finished
	 * @param timeoutOverride overrides default timeout. You can override with timeout other then zero, otherwise
	 * default timeout is used.
	 *
	 * @note there must be only one pending writting active at time. Running multiple
	 * writing operations leads to undefined behaviout
	 */

	void asyncFlush(const AsyncDispatcher &cntr, Callback callback, unsigned int timeoutOverride = 0) {
		conn->asyncFlush(cntr, callback,timeoutOverride);
	}

	///Cancels asynchronous read
	/**
	 *
	 * @param cntr object which provides asynchronous operation				rdbuff_used = rdbuff_pos = 0;
				prevRead = 0;
	 *
	 * @retval true asynchronous operation has been canceled
	 * @retval false there is no pending operation. This can happen, when function
	 * is called after the operation completted. This also means, that callback
	 * function has been or currently is being called
	 *
	 * @note After successful cancelation, no bytes should be transfered, so you
	 * can issue new reading operation
	 */
	bool cancelAsyncRead(const AsyncDispatcher &cntr) {
		return conn->cancelAsyncRead(cntr);
	}


	///Cancels asynchronous write
	/**
	 *
	 * @param cntr object which provides asynchronous operation
	 * @retval true asynchronous operation has been canceled
	 * @retval false there is no pending operation. This can happen, when function
	 * is called after the operation completted. This also means, that callback
	 * function has been or currently is being called
	 */
	bool cancelAsyncWrite(const AsyncDispatcher &cntr) {
		return conn->cancelAsyncWrite(cntr);
	}
protected:

	PConnection conn;

};


}
