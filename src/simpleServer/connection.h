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

	void flush() {conn->flush();}
	void closeOutput() {conn->closeOutput();}
	void closeInput() {conn->closeInput();}


	BinaryView operator()(unsigned int prevRead) {
		return conn->readData(prevRead);
	}

	void operator()(const BinaryView &data) {
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
	void asyncRead(const AsyncDispatcher &cntr, Callback callback,unsigned int timeoutOverride) {
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
	 * @param cntr object which provides asynchronous operation
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


template<std::size_t bufferSize >
class AbstractConnection: public IConnection {
public:

	AbstractConnection()
		:rdbuff_used(0)
		,rdbuff_pos(0)
		,wrbuff_pos(0)
		,eof(false)
		,eofReported(false) {}

	virtual BinaryView readData(unsigned int prevRead) override {
		BinaryView buff = getReadBuffer();
		if (prevRead >= buff.length) {
			rdbuff_used = rdbuff_pos = 0;
		} else{
			rdbuff_pos += prevRead;
		}
		buff = getReadBuffer();
		if (prevRead) {
			if (buff.length == 0 && !eof) {
				int res = recvData(rdbuff,bufferSize,true);
				if (res == 0) {
					eof = true;
				} else if (res > 0) {
					rdbuff_used = res;
					rdbuff_pos = 0;
					buff = getReadBuffer();
				}
			}
			return buff;
		} else {
			if (buff.length == 0) {
				if (!eof) {
					flush();
					int res = recvData(rdbuff,bufferSize,false);
					if (res == 0) {
						eof = true;
						eofReported = true;
					} else if (res > 0) {
						rdbuff_used = res;
						rdbuff_pos = 0;
						buff = getReadBuffer();
					}
				} else {
					if (eofReported)
						throw EndOfStreamException();
					eofReported = true;
				}
			}
			return buff;
		}
	}


	virtual void writeData(const BinaryView &data) override {
		if (data.length) {
			if (wrbuff_pos == 0 && data.length > bufferSize) {
				sendAll(data);
			} else {
				std::size_t remain = bufferSize - wrbuff_pos;
				std::size_t transmit = std::min(remain,data.length);
				std::memcpy(wrbuff+wrbuff_pos,data.data,transmit);
				wrbuff_pos += transmit;
				if (wrbuff_pos == bufferSize) {
					flush();
				}
				BinaryView newdata = data.substr(transmit);
				if (newdata.length) writeData(newdata);
			}
		} else {
			closeOutput();
		}

	}
	virtual void flush() override {
		if (wrbuff_pos) {
			BinaryView data(wrbuff, wrbuff_pos);
			wrbuff_pos = 0;
			sendAll(data);
		}
	}
protected:
	///must be implemented
	virtual void sendAll(BinaryView data) = 0;
	///must be implemented
	virtual int recvData(unsigned char *buffer, std::size_t size, bool nonblock) = 0;
	///must be implemented
	virtual void closeOutput() = 0;

	unsigned int rdbuff_used;
	unsigned int rdbuff_pos;
	unsigned int wrbuff_pos;
	bool eof;
	bool eofReported;

	unsigned char rdbuff[bufferSize];
	unsigned char wrbuff[bufferSize];

	BinaryView getReadBuffer() const {
		return BinaryView(rdbuff+rdbuff_pos, rdbuff_used - rdbuff_pos);
	}

};


}
