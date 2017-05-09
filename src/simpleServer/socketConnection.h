/*
 * socketConnection.h
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include "address.h"
#include "refcnt.h"
#include "stringview.h"
#include "connection.h"

#pragma once
namespace simpleServer {

class SocketConnection;
typedef RefCntPtr<SocketConnection> PSocketConnection;

class SocketConnection: public IConnection {


public:


	SocketConnection(int sock,unsigned int iotimeout, NetAddr peerAddr);
	~SocketConnection();


	BinaryView readData(unsigned int prevRead);

	///stream is closed on input (received 0)
	bool isClosed() const;

	///closes input - no more data can be read
	void closeInput();


	void writeData(const BinaryView &data);
	void flush();
	void closeOutput();





	NetAddr getPeerAddr() const {return peerAddr;}


	unsigned int getIOTimeout() const {return iotimeout;}
	void setIOTimeout(unsigned int t) {iotimeout = t;}



protected:

	void waitForData();
	void waitForSend();
	void sendAll(BinaryView data);


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

