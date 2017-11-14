#pragma once
#include <mutex>
#include "shared/refcnt.h"
#include "stringview.h"

#include "websockets_parser.h"

#include "abstractStream.h"




namespace simpleServer {

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;



class WebSocketStream: public RefCntObj, public WebSocketsConstants {
public:


	explicit WebSocketStream(Stream stream):stream(stream),serializer(WebSocketSerializer::server()) {}
	WebSocketStream(Stream stream, std::default_random_engine &randomEngine):stream(stream),serializer(WebSocketSerializer::client(randomEngine)) {}


	template<typename Fn>
	void readAsync(const Fn &fn);

	template<typename Fn>
	bool read(const Fn &fn, bool nonblock = false);


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

protected:
	Stream stream;
	WebSocketParser parser;
	WebSocketSerializer serializer;
	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;

	template<typename Fn>
	class CallAfterRead {
		Fn fn;
		RefCntPtr<WebSocketStream> owner;
	public:
		CallAfterRead(const Fn &fn, const RefCntPtr<WebSocketStream> &owner)
			:fn(fn),owner(owner) {}

		void operator()(AsyncState st, const BinaryView &data) const {
			if (st == asyncOK) {
				BinaryView rest = owner->parser.parse(data);
				stream.putBack(rest);
				if (owner->parser.isComplette()) {
					owner->completeRead();
					const WebSocketParser *p = &owner->parser;
					fn(st,p);
				} else {
					owner->readAsync(fn);
				}
			} else {
				fn(st, nullptr);
			}
		}
	};

	template<typename Fn>
	class UnlockAfterWrite {
		Fn fn;
		RefCntPtr<WebSocketStream> owner;
	public:
		UnlockAfterWrite (const Fn &fn, const RefCntPtr<WebSocketStream> &owner)
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
		const WebSocketParser *p = &parser;
		switch (p->getFrameType()) {
			case WebSocketParser::connClose: close();break;
			case WebSocketParser::ping: {
				Sync _(lock);
				stream.write(serializer.forgePongFrame(parser.getData()),writeAndFlush);
			}break;
			default:break;
		}
	}
};


template<typename Fn>
void WebSocketStream::readAsync(const Fn &fn) {
	stream.readAsync(CallAfterRead<Fn>(fn,this));

}
template<typename Fn>
bool WebSocketStream::read(const Fn &fn, bool nonblock) {
	BinaryView b = stream.read(nonblock);
	if (IGeneralStream::isEof(b)) return false;
	if (b.empty()) return true;
	BinaryView c = parser.parse(b);
	stream.putBack(c);
	if (parser.isComplette()) {
		completeRead();
		const WebSocketParser &p = parser;
		fn(p);
	}
	return parser.getFrameType() != WebSocketParser::connClose;
}

inline void WebSocketStream::close(int code) {
	Sync _(lock);
	stream.write(serializer.forgeCloseFrame(code),writeAndFlush);
}
inline void WebSocketStream::ping(const BinaryView &data) {
	Sync _(lock);
	stream.write(serializer.forgePingFrame(data),writeAndFlush);
}
inline void WebSocketStream::postText(const StrViewA &data) {
	Sync _(lock);
	stream.write(serializer.forgeTextFrame(data),writeAndFlush);
}
inline void WebSocketStream::postBinary(const BinaryView &data) {
	Sync _(lock);
	stream.write(serializer.forgeBinaryFrame(data),writeAndFlush);
}

template<typename Fn>
void WebSocketStream::closeAsync(int code, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeCloseFrame(code),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStream::pingAsync(const BinaryView &data, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgePingFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStream::postTextAsync(const StrViewA &data, const Fn &fn) {
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeTextFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}
template<typename Fn>
void WebSocketStream::postBinaryAsync(const BinaryView &data, const Fn &fn){
	lock.lock();
	try {
		stream.writeAsync(serializer.forgeBinaryFrame(data),UnlockAfterWrite<Fn>(fn,this));
	} catch (...) {
		lock.unlock();
	}
}


}
