#pragma once
#include "refcnt.h"
#include "stringview.h"
#include "asyncProvider.h"

namespace simpleServer {

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


class AsyncResource;

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


	///Changes timeout for all blocking operations
	/**
	 * @param timeoutms timeout in miliseconds. Set negative number to set infinite timeout
	 *
	 * @note not all streams support timeout. Especially streams which doesn't support waiting
	 * also cannot have timeout.
	 *
	 * @return function returns previous timeout (this allows to set timeout temporary and
	 * later to restore original value)
	 */
	virtual int setIOTimeout(int timeoutms) = 0;


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

	virtual void closeInput() = 0;
	virtual void closeOutput() = 0;


	virtual void flushOutput() = 0;

	virtual void doReadAsync(const IAsyncProvider::Callback &cb) = 0;
	virtual void doWriteAsync(const IAsyncProvider::Callback &cb, BinaryView data) = 0;

	virtual ~IGeneralStream() {}
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
			return readBuffer(nonblock);
		} else {
			BinaryView t = rdBuff;
			rdBuff = BinaryView();
			return t;
		}
	}


	void putBack(const BinaryView &data) {
		rdBuff = data;
	}


	///Reads byte from the stream and returns it. The byte is also removed from the stream
	/**
	 * @return next byte in the stream, or -1 for EOF
	 *
	 * One byte can be returned back by function putBackByte(). You cannot put back EOF
	 *
	 */
	int readByte() {
		if (rdBuff.empty()) {
			rdBuff = readBuffer(false);
			if (isEof(rdBuff)) return -1;
		}
		int r = rdBuff[0];
		rdBuff = rdBuff.substr(1);
		return r;
	}


	///Returns next byte without removing it from the stream
	/**
	 * @return next byte, or -1 for EOF
	 */
	int peekByte() {
		if (rdBuff.empty()) {
			rdBuff = readBuffer(false);
			if (isEof(rdBuff)) return -1;
		}
		int r = rdBuff[0];
		return r;
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

	static MutableBinaryView noOutputMode();


	bool waitForInput(int timeout) {
		if (rdBuff.empty()) return waitForRead(timeout);
		else return true;
	}

	bool waitForOutput(int timeout) {
		if (wrBuff.remain()) return waitForWrite(timeout);
		else return true;
	}


	IAsyncProvider *setAsyncProvider(IAsyncProvider *asyncProvider);


	///Read asynchronously
	/**
	 * @param callbackFn function called when operation completes.
	 *
	 * @note function can be called immediately if there are data already available to
	 * processing
	 */
	template<typename Fn>
	void readAsync(const Fn &callbackFn) {

		BinaryView data = read(true);
		if (!data.empty() || isEof(data))
			callbackFn(statusOK, data);
		else
			doReadAsync(callbackFn);
	}

	///Write synchronously
	/**
	 * Performs nonblocking write. If write is unsuccessful, it asynchronously flushes
	 * obsah of the buffer and then put the data to the new empty buffer. Note the function
	 * doesn't provide write modes. It can always transfer less bytes than requested, but
	 * at least one.
	 *
	 * @param data data to write
	 * @param callbackFn callback function called when operation completes
	 */
	template<typename Fn>
	void writeAsync(BinaryView data, const Fn &callbackFn) {

		BinaryView remainData = write(data,writeNonBlock);
		if (remainData == data) {
			doWriteAsync(wrBuff.getView(), [=](CompletionStatus status, BinaryView remain) {
				wrBuff.wrpos = 0;
				write(remain, writeNonBlock);
				BinaryView x = write(data, writeNonBlock);
				callbackFn(x);
			});
		}

	}



protected:



	BinaryView rdBuff;
	WrBuffer wrBuff;

	IAsyncProvider *asyncProvider = nullptr;




	BinaryView writeBuffered(const BinaryView &buffer, WriteMode wrmode );



};


class Stream: public RefCntPtr<AbstractStream> {
public:

	Stream() {}
	using RefCntPtr<AbstractStream>::RefCntPtr;


	///The Stream can be used as function returning next char when it is called
	/** Just call the stream as function, and it returns next char
	 *
	 * @retval >=0 next byte in the stream
	 * @retval -1 end of stream (no more data)
	 *
	 * */

	int operator()() {
		ptr->readByte();
	}

	///The Stream can be used as function to write next byte to the output stream
	/**
	 * @param b byte to write. Note that value can be in range 0-255, writting the value
	 *  outside of this range is not defined (probably it will write only lowest 8 bits)
	 */
	void operator()(int b) {
		ptr->writeByte((unsigned char)b);
	}

	///Reads next byte without removing it from the stream
	/**
	 * @retval >=0 value of the next byte
	 * @retval -1 end of stream (no more data)
	 */
	int peek() const {
		ptr->peekByte();
	}
	///Reads some bytes from the stream
	/**
	 * Function reads as much possible bytes from the stream without blocking
	 * or additional blocking.
	 *
	 * @param nonblock set this value to true, and function will read bytes without blocking.
	 *    The default value is false, which can cause blocking in case that input buffers
	 *    are empty so, the stream must block the thread until next data arrives. In case
	 *    blocking, the function garantees that everytime it blocks, it will return a buffer
	 *    with at least one byte.
	 *
	 * @return A view witch refers the buffer with data read from the stream. The buffer is
	 * allocated by the stream and it is large enough to asure optimal performance. The
	 * function can return empty view which can be interpreted as EOF (end of stream) in case
	 * that nonblock is false. If the nonblock is true, then the empty view can also mean
	 * that there are no more bytes available to read in non-blocking mode. If you need to
	 * distinguish between EOF and empty buffer, you need to call isEof() function
	 * with the returned view.
	 */
	BinaryView read(bool nonblock=false) {
		return ptr->read(nonblock);
	}

	///Puts back the part of the buffer, so next read will read it back
	/**
	 * Because read() always returns whole available buffer, which may contain mix of
	 * data that are not currently required, the rest of unused buffer can be put back
	 * to the stream, so the next read() will return it.
	 *
	 * @param data whole or parth of buffer to put back. Note that you can call this function
	 * only once after read(), multiple calls causes, that only buffer from the last call
	 * will be used. You can also put back different buffer, or complete different data, but
	 * in this case, you must ensure, that the buffer will be still valid in time
	 * of the reading and processing of that data
	 *
	 * @note Function is primarily used to put back part of the buffer which has not been
	 * processed. Because the data are still in possession of the stream, no
	 * allocation is needed. However, the function doesn't check the argument, so it is
	 * possible to put back different buffer. In this case, you must not destroy the
	 * data while it can be still used.
	 *
	 * @note you cannot push back EOF by this function. Use putBackEof()
	 *
	 * @note this function will not complete the blocking read. Note that function
	 * is not MT safe, so calling it during pending reading can result to unpredictable
	 * behaviour
	 */
	void putBack(const BinaryView &data) {
		return ptr->putBack(data);
	}

	///Puts backs one byte
	/** Function cannot put back any byte, it just returns latest read byte. Function
	 * can be called only after calling the function to read bytes, only once and
	 * only if there weren't EOF, otherwise it can cause inpredictable result
	 */
	void putBackByte() {
		return ptr->putBackByte();
	}
	///Puts back EOF, so it will appear that stream is ending
	/** This function causes, that next read will return EOF. However it is possible to
	 * complete any blocking operation. Function is MT safe.
	 */
	void putBackEof() {
		return ptr->putBackEof();
	}
	///Closes the output part of the stream by writing EOF
	/** the other side will receive EOF
	 *
	 * @note function also performs implicit flush of the all buffers, calling the flush()
	 * explicitly is not necesery
	 *
	 *  */
	void  closeOutput() {
		ptr->writeEof();
	}
	///Closes the output part of the stream by writing EOF
	/** the other side will receive EOF
	 *
	 * @note function also performs implicit flush of the all buffers, calling the flush()
	 * explicitly is not necesery
	 *
	 *  */
	void writeEof() {
		ptr->writeEof();
	}

	///flushes the internal output buffer
	/**
	 * @param wr use write mode to flush the data
	 *
	 * @p writeNonBlock try to flush without blocking (can do nothing)
	 * @p writeCanBlock try to flush but allow block operation at the begining
	 * @p writeWholeBuffer flush whole internal buffer
	 * @p writeAndFlush flush the whole internal buffer and also request to flush
	 * any transparent buffers on the way, request the sync with filesystem and doesn't
	 * return until operation completes
	 */
	void flush(WriteMode wr = writeAndFlush) {
		ptr->flush(wr);
	}
	MutableBinaryView getWriteBuffer(std::size_t reqSize = AbstractStream::minimumRequiredBufferSize) {
		return ptr->getWriteBuffer(reqSize);
	}
	void commitWriteBuffer(std::size_t commitSize) {
		return ptr->commitWriteBuffer(commitSize);
	}
	BinaryView write(const BinaryView &buffer, WriteMode wrmode = writeWholeBuffer) {
		return ptr->write(buffer, wrmode);
	}
	int setIOTimeout(int timeoutms) {
		return ptr->setIOTimeout(timeoutms);
	}

	bool waitForInput(int timeout) {
		return ptr->waitForInput(timeout);
	}

	bool waitForOutput(int timeout) {
		return ptr->waitForOutput(timeout);
	}
};



}
