#pragma once

#include <functional>
#include "http_parser.h"

namespace simpleServer {

class WebSocketStream;

typedef std::function<void(const WebSocketStream &wsstream)> WebSocketObserver;



class WebSocketHandler {
public:


	WebSocketHandler(const WebSocketObserver &hndl, const HTTPHandler &fallBack);
	WebSocketHandler(const WebSocketObserver &hndl, const HTTPHandler &fallBack, const std::string &protocolName);

	void operator()(const HTTPRequest &);
	void operator()(const HTTPRequest &, const StrViewA &);

protected:

	WebSocketObserver hndl;
	HTTPHandler fallback;
	std::string protocolName;

	void processRequest(const HTTPRequest &r);

	static void runAsyncCycle(const WebSocketObserver &hndl, const WebSocketStream &wsstream);
};


}
