
#pragma once
#include "shared/refcnt.h"
#include "shared/stringview.h"
#include "asyncProvider.h"
#include "exceptions.h"


namespace simpleServer {

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;
using ondra_shared::StringView;
using ondra_shared::StrViewA;
using ondra_shared::MutableBinaryView;
using ondra_shared::BinaryView;

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
class AbstractStream;
class Stream;


///Very abstract interface which descibes protocol to access data in the stream
class IGeneralStream {
public:

	typedef std::function<void(AsyncState, BinaryView)> Callback;


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
		WrBuffer(const MutableBinaryView &b, std::size_t wrpos = 0):ptr(b.data),size(b.length),wrpos(wrpos) {}
		WrBuffer(unsigned char *ptr, std::size_t bufferSize, std::size_t wrpos = 0)
			:ptr(ptr)
			,size(bufferSize)
			,wrpos(wrpos) {}
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
	 *
	 * @note function doesn't support putBack() operation declared on AbstractStream. It is
	 * supposed to be used when buffering is handled externally
	 */
	virtual BinaryView implRead(bool nonblock) = 0;

	///Read to buffer
	/** Function reads to an external buffer
	 *
	 * @param buffer buffer should contain at least one byte
	 * @param nonblock set true to perform nonblocking operation
	 * @return view of read data
	 *
	 *
	 * @note this function can be faster in some cases, because it transfers data directly
	 * from the stream to the buffer. Streams with internal buffer can be slower. Use this
	 * function if you need just only transfer data from the stream to existing buffer
	 */
	virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) = 0;

	///Write buffer to the stream
	/** @param buffer Writes buffer to the output stream
	 * @param nonblock specify true and function can return immediately if nonblocking operation
	 * cannot be performed. Otherwise function can block to write at-least one byte from the
	 * buffer
	 *
	 *
	 * @return A view to part of buffer written. Buffer is always filled from the beginning
	 * (you can use .length property to determine count of bytes). However if EOF
	 * is reached, an empty buffer is returned and you need to pass the result to function
	 * isEof() to determine whether EOF has been actually reached.
	 *
	 * @note this function doesn't provide buffering. It is supposed to be used when buffering is
	 * handled externally
	 *
	 */
	virtual BinaryView implWrite(BinaryView buffer, bool nonblock) = 0;



	///Writes part of internal output buffer
	/**
	 * @param curBuffer informations about buffer. Structure should contain some
	 *   informations about already stored data
	 * @param writeMode choose one of write mode. Function updates this structure
	 *
	 */
	virtual bool implWrite(WrBuffer &curBuffer, bool nonblock) = 0;

	///Read to the buffer asynchronoysly
	/**
	 * @param cb callback function
	 *
	 * @note function uses internal buffer.
	 *
	 * @note doesn't support putBack() function. Use only if you need direct access to the stream
	 */
	virtual void implReadAsync(Callback &&cb) = 0;

	///Read to the buffer asynchronously
	/**
	 * @param buffer reference to a buffer. Please ensure, that buffer is not destroyed while
	 * operation is in progress
	 *
	 * @param cb callback function called upon completion
	 */
	virtual void implReadAsync(const MutableBinaryView &buffer, Callback &&cb) = 0;


	///Write buffer asynchronously
	/**
	 * @param data buffer to write. Please ensure, that buffer is not destroyed while
	 * operation is in progress
	 *
	 * @param cb callback function called upon completion
	 *
	 * @note function doesn't support caching
	 */
	virtual void implWriteAsync(const BinaryView &data, Callback &&cb) = 0;



	///Request to block thread until there is data available for read
	/**
	 * @param timeout specifies timeout in miliseconds. A negative value is interpreted as
	 *  infinite wait. Operation can be interruped by closing the input
	 */
	virtual bool implWaitForRead(int timeoutms) = 0;
	///Request to block thread while there is no space in the output buffer
	/**
	 * 	 @param timeout specifies timeout in miliseconds. A negative value is interpreted as
	 *  infinite wait.
	 */

	virtual bool implWaitForWrite(int timeoutms) = 0;

	virtual void implCloseInput() = 0;
	virtual void implCloseOutput() = 0;


	virtual bool implFlush() = 0;


	virtual ~IGeneralStream() {}


	///DirectWrite interface is prepared for stream proxies to bypass buffering
	/** there is only write interface, because reading uses buffer limited to
	 * putBack() feature and
	 */
	class DirectWrite {
	protected:
		DirectWrite(IGeneralStream &owner):owner(owner) {}
		IGeneralStream &owner;
	public:
		bool write(WrBuffer &curBuffer, bool nonblock) {
			return owner.implWrite(curBuffer, nonblock);
		}
		BinaryView write(BinaryView buffer, bool nonblock) {
			return owner.implWrite(buffer,nonblock);
		}
		void writeAsync(const BinaryView &data, Callback &&cb) {
			return owner.implWriteAsync(data,std::move(cb));
		}
		bool flush() {
			return owner.implFlush();
		}


		friend class AbstractStream;
	};



protected:

	///Constant which should be returned by readBuffer() when reading reaches to EOF
	static BinaryView eofConst;

};


///AbstractStream brings some additional function which halps to work with streams and buffers.
/** Streams should extend this class, not IGenerlStream. The class also allows to ref-counted
 * references to the stream and expects that streams will be allocated by operator new  */
class AbstractStream: public RefCntObj, public IGeneralStream {
public:


	~AbstractStream() noexcept {}


	BinaryView read(bool nonblock = false) {
		if (rdBuff.empty()) {
			return implRead(nonblock);
		} else {
			BinaryView t = rdBuff;
			rdBuff = BinaryView();
			return t;
		}
	}

	BinaryView read(const MutableBinaryView &buffer, bool nonblock = false) {
		if (rdBuff.empty()) {
			return  implRead(buffer, nonblock);
		} else {
			BinaryView t = rdBuff.substr(0,buffer.length);
			copydata(buffer.data, t.data, t.length);
			rdBuff = rdBuff.substr(t.length);
			return BinaryView(buffer.data, t.length);
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
			rdBuff = implRead(false);
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
			rdBuff = implRead(false);
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
	 * @return part of buffer not written. Exception: If the function returns empty buffer, which is positive on isEof(),
	 * it does mean, that peer connection has been reset, and data has not been send. Because the return value
	 * is also empty, not-checking it causes, that writting continues without any reported error but into /dev/null
	 *
	 * Function uses buffer to collect small writes into single brust write.
	 */
	BinaryView write(const BinaryView &buffer, WriteMode wrmode = writeWholeBuffer) {
		if (buffer.empty()) return BinaryView(0,0);
		if (buffer.data == wrBuff.ptr+wrBuff.wrpos) {
			wrBuff.wrpos+=buffer.length;
			return BinaryView(0,0);
		}

		if (wrBuff.remain() == 0) {
			auto res = implWrite(wrBuff,wrmode == writeNonBlock);
			if (!res) return eofConst;
			if (wrBuff.remain() == 0)
				return buffer;
		}
		BinaryView wrb;
		bool rep = (wrmode == writeWholeBuffer || wrmode == writeAndFlush);
		if (wrBuff.wrpos == 0 && buffer.length > wrBuff.size) {
			 wrb = implWrite(buffer, wrmode == writeNonBlock);
		} else {
			auto remain = wrBuff.remain();
			BinaryView p1 = buffer.substr(0,remain);
			wrb = buffer.substr(p1.length);
			copydata(wrBuff.ptr+wrBuff.wrpos, buffer.data, p1.length);
			wrBuff.wrpos+= p1.length;
		}
		while (!wrb.empty() && rep) {
			wrb = write(wrb, writeCanBlock);
		}
		if (wrmode == writeAndFlush) {
			flush(writeAndFlush);
		}
		return wrb;
	}


	///write one byte to the output stream.
	/**
	 * @param b byte to write. note that writes are buffered, so you will need to call flush()
	 * to force to send everything in the stream.
	 *
	 * @retval true success
	 * @retval false connection lost
	 */
	bool writeByte(unsigned char b) {
		if (wrBuff.remain() == 0) {
			if (!implWrite(wrBuff, false)) return false;
		}
		wrBuff.ptr[wrBuff.wrpos] = b;
		wrBuff.wrpos++;
		return true;
	}



	///Flush output buffer to the stream
	/**
	 *
	 * @param wrmode write mode
	 *
	 * @retval true flushed
	 * @retval false connection lost (EPIPE)
	 */
	bool flush(WriteMode wrmode = writeAndFlush) {
		bool ok = true;
		if (wrBuff.wrpos) {
			bool rep = (wrmode == writeWholeBuffer || wrmode == writeAndFlush);
			do {
				ok = ok && implWrite(wrBuff, wrmode == writeNonBlock);
			} while (wrBuff.wrpos != 0 && rep && ok);
			if (wrmode == writeAndFlush) {
				ok = ok && implFlush();
			}
		} else if (wrmode == writeAndFlush) {
			return implFlush();
		}
		return ok;
	}


	///writes EOF mark to the output stream
	/** it finishes writing and closes output stream. Other side receives EOF. After
	 * this function, no more data are expected
	 *
	 * This function is called automatically when last instance of the stream is destroyed
	 */
	void writeEof() {
		flush(writeWholeBuffer);
		implCloseOutput();
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
		implCloseInput();
	}

	static MutableBinaryView noOutputMode();


	bool waitForInput(int timeout) {
		if (rdBuff.empty()) return implWaitForRead(timeout);
		else return true;
	}

	bool waitForOutput(int timeout) {
		if (wrBuff.remain()) return implWaitForWrite(timeout);
		else return true;
	}


	AsyncProvider setAsyncProvider(AsyncProvider asyncProvider);

	///Returns true, if asynchronous operations are available
	/**
	 * @retval false asynchronous operations cannot be used. Try to call setAsyncProvider, however
	 * some streams doesn't support asynchronous operations at all
	 *
	 * @retval true asynchronous operations are available
	 */
	virtual bool canRunAsync() const;


	AsyncProvider getAsyncProvider() const {return asyncProvider;}

	template<typename Fn>
	class AsyncDirectCall {
	public:

		AsyncDirectCall(AsyncState state, const BinaryView &data, const Fn &fn):state(state),data(data),fn(fn) {}
		void operator()() const {
			fn(state, data);
		}

	protected:
		AsyncState state;
		BinaryView data;
		Fn fn;

	};

	///Read asynchronously
	/**
	 * @param callbackFn function called when operation completes.
	 *
	 * @note Function always transfer execution to the asynchronous thread even if there
	 * already data available to process immediately. This also means, that function is
	 * always slower then synchronous alternative
	 *
	 *
	 */
	template<typename Fn>
	void readAsync(Fn &&callbackFn) {
		if (asyncProvider == nullptr && canRunAsync()) {
			implReadAsync(std::forward<Fn>(callbackFn));
		} else {
			BinaryView data = read(true);
			bool eof = isEof(data);
			if (!eof && data.empty()) {
				implReadAsync(std::forward<Fn>(callbackFn));
			} else {
				asyncProvider.runAsync([=,callbackFn=std::move(callbackFn)] ()mutable {
					callbackFn(eof?asyncEOF:asyncOK,data);
				});

			}
		}
	}

	template<typename Fn>
	void readAsync(const MutableBinaryView &b, Fn &&callbackFn) {
		if (asyncProvider == nullptr && canRunAsync()) {
			implReadAsync(b, std::forward<Fn>(callbackFn));
		} else {
			BinaryView data = read(b,true);
			bool eof = isEof(data);
			if (!eof && data.empty()) {
				implReadAsync(b, std::forward<Fn>(callbackFn));
			} else {
				asyncProvider.runAsync([=]{
					callbackFn(eof?asyncEOF:asyncOK,data);
				});
			}
		}
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
	 *
	 *  @note Function always transfer execution to the asynchronous thread even if there
	 * already data available to process immediately. This also means, that function is
	 * always slower then synchronous alternative
	 *
	 */
	template<typename Fn>
	void writeAsync(BinaryView data, Fn &&callbackFn, bool all= false) {

		BinaryView remainData = write(data,writeNonBlock);
		if (remainData == data) {
			implWriteAsync(wrBuff.getView(),[=](AsyncState status, BinaryView remain) {
				if (status == asyncOK) {
					wrBuff.wrpos = 0;
					write(remain, writeNonBlock);
					BinaryView x = write(data, writeNonBlock);
					if (all && !x.empty()) {
						writeAsync(x, std::forward<Fn>(callbackFn), all);
					} else {
						callbackFn(status,x);
					}
				} else {
					callbackFn(status,remain);
				}
			});
		} else {
				callbackFn(asyncOK,remainData);
		}

	}


	DirectWrite getDirectWrite() {
		flush(writeWholeBuffer);
		return DirectWrite(*this);
	}




protected:



	BinaryView rdBuff;
	WrBuffer wrBuff;

	AsyncProvider asyncProvider;




	///executes function through asyncProvider so it appears executed asynchronously
	template<typename Fn>
	void fakeAsync(AsyncState st, const BinaryView &data, const Fn &fn) {
		if (asyncProvider == nullptr) throw NoAsyncProviderException();
		asyncProvider.runAsync(AsyncDirectCall<Fn>(st,data, fn));
	}

	void copydata(unsigned char *target, const unsigned char *source, std::size_t count);

//OD	template<typename TX> friend class RefCntPtr;


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

	int operator()() const {
		return (*this)->readByte();
	}

	///The Stream can be used as function to write next byte to the output stream
	/**
	 * @param b byte to write. Note that value can be in range 0-255, writting the value
	 *  outside of this range is not defined (probably it will write only lowest 8 bits)
	 */
	void operator()(int b) const {
		(*this)->writeByte((unsigned char)b);
	}

	///Reads next byte without removing it from the stream
	/**
	 * @retval >=0 value of the next byte
	 * @retval -1 end of stream (no more data)
	 */
	int peek() const {
		return (*this)->peekByte();
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
	BinaryView read(bool nonblock=false) const {
		return (*this)->read(nonblock);
	}

	BinaryView read(const MutableBinaryView &buffer, bool nonblock=false) const {
		return (*this)->read(buffer, nonblock);
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
	void putBack(const BinaryView &data) const {
		return (*this)->putBack(data);
	}

	///Puts backs one byte
	/** Function cannot put back any byte, it just returns latest read byte. Function
	 * can be called only after calling the function to read bytes, only once and
	 * only if there weren't EOF, otherwise it can cause inpredictable result
	 */
	void putBackByte() const {
		return (*this)->putBackByte();
	}
	///Puts back EOF, so it will appear that stream is ending
	/** This function causes, that next read will return EOF. However it is possible to
	 * complete any blocking operation. Function is MT safe.
	 */
	void putBackEof() const {
		return (*this)->putBackEof();
	}
	///Closes the output part of the stream by writing EOF
	/** the other side will receive EOF
	 *
	 * @note function also performs implicit flush of the all buffers, calling the flush()
	 * explicitly is not necesery
	 *
	 *  */
	void  closeOutput() const {
		(*this)->writeEof();
	}
	///Closes the output part of the stream by writing EOF
	/** the other side will receive EOF
	 *
	 * @note function also performs implicit flush of the all buffers, calling the flush()
	 * explicitly is not necesery
	 *
	 *  */
	void writeEof() const {
		(*this)->writeEof();
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
	bool flush(WriteMode wr = writeAndFlush) const {
		return (*this)->flush(wr);
	}
	BinaryView write(const BinaryView &buffer, WriteMode wrmode = writeWholeBuffer) const {
		return (*this)->write(buffer, wrmode);
	}
	int setIOTimeout(int timeoutms) const {
		return (*this)->setIOTimeout(timeoutms);
	}

	bool waitForInput(int timeout) const {
		return (*this)->waitForInput(timeout);
	}

	bool waitForOutput(int timeout) const {
		return (*this)->waitForOutput(timeout);
	}
	template<typename Fn>
	void readAsync(Fn &&completion) const {
		(*this)->readAsync(std::forward<Fn>(completion));
	}
	template<typename Fn>
	void readAsync(const MutableBinaryView &buffer, Fn &&completion) const {
		(*this)->readAsync(buffer,std::forward<Fn>(completion));
	}
	template<typename Fn>
	void writeAsync(const BinaryView &data, const Fn &completion, bool alldata = false) const {
		(*this)->writeAsync(data, completion, alldata);
	}

	AsyncProvider setAsyncProvider(AsyncProvider asyncProvider) const {
		return (*this)->setAsyncProvider(asyncProvider);
	}
	AsyncProvider getAsyncProvider() const {
		return (*this)->getAsyncProvider();
	}

	bool canRunAsync() const {
		return (*this)->canRunAsync();
	}

	const Stream &operator << (StrViewA text) const {
		write(BinaryView(text));
		return *this;
	}

	const Stream &operator << (std::intptr_t x) const {
		if (x < 0) {
			operator()('-');
			x = -x;
		}
		char buffer[100];
		if (x == 0) operator()('0');
		char *p = buffer+sizeof(buffer);
		char *b = p;
		while (x) {
			p--;
			*p=(x%10) + '0';
			x/=10;
		}
		write(BinaryView(StrViewA(p,b-p)));
		return *this;
	}

	std::string toString(std::size_t limit = (std::size_t)-1, bool nonblock = false) {
		std::string x;
		BinaryView b = read(nonblock);
		while (!b.empty()) {
			if (b.length > limit) {
				putBack(b.substr(limit));
				b = b.substr(0,limit);
				x.append(reinterpret_cast<const char *>(b.data),b.length);
				break;
			} else {
				x.append(reinterpret_cast<const char *>(b.data),b.length);
				b = read(nonblock);
			}

		}
		return x;
	}

};



}
