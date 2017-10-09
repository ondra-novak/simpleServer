#pragma once
#include "refcnt.h"
#include "stringview.h"

namespace simpleServer {


///Very abstract interface which descibes protocol to access data in the stream
class IGeneralStream {
public:


	///Describes outbut buffer
	struct WrBuffer{
		///pointer to first byte in the buffer
		unsigned char *ptr = 0;
		///total size of the buffer
		std::size_t size = 0;
		///currect write position
		std::size_t wrpos = 0;


		std::size_t remain() const {return size - wrpos;}
		BinaryView getView() const {return BinaryView(ptr,wrpos);}

		WrBuffer() {}
		WrBuffer(const MutableBinaryView &b):ptr(b.data),size(b.length),wrpos(0) {}
	};


	enum WriteMode {
		///perform writing non-blocking. If it is impossible, nothing is written
		/** this mode is fastest but can cause failure while writing */
		writeNonBlock = 0,
		/** fastest blocking write, but can write less than expected */
		///write at least one byte, but don't block for writing additional bytes
		writeCanBlock = 1,
		///write whole buffer, which can cause multiple blocking calls
		/** slower blocking write, but ensures, that whole buffer will be sent */
		writeWholeBuffer = 2,
		///write whole buffer and request the flush operation for any transparent buffer in the stream
		/** slowest operation, but ensures that everything has been written */
		writeAndFlush = 3

	};

	///Determines whether buffer returned by readBuffer() is empty because end of file has been reached
	/**
	 * For blocking reading, returning empty buffer is always considered as end of file. However
	 * for non-blocking reading, an empty buffer often mean, that no data are currently available.
	 * To distinguish between these ambignuous situations, you should call isEof on restult
	 * to determine, whether returned empty buffer is because eof has been reached
	 *
	 * @param buffer buffer returned by readBuffer function()
	 * @retval true buffer is empty because eof
	 * @retval false buffer is empty because no data are available for non-blocking call, or buffer
	 * is not empty
	 */
	static bool isEof(const BinaryView &buffer);

protected:

	///Read buffer
	/** Function reads to internal buffer, optimal count of bytes and returns it as BinaryView
	 *
	 * @param nonblock set true to request non-blocking operation. In this case, returned value
	 * can be empty buffer, if the operation would block. Setting to false causes, that operation
	 * can block while it waiting for the first byte. Function should not block while there is
	 * at least one byte available to read by non-blocking way.
	 */
	virtual BinaryView readBuffer(bool nonblock) = 0;
	///Request to create output buffer.
	/** To start writing, caller requires buffer to write bytes. This function is called
	 * before very first writing. Function should supply a buffer with optimal size for
	 * the stream. In most of cases, the buffer is located in the instance of the stream and
	 * this function just prepares that buffer .
	 *
	 *
	 *
	 */
	virtual MutableBinaryView createOutputBuffer() = 0;


	///Write buffer to the stream
	/** @param buffer Writes buffer to the output stream
	 * @param wrmode specify one of writing mode
	 *
	 * @return count of bytes written
	 *
	 */
	virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode) = 0;
	///Request to block thread until there is data available for read
	/**
	 * @param timeout specifies timeout in miliseconds. A negative value is interpreted as
	 *  infinite wait. Operation can be interruped by closing the input
	 */
	virtual bool waitForRead(int timeoutms) = 0;
	///Request to block thread while there is no space in the output buffer
	/**
	 * 	 @param timeout specifies timeout in miliseconds. A negative value is interpreted as
	 *  infinite wait.
	 */

	virtual bool waitForWrite(int timeoutms) = 0;


	///Closes input, so no more bytes will arrive and any waiting is immediately terminated
	/** Function can be called from the other thread, however only one thread should call this
	 * function at the time
	 */
	virtual void closeInput() = 0;

	///Closes the output sending EOF to the stream. Other side will receive the EOF through the readBuffer()
	/** Function can be called from the other thread, however only one thread should call this
	 * function at the time
	 */
	virtual void closeOutput() = 0;


	virtual void flushOutput() = 0;
protected:

	///Constant which should be returned by readBuffer() when reading reaches to EOF
	static BinaryView eofConst;

};


///AbstractStream brings some additional function which halps to work with streams and buffers.
/** Streams should extend this class, not IGenerlStream. The class also allows to ref-counted
 * references to the stream and expects that streams will be allocated by operator new  */
class AbstractStream: public RefCntObj, public IGeneralStream {
public:


	BinaryView read(bool nonblock = false) {
		if (rdBuff.empty()) {
			rdBuff = readBuffer(nonblock);
		}
		return rdBuff;
	}

	BinaryView commit(std::size_t sz) {
		rdBuff = rdBuff.substr(sz);
		return rdBuff;
	}


	///Reads byte from the stream and returns it. The byte is also removed from the stream
	/**
	 * @return next byte in the stream, or -1 for EOF
	 *
	 * One byte can be returned back by function putBackByte(). You cannot put back EOF
	 *
	 */
	int readByte() {
		BinaryView b = read();
		if (isEof(b)) return -1;
		int r = b[0];
		commit(1);
		return r;
	}


	///Returns next byte without removing it from the stream
	/**
	 * @return next byte, or -1 for EOF
	 */
	int peekByte() {
		BinaryView b = read();
		if (isEof(b)) return -1;
		return b[0];
	}


	///Returns last read byte back to the stream
	/** Function just adjusts the buffer by 1 byte back. Note that function is potentially unsafe
	 * if it is used by incorrect way. It is allowed to be called just once after readByte(). Otherwise
	 * the function can put the stream into unpredictable state. Using unreadByte() without
	 * previously called readByte() will probably crash the program
	 */
	void putBackByte() {
		rdBuff = BinaryView(rdBuff.data-1, rdBuff.length+1);
	}

	///Writes block of bytes
	/**
	 * @param buffer buffer to write
	 * @param wrmode specify write mode
	 *
	 * Function uses buffer to collect small writes into single brust write.
	 */
	BinaryView write(const BinaryView &buffer, WriteMode wrmode = writeWholeBuffer);


	///write one byte to the output stream.
	/**
	 * @param b byte to write. note that writes are buffered, so you will need to call flush()
	 * to force to send everything in the stream.
	 */
	void writeByte(unsigned char b) {
		if (wrBuff.size == wrBuff.wrpos) {
			if (wrBuff.size == 0) wrBuff = createOutputBuffer();
			else flush(writeCanBlock);
		}
		wrBuff.ptr[wrBuff.wrpos] = b;
		wrBuff.wrpos++;
	}


	///Contains minimum required size of buffer which must be supported by all the streams
	static const size_t minimumRequiredBufferSize = 256;

	///Asks the stream to prepare a write buffer for the direct access
	/**
	 * @param reqSize requested size. The stream should prepare specified count of bytes. However
	 * the value can be limited to minimumRequiredBufferSize. Specifiying smaller value allows
	 * to effectively use of the buffer.
	 *
	 * @return returns WrBuffer structure containing informations about the buffer
	 *
	 * If you finished writting to the buffer, you need to call commitWriteBuffer()
	 *
	 * @note there is one buffer only. You should avoid to call getWriteBuffer() twice or more
	 * without calling the comitWriteBuffer()
	 */
	MutableBinaryView getWriteBuffer(std::size_t reqSize = minimumRequiredBufferSize) {
		if (reqSize > wrBuff.size) reqSize = wrBuff.size;
		while (wrBuff.remain() < reqSize) {
			flush(writeCanBlock);
		}

		return MutableBinaryView(wrBuff.ptr + wrBuff.wrpos, wrBuff.remain());
	}

	///commits writtes to the buffer
	/** @param commitSize count of bytes written to the buffer. The number must be less or equal
	 * to size of the buffer returned by getWriteBuffer()
	 */
	void commitWriteBuffer(std::size_t commitSize) {
			wrBuff.wrpos += commitSize;
	}


	///Flush output buffer to the stream
	void flush(WriteMode wrmode = writeAndFlush) {
		if (wrBuff.size) {
			BinaryView b = wrBuff.getView();
			auto r = writeBuffer(b,writeAndFlush);
			if (r == 0) return;
			wrBuff.wrpos = 0;
			if (r != b.length) {
				write(b.substr(r),writeNonBlock);
			}
		} else if (wrmode == writeAndFlush) {
			flushOutput();
		}
	}


	///writes EOF mark to the output stream
	/** it finishes writing and closes output stream. Other side receives EOF. After
	 * this function, no more data are expected
	 *
	 * This function is called automatically when last instance of the stream is destroyed
	 */
	void writeEof() {
		flush(writeWholeBuffer);
		closeOutput();
	}


	///Puts EOF character into input buffer
	/**
	 * Function commits whole input buffer, then it puts EOF to the buffer. Next, it
	 * closes the input which should cause that any waiting is finished returning EOF
	 * as result.
	 *
	 * Functon can be called from different thread
	 */
	void putBackEof() {
		rdBuff = eofConst;
		closeInput();
	}

protected:



	BinaryView rdBuff;
	WrBuffer wrBuff;


	BinaryView writeBuffered(const BinaryView &buffer, WriteMode wrmode );



};


class Stream: public RefCntPtr<AbstractStream> {
public:

	Stream() {}
	using RefCntPtr<AbstractStream>::RefCntPtr;

	int operator()() {
		ptr->readByte();
	}
	void operator()(int b) {
		ptr->writeByte((unsigned char)b);
	}

	int peek() const {
		ptr->peekByte();
	}
	BinaryView read(bool nonblock=false) {
		return ptr->read(nonblock);
	}
	BinaryView commit(std::size_t sz) {
		return ptr->commit(sz);
	}
	void putBackByte() {
		return ptr->putBackByte();
	}
	void putBackEof() {
		return ptr->putBackEof();
	}
	void writeEof() {
		ptr->writeEof();
	}
	void flush(IGeneralStream::WriteMode wr = IGeneralStream::writeAndFlush) {
		ptr->flush(wr);
	}
	MutableBinaryView getWriteBuffer(std::size_t reqSize = AbstractStream::minimumRequiredBufferSize) {
		return ptr->getWriteBuffer(reqSize);
	}
	void commitWriteBuffer(std::size_t commitSize) {
		return ptr->commitWriteBuffer(commitSize);
	}
	BinaryView write(const BinaryView &buffer, IGeneralStream::WriteMode wrmode = IGeneralStream::writeWholeBuffer) {
		return ptr->write(buffer, wrmode);
	}
};

class InputStream: public RefCntPtr<AbstractStream> {
public:

	InputStream() {}
	using RefCntPtr<AbstractStream>::RefCntPtr;
	InputStream(const Stream &other):RefCntPtr<AbstractStream>(other) {}

	int operator()() {
		ptr->readByte();
	}
	int peek() const {
		ptr->peekByte();
	}
	BinaryView read(bool nonblock=false) {
		return ptr->read(nonblock);
	}
	BinaryView commit(std::size_t sz) {
		return ptr->commit(sz);
	}
	void putBackByte() {
		return ptr->putBackByte();
	}
	void putBackEof() {
		return ptr->putBackEof();
	}
};

class OutputStream: public RefCntPtr<AbstractStream> {
public:

	OutputStream() {}
	using RefCntPtr<AbstractStream>::RefCntPtr;
	OutputStream(const Stream &other):RefCntPtr<AbstractStream>(other) {}

	void operator()(int b) {
		ptr->writeByte((unsigned char)b);
	}
	void writeEof() {
		ptr->writeEof();
	}
	void flush(IGeneralStream::WriteMode wr = IGeneralStream::writeAndFlush) {
		ptr->flush(wr);
	}
	MutableBinaryView getWriteBuffer(std::size_t reqSize = AbstractStream::minimumRequiredBufferSize) {
		return ptr->getWriteBuffer(reqSize);
	}
	void commitWriteBuffer(std::size_t commitSize) {
		return ptr->commitWriteBuffer(commitSize);
	}
	BinaryView write(const BinaryView &buffer, IGeneralStream::WriteMode wrmode = IGeneralStream::writeWholeBuffer) {
		return ptr->write(buffer, wrmode);
	}
};




}
