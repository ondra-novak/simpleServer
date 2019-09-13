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
	int getSocket() const {return sck;}
	int getIOTimeout() const {return iotimeout;}

protected:


	virtual BinaryView implRead(bool nonblock) override;
	virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) override;
	virtual BinaryView implWrite(BinaryView buffer, bool nonblock) override;
	virtual bool implWrite(WrBuffer &curBuffer, bool nonblock) override;
	virtual void implReadAsync(Callback &&cb) override;
	virtual void implReadAsync(const MutableBinaryView &buffer, Callback &&cb)  override;
	virtual void implWriteAsync(const BinaryView &data, Callback &&cb)  override;
	virtual bool implWaitForRead(int timeoutms)  override;
	virtual bool implWaitForWrite(int timeoutms)  override;
	virtual void implCloseInput()  override;
	virtual void implCloseOutput()  override;
	virtual bool implFlush()  override;

//	template<typename T> friend class RefCntPtr;


protected:


	static const int inputBufferSize = 4096;
	static const int outputBufferSize = 4096;

	unsigned char inputBuffer[inputBufferSize];
	unsigned char outputBuffer[outputBufferSize];

	int sck;
	int iotimeout;
	NetAddr peer;

	virtual void asyncReadCallback(const MutableBinaryView& buffer, Callback&& cb, AsyncState state);
	virtual void asyncWriteCallback(const BinaryView& data, Callback&& cb, AsyncState state);

	static bool doPoll(int sock, int events, int timeoutms);

};


}
