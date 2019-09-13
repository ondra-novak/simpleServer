#pragma once

#include "../abstractStream.h"
#include "../address.h"


namespace simpleServer {

class FileStream: public AbstractStream {
public:

	FileStream(int fd);
	virtual ~FileStream() noexcept;

	virtual int setIOTimeout(int timeoutms) override;
	int getFD() const {return fd;}

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

	int fd;


};


}
