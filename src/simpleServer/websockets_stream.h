#pragma once
#include <random>
#include <mutex>
#include "shared/refcnt.h"
#include "stringview.h"

#include "websockets_parser.h"

#include "abstractStream.h"




namespace simpleServer {

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;

namespace _details {


class WebSocketStreamImpl: public RefCntObj, public WebSocketsConstants, public WebSocketParser {
public:


	explicit WebSocketStreamImpl(Stream stream):stream(stream),serializer(WebSocketSerializer::server()) {}
	WebSocketStreamImpl(Stream stream, std::default_random_engine &randomEngine):stream(stream),serializer(WebSocketSerializer::client(randomEngine)) {}


	template<typename Fn>
	void readAsync(const Fn &fn);

	bool read( bool nonblock = false);


	void close(int code = closeNormal);
	void ping(const BinaryView &data);
	void postText(const StrViewA &data);
	void postBinary(const BinaryView &data);

	template<typename Fn>
	void closeAsync(int code, const Fn &fn);
	template<typename Fn>
	void pingAsync(const BinaryView &data, const Fn &fn);
	template<typename Fn>
	void postTextAsync(const StrViewA &data, const Fn &fn);
	template<typename Fn>
	void postBinaryAsync(const BinaryView &data, const Fn &fn);

	const Stream getStream() const {return stream;}

protected:
	Stream stream;
	WebSocketSerializer serializer;
	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;

	template<typename Fn>
	class CallAfterRead {
		Fn fn;
		RefCntPtr<WebSocketStreamImpl> owner;
	public:
		CallAfterRead(const Fn &fn, const RefCntPtr<WebSocketStreamImpl> &owner)
			:fn(fn),owner(owner) {}

		void operator()(AsyncState st, const BinaryView &data) const {
			if (st == asyncOK) {
				BinaryView rest = owner->parse(data);
				owner->stream.putBack(rest);
				if (owner->isComplette()) {
					owner->completeRead();
					fn(st);
				} else {
					owner->readAsync(fn);
				}
			} else {
				fn(st);
			}
		}
	};

	template<typename Fn>
	class UnlockAfterWrite {
		Fn fn;
		RefCntPtr<WebSocketStreamImpl> owner;
	public:
		UnlockAfterWrite (const Fn &fn, const RefCntPtr<WebSocketStreamImpl> &owner)
			:fn(fn),owner(owner) {}

		void operator()(AsyncState st, const BinaryView &data) const {
			if (st != asyncOK || data.empty()) {
				owner->lock.unlock();
				fn(st);
			} else {
				owner->stream.writeAsync(data,*this,false);
			}
		}

	};
	void completeRead() {
		switch (getFrameType()) {
			case WebSocketParser::connClose: close();break;
			case WebSocketParser::ping: {
				Sync _(lock);
				stream.write(serializer.forgePongFrame(getData()),writeAndFlush);
			}break;
			default:break;
		}
	}
};


template<typename Fn>
void WebSocketStreamImpl::readAsync(const Fn &fn) {
	stream.readAsync(CallAfterRead<Fn>(fn,this));

}
inline bool WebSocketStreamImpl::read(bool nonblock) {
	BinaryView b = stream.read(nonblock);
	if (IGeneralStream::isEof(b)) return false;
	if (b.empty()) return true;
	BinaryView c = parse(b);
	stream.putBack(c);
	if (isComplette()) {
		completeRead();
	}
	return getFrameType() != WebSocketParser::connClose;
}

inline void WebSocketStreamImpl::close(int code) {
	Sync _(lock);
	stream.write(serializer.forgeCloseFrame(code),writeAndFlush);
}
inline void WebSocketStreamImpl::ping(const BinaryView &data) {
	Sync _(lock);
	stream.write(serializer.forgePingFrame(data),writeAndFlush);
}
inline void WebSocketStreamImpl::postText(const StrViewA &data) {
	Sync _(lock);
	stream.write(serializer.forgeTextFrame(data),writeAndFlush);
}
inline void WebSocketStreamImpl::postBinary(const BinaryView &data) {
	Sync _(lock);
	stream.write(serializer.forgeBinaryFrame(data),writeAndFlush);
}

template<typename Fn>
void WebSocketStreamImpl::closeAsync(int code, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeCloseFrame(code),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStreamImpl::pingAsync(const BinaryView &data, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgePingFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStreamImpl::postTextAsync(const StrViewA &data, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeTextFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStreamImpl::postBinaryAsync(const BinaryView &data, const Fn &fn){
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeBinaryFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}

}


///Websocke stream - parses and serializes websocket frames to/from messages
class WebSocketStream : public RefCntPtr<_details::WebSocketStreamImpl> {
public:

	using RefCntPtr<_details::WebSocketStreamImpl>::RefCntPtr;

	///Returns underlying stream
	/** You should not perform I/O operations on that stream otherwise it can
	 *  corrupt communication and put it out of sync.
	 *
	 *  However it is possible to use the stream to control or block threads depend
	 *  on whether the data are prepared or not.
	 *
	 *  You can also need this function to set or modify the asynchronous provider
	 *  for the underying socket
	 */
	const Stream getStream() const {return ptr->getStream();}


	///Read next message asynchronously
	/** Function starts to reading message asynchronously. The underlying socket
	 * must have defined an asychnrooous provdider, otherwise the function fails
	 *
	 * @param fn function called when the message is ready to collect. The function
	 * has following prototype void(AsyncState)
	 *
	 */
	template<typename Fn>
	void readAsync(const Fn &fn) {ptr->readAsync(fn);}

	///Read next message synchronously
	/** Function tries to read data a collect enough bytes to complete the message.
	 * Function don't need to complete the message by one call. An additional calls
	 * can be required to complete.
	 *
	 * @param nonblock perform nonblocking operation. In this case, function will
	 * not wait if the stream has no data to collect.
	 *
	 * @retval true stream continues
	 * @retval false connection has been closed
	 *
	 * @note to detect whether message has been completted, use the function isComplete()
	 *
	 *
	 * @code
	 * WebSocketStream s = ....;
	 * while (s.read()) {
	 * 		if (s.isComplete()) {
	 * 		   //... process the message here
	 *      }
	 * }
	 * @endcode
	 */
	bool read( bool nonblock = false) {ptr->read(nonblock);}

	///Send close request
	/**
	 * Before the websocket conection is closed, the close() should be called
	 *
	 * @param code reason of closing connecton
	 *
	 * @note function just sends the "closing frame", it doesn't closing connection.
	 *
	 * @note function is MT safe
	 */
	void close(int code = WebSocketsConstants::closeNormal) {ptr->close(code);}
	///Send ping request
	/**
	 * @param data user payload
	 *
	 * @note function is MT safe
	 */
	void ping(const BinaryView &data) {ptr->ping(data);}
	///Send text message
	/**
	 * @param contans text to send
	 *
	 * @note function is MT safe
	 */
	void postText(const StrViewA &data) {ptr->postText(data);}
	///Send binary message
	/**
	 * @param contans binary data to send
	 *
	 * @note function is MT safe
	 */
	void postBinary(const BinaryView &data) {ptr->postBinary(data);}

	///Send close request asynchronously
	/**
	 * Before the websocket conection is closed, the close() should be called
	 *
	 * @param code reason of closing connecton
	 * @param fn function called once the frame is send to the network
	 *
	 * @note function just sends the "closing frame", it doesn't closing connection.
	 *
	 * @note function is MT safe. However, calling any sending function including
	 * the asynchronous version block the call unless the pending operation
	 * is complete. The function has prototype void(AsyncState)
	 */
	template<typename Fn>
	void closeAsync(int code, const Fn &fn) {ptr->closeAsync(code,fn);}
	///Send ping request asynchronously
	/**
	 *
	 * @param data payload
	 * @param fn function called once the frame is send to the network
	 *
	 * @note function is MT safe. However, calling any sending function including
	 * the asynchronous version block the call unless the pending operation
	 * is complete.
	 */
	template<typename Fn>
	void pingAsync(const BinaryView &data, const Fn &fn) {ptr->pingAsync(data,fn);}
	///Send text request asynchronously
	/**
	 *
	 * @param data payload
	 * @param fn function called once the frame is send to the network
	 *
	 * @note function is MT safe. However, calling any sending function including
	 * the asynchronous version block the call untill the pending operation
	 * is complete.
	 */
	template<typename Fn>
	void postTextAsync(const StrViewA &data, const Fn &fn) {ptr->postTextAsync(data,fn);}
	///Send binary request asynchronously
	/**
	 *
	 * @param data payload
	 * @param fn function called once the frame is send to the network
	 *
	 * @note function is MT safe. However, calling any sending function including
	 * the asynchronous version block the call until the pending operation
	 * is complete.
	 */
	template<typename Fn>
	void postBinaryAsync(const BinaryView &data, const Fn &fn) {ptr->postBinaryAsync(data,fn);}

	///Determines, whether receiving messsage is complete
	/** You should check complete status after every read(), otherwise the message
	 * can be discarded
	 *
	 * @retval true message is complete
	 * @retval false more data are needed
	 */
	bool isComplete() const {return ptr->isComplette();}


	///Retrieves type of frame has been received
	/**
	 *@retval incomplette the frame is not complete yet (isComplete() = false)
	 *@retval text recieved a text message
	 *@retval binary receiuved a binary message
	 *@retval connClose received connection close request (note that the object automatically responds to close)
	 *@retval ping receieved ping (note that the object handles pong response automatically)
	 *@retval pong pong received
	 *
	 */
	WebSocketParser::FrameType getFrameType() const {return ptr->getFrameType();}

	///Retrieve data as binary view
	/** Function retrieves data as binary view*/
	BinaryView getData() const {return ptr->getData();}

	///Retrieve data as text view
	StrViewA getText() const {return ptr->getText();}

	///Get code (for opcodeConnClose)
	unsigned int getCode() const {return ptr->getCode();}

};


}
