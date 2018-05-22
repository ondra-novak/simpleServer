#pragma once

#include <functional>
#include "http_parser.h"

namespace simpleServer {

class WebSocketStream;

typedef std::function<void(HTTPRequest orgRequest, WebSocketStream wsstream)> WebSocketObserver;



class WebSocketHandler {
public:


	WebSocketHandler(const WebSocketObserver &hndl);
	WebSocketHandler(const WebSocketObserver &hndl, const std::string &protocolName);

	void operator()(const HTTPRequest &);
	bool operator()(const HTTPRequest &, const StrViewA &);

protected:

	WebSocketObserver hndl;
	std::string protocolName;

	bool processRequest(const HTTPRequest &r);

	virtual WebSocketStream createStream(Stream sx) const;

	static void runAsyncCycle(HTTPRequest r, const WebSocketObserver &hndl, const WebSocketStream &wsstream);
};


}
