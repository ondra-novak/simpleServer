/*
 * socketConnection.h
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include "../address.h"
#include "../refcnt.h"
#include "../stringview.h"
#include "../connection.h"

#pragma once
namespace simpleServer {

class SocketConnection;
typedef RefCntPtr<SocketConnection> PSocketConnection;

class SocketConnection: public AbstractConnection<2000> {


public:


	SocketConnection(int sock,unsigned int iotimeout, NetAddr peerAddr);
	~SocketConnection();



	///stream is closed on input (received 0)
	bool isClosed() const;
	///closes input - no more data can be read
	void closeInput() override;

	void closeOutput() override;

	NetAddr getPeerAddr() const override {return peerAddr;}


	unsigned int getIOTimeout() const override {return iotimeout;}
	void setIOTimeout(unsigned int t) override {iotimeout = t;}

	virtual void asyncRead(AsyncControl cntr, Callback callback, unsigned int timeoutOverride = 0) override;
	virtual void asyncWrite(BinaryView data, AsyncControl cntr, Callback callback, unsigned int timeoutOverride = 0) override;
	virtual void asyncFlush(AsyncControl cntr, Callback callback, unsigned int timeoutOverride = 0) override;
	virtual bool cancelAsyncRead(AsyncControl cntr) override;
	virtual bool cancelAsyncWrite(AsyncControl cntr) override;



protected:

	virtual void sendAll(BinaryView data) override;
	virtual int recvData(unsigned char *buffer, std::size_t size, bool nonblock) override;
	void runAsyncWrite(BinaryView data, std::size_t offset, AsyncControl cntr, Callback callback, unsigned int timeoutOverride = 0) ;


	void waitForData();
	void waitForSend();


	BinaryView getReadBuffer() const;

	static const int bufferSize = 3000;

	NetAddr peerAddr;
	int sock;
	unsigned int iotimeout;
	unsigned int rdbuff_used;
	unsigned int rdbuff_pos;
	unsigned int wrbuff_pos;
	bool eof;
	bool eofReported;

	unsigned char rdbuff[bufferSize];
	unsigned char wrbuff[bufferSize];
};

} /* namespace simpleServer */

