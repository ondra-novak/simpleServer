#pragma once

#include "../abstractStream.h"


namespace simpleServer {

class TCPStream: public AbstractStream {
protected:

	virtual BinaryView readBuffer(bool nonblock) ;
	virtual MutableBinaryView createOutputBuffer() ;
	virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode) ;
	virtual bool waitForRead(int timeoutms) ;
	virtual bool waitForWrite(int timeoutms);
	virtual void closeInput() ;
	virtual void closeOutput();
	virtual void flushOutput();



protected:

	static const int inputBufferSize = 4096;
	static const int outputBufferSize = 4096;

	char inputBuffer[inputBufferSize];
	char outputBuffer[outputBufferSize];

	int sck;


	int iotimeout;


};


}
