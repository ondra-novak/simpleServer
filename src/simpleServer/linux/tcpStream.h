#pragma once

#include "../abstractStream.h"
#include "../address.h"


namespace simpleServer {

class TCPStream: public AbstractStream {
public:
	virtual NetAddr getPeerAddr() const {return peer;}

	TCPStream(int sck, int iotimeout, const NetAddr &peer);
	virtual ~TCPStream() noexcept;

	virtual int setIOTimeout(int timeoutms) override;

protected:


	virtual BinaryView implRead(bool nonblock) override;
	virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) override;
	virtual BinaryView implWrite(BinaryView buffer, bool nonblock) override;
	virtual void implWrite(WrBuffer &curBuffer, bool nonblock) override;
	virtual void implReadAsync(const Callback &cb) override;
	virtual void implReadAsync(const MutableBinaryView &buffer, const Callback &cb)  override;
	virtual void implWriteAsync(const BinaryView &data, const Callback &cb)  override;
	virtual bool implWaitForRead(int timeoutms)  override;
	virtual bool implWaitForWrite(int timeoutms)  override;
	virtual void implCloseInput()  override;
	virtual void implCloseOutput()  override;
	virtual void implFlush()  override;

//	template<typename T> friend class RefCntPtr;


protected:


	static const int inputBufferSize = 4096;
	static const int outputBufferSize = 4096;

	unsigned char inputBuffer[inputBufferSize];
	unsigned char outputBuffer[outputBufferSize];

	int sck;
	int iotimeout;
	NetAddr peer;


};


}
