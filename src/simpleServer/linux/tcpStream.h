#pragma once

#include "../abstractStream.h"
#include "../address.h"


namespace simpleServer {

class TCPStream: public AbstractStream {
public:
	virtual NetAddr getPeerAddr() const {return peer;}

	TCPStream(int sck, int iotimeout, const NetAddr &peer);
	~TCPStream();

protected:


	virtual BinaryView readBuffer(bool nonblock) ;
	virtual MutableBinaryView createOutputBuffer() ;
	virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode) ;
	virtual bool waitForRead(int timeoutms) ;
	virtual bool waitForWrite(int timeoutms);
	virtual void closeInput() ;
	virtual void closeOutput();
	virtual void flushOutput();
	virtual int setIOTimeout(int iotimeoutms) override;


protected:

	static const int inputBufferSize = 4096;
	static const int outputBufferSize = 4096;

	char inputBuffer[inputBufferSize];
	char outputBuffer[outputBufferSize];

	int sck;
	int iotimeout;
	NetAddr peer;


};


}
