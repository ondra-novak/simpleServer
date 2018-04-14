#include "webSocketsHandler.h"

#include "http_parser.h"
#include "base64.h"
#include "sha1.h"
#include "websockets_stream.h"

namespace simpleServer {




WebSocketHandler::WebSocketHandler(const WebSocketObserver& hndl)
	:hndl(hndl)
{
}

void WebSocketHandler::operator ()(const HTTPRequest&r) {
	if (!processRequest(r)) {
		r.sendErrorPage(400);
	}

}

bool WebSocketHandler::operator ()(const HTTPRequest&r, const StrViewA&p) {
	return processRequest(r);
}


bool WebSocketHandler::processRequest(const HTTPRequest &r) {

	HeaderValue upgrade(r["Upgrade"]),
				connection(r["Connection"]),
				secKey(r["Sec-WebSocket-Key"]);


	if (upgrade == "websocket" && connection == "Upgrade" && secKey.defined()) {


		HTTPResponse resp(101,"Switching Protocols");
		resp("Upgrade", upgrade)
			("Connection", connection);

		Sha1 sha;
		sha.update(BinaryView(secKey));
		sha.update(BinaryView(StrViewA("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
		BinaryView shadigest = sha.final();

		std::string secaccept = base64encode(shadigest);
		resp("Sec-WebSocket-Accept", secaccept);
		if (!protocolName.empty()) resp("Sec-WebSocket-Protocol", protocolName);

		Stream sx = r.sendResponse(resp);
		sx.flush();

		WebSocketStream stream (new _details::WebSocketStreamImpl(sx));

		hndl(r, stream);
		if (sx.canRunAsync()) {
			runAsyncCycle(r,hndl,stream);
		} else {
			while (stream.readFrame()) {
				hndl(r, stream);
			}
		}

		return true;

	} else {
		return false;

	}


}

void WebSocketHandler::runAsyncCycle(HTTPRequest r, const WebSocketObserver &hndl, const WebSocketStream& wsstream) {
	WebSocketStream ws(wsstream);
	WebSocketObserver h(hndl);
	ws.readAsync([=](AsyncState st)  {
		if (st == asyncOK) {
			h(r,ws);
			runAsyncCycle(r,h,ws);
		} else if (st == asyncTimeout) {
			WebSocketStream ws2(ws);
			ws2.ping(BinaryView());
			ws2.readAsync([=](AsyncState st){
				if (st == asyncOK) {
					h(r,ws);
					runAsyncCycle(r,h,ws);
				}
			});
		}
	});
}

}
