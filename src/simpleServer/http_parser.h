#pragma once

#include <functional>

#include "abstractStream.h"

#include "refcnt.h"

namespace simpleServer {

class HTTPRequest;
class HTTPRequestData;
typedef RefCntPtr<HTTPRequestData> PHTTPRequestData;


typedef std::function<void(Stream, HTTPRequest)> HTTPHandler;



class HTTPRequestData: public RefCntObj {
public:



	void parseHttp(Stream stream, HTTPHandler handler);


protected:


	std::vector<char> requestHdrLine;

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


	bool parseHeaderLine(StrViewA hdrLine);

};



class HTTPRequest: public PHTTPRequestData {
public:

	using PHTTPRequestData::RefCntPtr;





};





///Parses stream for http request and calls handler with informations about this request
/** Function will run asynchronously if there is AsyncProvider assigned to the stream */
void parseHttp(Stream stream, HTTPHandler handler);




}
