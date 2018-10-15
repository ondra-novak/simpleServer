#pragma once
#include <map>
#include <unordered_map>

#include "abstractStream.h"
#include "shared/stringpool.h"

#include "http_headervalue.h"

#include "shared/stringview.h"

namespace simpleServer {

using ondra_shared::StringPool;
using ondra_shared::StringView;


struct CmpHeaderKeys {
	bool operator()(StrViewA a, StrViewA b) const;
};


///The class parses and stores headers read from the stream
/**
 * It expects that every header block contains a first line which carries request or status, followed by
 * many lines in form "key: value". The block is terminated by the empty line
 *
 * The object is fed ethier from a stream or from buffers. In case of stream, synchronous or asynchronous mode
 * can be choosen. In case of buffers, it expects, that series of buffer will be send to it while the object
 * can report that enough informations has been collected and put back the buffer containing unused extra
 * data (beyond the header);
 */
class ReceivedHeaders {
public:


	typedef std::map<StrViewA, HeaderValue, CmpHeaderKeys> HdrMap;

	///Begin of the headers
	HdrMap::const_iterator begin() const;
	///End of the headers
	HdrMap::const_iterator end() const;

	///Retrieves header value
	HeaderValue operator[](StrViewA key) const;

	void clear();


	bool parse(const BinaryView &data, BinaryView &putBack);
	bool parse(const Stream &stream);
	void parseAsync(const Stream &stream, const std::function<void(AsyncState)> &callback);


	StrViewA getFirstLine() const;

	ReceivedHeaders() {}
	ReceivedHeaders(const ReceivedHeaders& other);
	void operator=(const ReceivedHeaders& ) = delete;

protected:

	std::vector<char> requestHdrLineBuffer;
	HdrMap hdrMap;
	StrViewA reqLine;


	bool acceptLine(const BinaryView &data, BinaryView &putBack);
	void parseHeaders(const StrViewA &hdr);


};


///The class helps to build header block for the request or response
/**
 * Similar to ReceivedHeaders, you can have headers containing a first line followed by header block terminated by an empty line
 *
 */
class SendHeaders {
public:

	///construct header
	SendHeaders() {}
	SendHeaders(const SendHeaders &other) {
		firstLine = pool.add(other.getFirstLine());
		other.forEach([&](const StrViewA &key, const StrViewA &value){
			operator()(key, value);
		});
	}
	SendHeaders(SendHeaders &&other)
			:pool(std::move(other.pool))
			,hdrMap(std::move(other.hdrMap))
			,firstLine(std::move(other.firstLine)) {
		firstLine.relocate(pool);
		for (auto &&k : hdrMap) {
			k.first.relocate(pool);
			k.second.relocate(pool);
		}
	}

	SendHeaders(const StrViewA &key, const StrViewA &value) {this->operator ()(key,value);}

	explicit SendHeaders(StrViewA firstLine) {
		request(firstLine);
	}
	///Easily add new header, just call this object as function with key and value arguments
	/**
	 * Because the object returns itself, you can chain calls
	 *
	 * hdr("aaa","bbb")
	 *    ("ccc","ddd")
	 *    ("eee","fff")
	 *
	 * @param key key
	 * @param value value
	 */
	SendHeaders &&operator()(const StrViewA key, const StrViewA value);
	///Put content-length
	/**
	 * Function is equivalent to ("Content-Length",sz)
	 *
	 * @param sz size of content
	 */
	SendHeaders &&contentLength(std::size_t sz);
	///Put content-type
	/**
	 * Function is equivalent to ("Content-Type",str)
	 *
	 * @param str type of content
	 */
	SendHeaders &&contentType(StrViewA str);
	///Sets status line
	/**
	 * Status line is the first line in header block
	 * @param str string containing whole status line
	 * @note function is alias to request(), use appropriate function for descriptive reasons
	 */
	SendHeaders &&status(StrViewA str);
	///Sets status line
		/**
		 * Request line is the first line in header block
		 * @param str string containing whole request line
		 * @note function is alias to status(), use appropriate function for descriptive reasons
		 */
	SendHeaders &&request(StrViewA str);

	SendHeaders &&cacheFor(std::size_t seconds);

	SendHeaders &&cacheForever();

	SendHeaders &&disableCache();

	SendHeaders &&eTag(StrViewA str);


	///Clear content of object
	void clear();


	///Enumerate all headers
	template<typename Fn>
	void forEach(const Fn &fn) const {
		for (auto &&x : hdrMap) {
			fn(x.first.getView(), x.second.getView());
		}
	}


	///Generate header block
	/**
	 * @param fn function which receives BinaryView contains parts of the headers. It expects
	 * that function sends these buffers directly to the stream
	 *
	 * @note It is still preferred to use buffered stream, because results don't need to have
	 * optimal length for the stream
	 */
	template<typename Fn>
	void generate(const Fn &fn) const {
		BinaryView crlf(StrViewA("\r\n"));
		BinaryView collon(StrViewA(": "));
		if (!firstLine.getView().empty()) {
			fn(BinaryView(firstLine.getView()));
			fn(crlf);
		}

		forEach([&](const StrViewA &key, const StrViewA &value){
			fn(BinaryView(key));
			fn(collon);
			fn(BinaryView(value));
			fn(crlf);
		});
		fn(crlf);

	}


	///Receive the first line
	StrViewA getFirstLine() const {
		return firstLine.getView();
	}

	void operator=(const SendHeaders &other) = delete;

	///Retrieves header value
	HeaderValue operator[](StrViewA key) const;
protected:
	typedef StringPool<char> Pool;
	typedef std::map<Pool::String, Pool::String, CmpHeaderKeys> HdrMap;



	Pool pool;
	HdrMap hdrMap;
	Pool::String firstLine;

};

}
