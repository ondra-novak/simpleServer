#include "webSocketsHandler.h"

#include "http_parser.h"
#include "base64.h"
#include "sha1.h"
#include "websockets_stream.h"

namespace simpleServer {




WebSocketHandler::WebSocketHandler(const WebSocketObserver& hndl, const HTTPHandler& fallBack)
	:hndl(hndl),fallback(fallBack)
{
}

void WebSocketHandler::operator ()(const HTTPRequest&r) {
	processRequest(r);

}

void WebSocketHandler::operator ()(const HTTPRequest&r, const StrViewA&) {
	processRequest(r);
}


void WebSocketHandler::processRequest(const HTTPRequest &r) {

	HeaderValue upgrade(r["upgrade"]),
				connection(r["connection"]),
				secKey(r["Sec-WebSocket-Key"]);


	if (upgrade == "websocket" && connection == "upgrade" && secKey.defined()) {


		HTTPResponse resp(101,"Switching Protocols");
		resp("upgrade", upgrade)
			("connection", connection);

		Sha1 sha;
		sha.update(BinaryView(secKey));
		sha.update(BinaryView(StrViewA("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
		BinaryView shadigest = sha.final();

		std::string secaccept = base64encode(shadigest);
		resp("Sec-WebSocket-Accept", secaccept);
		if (!protocolName.empty()) resp("Sec-WebSocket-Protocol", protocolName);

		Stream sx = r.sendResponse(resp);

		WebSocketStream stream (new _details::WebSocketStreamImpl(sx));

		hndl(stream);
		if (sx.canRunAsync()) {
			runAsyncCycle(hndl,stream);
		} else {
			while (stream.read()) {
				if (stream.isComplete()) {
					hndl(stream);
				}
			}
		}



	} else {
		if (fallback!=nullptr)
			fallback(r);
		else
			r.sendErrorPage(403);

	}


}

void WebSocketHandler::runAsyncCycle(const WebSocketObserver &hndl, const WebSocketStream& wsstream) {
	WebSocketStream ws(wsstream);
	WebSocketObserver h(hndl);
	ws.readAsync([=](AsyncState st){
		if (st == asyncOK) {
			h(ws);
			runAsyncCycle(h,ws);
		}
	});
}

}
