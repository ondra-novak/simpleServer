#include "http_parser.h"

namespace simpleServer {








void parseHttp(Stream stream, HTTPHandler handler) {


	RefCntPtr<HTTPRequestData> req(new HTTPRequestData);

	req->parseHttp(stream, handler);

}

void HTTPRequestData::parseHttp(Stream stream, HTTPHandler handler) {

	RefCntPtr<HTTPRequestData> me(this);
	if (stream.canRunAsync()) {
		//async
		stream.readASync([me,handler,stream](AsyncState st, const BinaryView &data) {
			if (st == asyncOK) {
				if (me->acceptLine(stream, data))
					me->parseHttp(stream, handler);
				else {
					Stream stream2 = me->prepareRequest(stream);
					handler(stream2, HTTPRequest(me));
				}
			}
		});
	} else {
		BinaryView b = stream.read();
		while (!b.empty() && me->acceptLine(stream,b)) {
			b = stream.read();
		}
		Stream stream2 = me->prepareRequest(stream);
		handler(stream2, HTTPRequest(me));
	}

}

bool HTTPRequestData::acceptLine(Stream stream, BinaryView data) {
	if (!requestHdrLine.empty()
			&& requestHdrLine[requestHdrLine.size()-1] == '\r'
			&& data[0] == '\n') {
		requestHdrLine.push_back(data[0]);
		data = data.substr(1);
	} else {
		auto pos = data.indexOf(BinaryView(StrViewA("\r\n")));
		if (pos != data.npos) {
			BinaryView part = data.substr(0,pos+2);
			requestHdrLine.insert(requestHdrLine.end(),part.begin(),part.end());
			data = data.substr(pos+2);
		} else {
			requestHdrLine.insert(requestHdrLine.end(),data.begin(),data.end());
			data = BinaryView();
		}
	}

	bool r = parseHeaderLine(requestHdrLine);
	requestHdrLine.clear();
	if (r) {
		if (data.empty()) return true;
		return acceptLine(stream,data);
	} else {
		stream.putBack(data);
	}

}

Stream HTTPRequestData::prepareRequest(const Stream& stream) {
	return stream;
}



}


