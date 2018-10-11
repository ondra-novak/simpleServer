#pragma once

#include <functional>
#include <map>
#include <unordered_map>


#include "abstractStream.h"
#include "shared/stringpool.h"
#include "http_headers.h"
#include "http_headervalue.h"
#include "shared/logOutput.h"

#include "shared/refcnt.h"
#include "shared/stringview.h"

namespace simpleServer {

using ondra_shared::StringPool;
using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;
using ondra_shared::StrViewA;
using ondra_shared::BinaryView;
using ondra_shared::LogObject;

class HTTPRequest;
class HTTPRequestData;

typedef RefCntPtr<HTTPRequestData> PHTTPRequestData;



typedef std::function<void(const HTTPRequest &)> HTTPHandler;

enum HttpVersion {
	http09,
	http10,
	http11
};





class HTTPResponse: public SendHeaders{
public:

	explicit HTTPResponse(int code);
	HTTPResponse(int code, StrViewA response);
	HTTPResponse(const HTTPResponse &other);


	HTTPResponse &operator()(const StrViewA key, const StrViewA value) {
		SendHeaders::operator ()(key, value);return *this;
	}
	HTTPResponse &contentLength(std::size_t sz) {
		SendHeaders::contentLength(sz);return *this;
	}
	HTTPResponse &contentType(StrViewA ctx) {
		SendHeaders::contentType(ctx);return *this;
	}

	void clear();

	int getCode() const;
	StrViewA getStatusMessage() const;




protected:

	int code;
	Pool::String message;

};


enum class Redirect {
	///Permanent redirect - browser can choose whether the method will be repeated
	permanent = 301,
	///Temporary redirect - browser can choose whether the method will be repeated
	temporary = 302,
	///Temporary redirect - browser will use the method GET
	temporary_GET = 303,
	///Temporary redirect - browser will repeat the request to new url
	temporary_repeat = 307,
	///Permanent redirect - browser will repeat the request to new url
	permanent_repeat = 308,
};


class HTTPRequestData: public RefCntObj   {


public:

	HTTPRequestData();
	HTTPRequestData(LogObject curLog);


	typedef ReceivedHeaders::HdrMap HdrMap;

	///Begin of the headers
	HdrMap::const_iterator begin() const;
	///End of the headers
	HdrMap::const_iterator end() const;
	///Retrieves header value
	HeaderValue operator[](StrViewA key) const;



	bool parseHttp(Stream stream, HTTPHandler handler, bool keepAlive);


	///Retrieves method
	/**
	 * @return String contains method name: GET, PUT, POST, OPTIONS, etc
	 */
	StrViewA getMethod() const;


	///Defines allowed methods
	/** Function uses getMethod() to retrieve current method. If
	 * the method is not in the list, it generates the error response 405
	 * Method Not Allowed, and returns false
	 *
	 * @param methods list of allowed methods
	 * @retval true pass
	 * @retval false not pass, function generated response 405. Request
	 * is finished
	 */
	bool allowMethods(std::initializer_list<StrViewA> methods);
	///Retrieves path
	/**
	 * @return string contains path from the request line (raw)
	 */
	StrViewA getPath() const;
	///Retrieves http version
	/**
	 * @return string contains HTTP version: HTTP/1.0 or HTTP/1.1
	 */
	HttpVersion getVersion() const;

	StrViewA getHost() const;

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


	void redirect(StrViewA url, Redirect type);



	///A buffer which can be used to any purpose.
	/** The function readBodyAsync uses the buffer to put content of body there
	 *
	 */
	std::vector<unsigned char> userBuffer;



	///Reads body of the request asynchronously.
	/**
	 * The content of the body is put into userBuffer. The buffer is cleared before
	 * the reading starts. Reading is made asynchronously. Once the reading is complete,
	 * the completion callback is called
	 *
	 * @param maxSize allows to limit size of the body. If the body is larger, error status
	 * is generated
	 * @param completion function called when reading is complette.
	 *
	 */
	void readBodyAsync(std::size_t maxSize, HTTPHandler completion);

	///sends file directly from the disk to the client
	/**
	 * Function also supports ETag, so it can skip sending file when the file did not changed
	 *
	 * @param content_type content type of the file
	 * @param pathname whole pathname. Note if the file doesn't exists or cannot be opened, 404 error is generated instead
	 * @param etag true to support ETag. If file is not changed, 304 header can be generated. Note that
	 * etag generation is implementation depended. It could be encoded time of modification, or has generated
	 * from the content of the file.
	 */

	void sendFile(StrViewA content_type, StrViewA pathname, bool etag = true);

	bool redirectToFolderRoot(Redirect type);


	LogObject log;

protected:

	ReceivedHeaders hdrs;


	StrViewA method;
	StrViewA path;
	StrViewA versionStr;
	StrViewA host;
	HttpVersion version;
	bool keepAlive;

	Stream originStream;
	Stream reqStream;
	bool responseSent;

	///in case that keepalive is enabled, contains handler to call for second and othe requests
	HTTPHandler keepAliveHandler;


	void runHandler(const Stream& stream, const HTTPHandler& handler);
	void parseReqLine(StrViewA line);
	Stream prepareStream(const Stream &stream);


	void sendResponseLine(int statusCode, StrViewA statusMessage);
	Stream sendHeaders(int code, const HTTPResponse *resp, const StrViewA *contentType,const size_t *contentLength);


	class KeepAliveFn;

	void handleKeepAlive();

	void readBodyAsync_cont1(std::size_t maxSize, HTTPHandler completion);

	template<typename It>
	bool allowMethodsImpl(const It &beg, const It &end);

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
	static bool parseHttp(Stream stream, HTTPHandler handler,  bool keepAlive=true);



	///Retrieves method
	/**
	 * @return String contains method name: GET, PUT, POST, OPTIONS, etc
	 */
	StrViewA getMethod() const {return ptr->getMethod();}

	bool allowMethods(std::initializer_list<StrViewA> methods) {
		return ptr->allowMethods(methods);
	}

	///Retrieves path
	/**
	 * @return string contains path from the request line (raw)
	 */
	StrViewA getPath() const {return ptr->getPath();}

	StrViewA getHost() const {return ptr->getHost();}
///Retrieves http version
	/**
	 * @return string contains HTTP version: HTTP/1.0 or HTTP/1.1
	 */
	HttpVersion getVersion() const {return ptr->getVersion();}
	///Retrieves whole request line
	StrViewA getRequestLine() const {return ptr->getRequestLine();}
	///Forges full uri
	/**
	 * @param secure set true if the connection is secured, otherwise false
	 * @return full URI
	 *
	 * @note function just combines Host and path to create URI.
	 */
	std::string getURI(bool secure=true) const {return ptr->getURI(secure);}


	///Send response
	/**
	 * Allows to send simple response to the client
	 *
	 * @param contentType type of the content (i.e.: "text/plain")
	 * @param body body of the response
	 *
	 */
	void sendResponse(StrViewA contentType, BinaryView body)const  {ptr->sendResponse(contentType,body);}
	void sendResponse(StrViewA contentType, StrViewA body)const  {ptr->sendResponse(contentType,body);}
	///Send response
	/**
	 * Allows to send simple response to the client
	 *
	 * @param contentType type of the content (i.e.: "text/plain")
	 * @param body body of the response
	 * @param statusCode sends with specified status code. (i.e. 404)
	 *
	 */
	void sendResponse(StrViewA contentType, BinaryView body, int statusCode)const  {ptr->sendResponse(contentType,body,statusCode);}
	void sendResponse(StrViewA contentType, StrViewA body, int statusCode) const  {ptr->sendResponse(contentType,body,statusCode);}
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
	void sendResponse(StrViewA contentType, BinaryView body, int statusCode, StrViewA statusMessage) const
	 	 {ptr->sendResponse(contentType,body,statusCode,statusMessage);}
	void sendResponse(StrViewA contentType, StrViewA body, int statusCode, StrViewA statusMessage) const
		{ptr->sendResponse(contentType,body,statusCode,statusMessage);}
	///Generates error page
	/**
	 * @param statusCode sends with specified status code. (i.e. 404)
	 *
	 */
	void sendErrorPage(int statusCode) const {ptr->sendErrorPage(statusCode);}
	///Generates error page
	/**
	 * @param statusCode sends with specified status code. (i.e. 404)
	 * @param statusMessage sends with specified status message. (i.e. "Not found")
	 */
	void sendErrorPage(int statusCode, StrViewA statusMessage, StrViewA desc = StrViewA()) const {ptr->sendErrorPage(statusCode,statusMessage,desc);}


	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType) const {return ptr->sendResponse(contentType);}
	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @param statusCode allows to set status code
	 * @param statusMessage allows to set status message
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType, int statusCode) const {return ptr->sendResponse(contentType, statusCode);}
	///Generates response, returns stream
	/**
	 * Allows to stream response
	 *
	 * @param contentType contenr type of the stream
	 * @param statusCode allows to set status code
	 * @param statusMessage allows to set status message
	 * @return stream. You can start sending data through the stream
	 */
	Stream sendResponse(StrViewA contentType, int statusCode, StrViewA statusMessage) const {return ptr->sendResponse(contentType, statusCode, statusMessage);}


	///Generates response using HTTPResponse object
	/**
	 * @param resp object contains headers
	 * @param content content
	 */
	void sendResponse(const HTTPResponse &resp, StrViewA body)const {ptr->sendResponse(resp,body);}
	///Generates response using HTTPResponse object
	/**
	 * @param resp object contains headers
	 * @return stream
	 */
	Stream sendResponse(const HTTPResponse &resp) const {return ptr->sendResponse(resp);}


	void redirect(StrViewA url, Redirect type = Redirect::temporary) const {return ptr->redirect(url,type);}

	///Redirects the browser
	bool redirectToFolderRoot(Redirect type = Redirect::permanent_repeat) const {return ptr->redirectToFolderRoot(type);}



	std::vector<unsigned char> &getUserBuffer() const {return ptr->userBuffer;}

	///Reads body of the request asynchronously.
	/**
	 * The content of the body is put into userBuffer. The buffer is cleared before
	 * the reading starts. Reading is made asynchronously. Once the reading is complete,
	 * the completion callback is called
	 *
	 * @param maxSize allows to limit size of the body. If the body is larger, error status
	 * is generated
	 * @param completion function called when reading is complette.
	 *
	 */
	void readBodyAsync(std::size_t maxSize, HTTPHandler completion) const {ptr->readBodyAsync(maxSize, completion);}

	///sends file directly from the disk to the client
	/**
	 * Function also supports ETag, so it can skip sending file when the file did not changed
	 *
	 * @param pathname whole pathname. Note if the file doesn't exists or cannot be opened, 404 error is generated instead
	 * @param content_type content type of the file. If it is empty, function select content type depend on file extension
	 * @param etag true to support ETag. If file is not changed, 304 header can be generated. Note that
	 * etag generation is implementation depended. It could be encoded time of modification, or has generated
	 * from the content of the file.
	 */

	void sendFile(StrViewA pathname, StrViewA content_type = StrViewA(), bool etag = true)const {ptr->sendFile(content_type, pathname, etag);}

	///Begin of the headers
	ReceivedHeaders::HdrMap::const_iterator begin() const {
		return ptr->begin();
	}
	///End of the headers
	ReceivedHeaders::HdrMap::const_iterator end() const {
		return ptr->end();
	}
	///Retrieves header value
	HeaderValue operator[](StrViewA key) const {
		return ptr->operator [](key);
	}




};









}
