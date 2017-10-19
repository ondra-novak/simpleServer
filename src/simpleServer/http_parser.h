#pragma once

#include <functional>
#include <unordered_map>


#include "abstractStream.h"
#include "http_headervalue.h"

#include "refcnt.h"
#include "stringview.h"

namespace simpleServer {

class HTTPRequest;
class HTTPRequestData;
typedef RefCntPtr<HTTPRequestData> PHTTPRequestData;


typedef std::function<void(Stream, HTTPRequest)> HTTPHandler;



class HTTPRequestData: public RefCntObj {
	struct Hash {
		std::size_t operator()(StrViewA text) const;
	};
public:

	typedef std::unordered_map<StrViewA, HeaderValue, Hash> HdrMap;

	///Begin of the headers
	HdrMap::const_iterator begin() const;
	///End of the headers
	HdrMap::const_iterator end() const;



	void parseHttp(Stream stream, HTTPHandler handler);


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

protected:

	std::vector<char> requestHdrLineBuffer;
	HdrMap hdrMap;

	StrViewA reqLine;
	StrViewA method;
	StrViewA path;
	StrViewA version;
	bool keepAlive;


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
};



class HTTPRequest: public PHTTPRequestData {
public:

	using PHTTPRequestData::RefCntPtr;





};





///Parses stream for http request and calls handler with informations about this request
/** Function will run asynchronously if there is AsyncProvider assigned to the stream */
void parseHttp(Stream stream, HTTPHandler handler);




}
