#pragma once

#include <functional>
#include <map>
#include <unordered_map>


#include "abstractStream.h"
#include "common/stringpool.h"
#include "http_headervalue.h"

#include "refcnt.h"
#include "stringview.h"

namespace simpleServer {

class HTTPRequest;
class HTTPRequestData;

typedef RefCntPtr<HTTPRequestData> PHTTPRequestData;



typedef std::function<void(HTTPRequest)> HTTPHandler;


class HTTPResponse {
public:

	explicit HTTPResponse(int code);
	HTTPResponse(int code, StrViewA response);

	HTTPResponse &header(const StrViewA key, const StrViewA value);
	HTTPResponse &contentLength(std::size_t sz);

	void clear();


	int getCode() const;
	StrViewA getStatusMessage() const;

	template<typename Fn>
	void forEach(const Fn &fn) const {
		for (auto &&x : hdrMap) {
			fn(x.first.getView(), x.second.getView());
		}
	}



protected:

	typedef StringPool<char> Pool;
	typedef std::map<Pool::String, Pool::String> HdrMap;



	Pool pool;
	HdrMap hdrMap;
};





class HTTPRequestData: public RefCntObj {
	struct Hash {
		std::size_t operator()(StrViewA text) const;
	};

	void parseHttpAsync(Stream stream, HTTPHandler handler);

public:

	typedef std::unordered_map<StrViewA, HeaderValue, Hash> HdrMap;

	///Begin of the headers
	HdrMap::const_iterator begin() const;
	///End of the headers
	HdrMap::const_iterator end() const;


	typedef std::function<void()> Callback;


	void parseHttp(Stream stream, HTTPHandler handler, bool keepAlive);


	///Retrieves header value
	HeaderValue operator[](StrViewA key) const;

	///Retrieves method
	/**
	 * @return String contains method name: GET, PUT, POST, OPTIONS, etc
	 */
	StrViewA getMethod() const;
	///Retrieves path
	/**
	 * @return string contains path from the request line (raw)
	 */
	StrViewA getPath() const;
	///Retrieves http version
	/**
	 * @return string contains HTTP version: HTTP/1.0 or HTTP/1.1
	 */
	StrViewA getVersion() const;
	///Retrieves whole request line
	StrViewA getRequestLine() const;
	///Forges full uri
	/**
	 * @param secure set true if the connection is secured, otherwise false
	 * @return full URI
	 *
	 * @note function just combines Host and path to create URI.
	 */
	std::string getURI(bool secure=true) const;


	///Send response
	/**
	 * Allows to send simple response to the client
	 *
	 * @param contentType type of the content (i.e.: "text/plain")
	 * @param body body of the response
	 *
	 */
	void sendResponse(StrViewA contentType, BinaryView body);
	void sendResponse(StrViewA contentType, StrViewA body);
	///Send response
	/**
	 * Allows to send simple response to the client
	 *
	 * @param contentType type of the content (i.e.: "text/plain")
	 * @param body body of the response
	 * @param statusCode sends with specified status code. (i.e. 404)
	 *
	 */
	void sendResponse(StrViewA contentType, BinaryView body, int statusCode);
	void sendResponse(StrViewA contentType, StrViewA body, int statusCode);
	///Send response
	/**
	 * Allows to send simple response to the client
	 *
	 * @param contentType type of the content (i.e.: "text/plain")
	 * @param body body of the response
	 * @param statusCode sends with specified status code. (i.e. 404)
	 * @param statusMessage sends with specified status message. (i.e. "Not found")
	 *
	 */
	void sendResponse(StrViewA contentType, BinaryView body, int statusCode, StrViewA statusMessage);
	void sendResponse(StrViewA contentType, StrViewA body, int statusCode, StrViewA statusMessage);
	///Generates error page
	/**
	 * @param statusCode sends with specified status code. (i.e. 404)
	 *
	 */
	void sendErrorPage(int statusCode);
	///Generates error page
	/**
	 * @param statusCode sends with specified status code. (i.e. 404)
	 * @param statusMessage sends with specified status message. (i.e. "Not found")
	 */
	void sendErrorPage(int statusCode, StrViewA statusMessage, StrViewA desc = StrViewA());


	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType);
	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @param statusCode allows to set status code
	 * @param statusMessage allows to set status message
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType, int statusCode);
	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @param statusCode allows to set status code
	 * @param statusMessage allows to set status message
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType, int statusCode, StrViewA statusMessage);


	///Generates response using HTTPResponse object
	/**
	 * @param resp object contains headers
	 * @param content content
	 */
	void sendResponse(const HTTPResponse &resp, StrViewA body);
	///Generates response using HTTPResponse object
	/**
	 * @param resp object contains headers
	 * @return stream
	 */
	Stream sendResponse(const HTTPResponse &resp);


	void redirect(StrViewA url);


protected:

	std::vector<char> requestHdrLineBuffer;
	HdrMap hdrMap;

	StrViewA reqLine;
	StrViewA method;
	StrViewA path;
	StrViewA version;

	Stream originStream;
	Stream reqStream;
	bool responseSent;

	///in case that keepalive is enabled, contains handler to call for second and othe requests
	HTTPHandler keepAliveHandler;

	///accepts line while it parses headers
	/**
	 * @param stream source stream (need to allow putBack)
	 * @param data data read from the stream
	 * @retval true line accepted, but more data are need
	 * @retval false line accepted, this was last line
	 */
	bool acceptLine(Stream stream, BinaryView data);
	///Prepares request - parses headers and convert the stream
	/**
	 * @param stream source stream
	 * @return converted stream
	 */
	Stream prepareRequest(const Stream &stream);



	bool parseHeaders(StrViewA hdr);
	void runHandler(const Stream& stream, const HTTPHandler& handler);
	void parseReqLine(StrViewA line);
	Stream prepareStream(const Stream &stream);


	void sendResponseLine(int statusCode, StrViewA statusMessage);
	Stream sendHeaders(const HTTPResponse *resp, const StrViewA *contentType,const size_t *contentLength);

	class KeepAliveFn;

	void handleKeepAlive();
};



class HTTPRequest: public PHTTPRequestData {
public:

	using PHTTPRequestData::RefCntPtr;


	///Parses stream for http request and calls handler with informations about this request
	/** Function will run asynchronously if there is AsyncProvider assigned to the stream
	 *
	 * @param stream source stream
	 * @param handler function called when request arrives
	 * @param keepAlive if argument is true, then handler can be called multiple
	 * times for keep-alive connections. Settings this argument false causes that
	 * handler will be called just once. Note that argument is supported only
	 * for connections with asynchronous support. Otherwise handler is called once
	 * and then funcion exits.
	 *
	 *
	 *
	 * */
	static void parseHttp(Stream stream, HTTPHandler handler,  bool keepAlive=true);




};









}
