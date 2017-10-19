#include "http_parser.h"

#include <cstdlib>


#include "chunkedStream.h"
#include "exceptions.h"

#include "limitedStream.h"

namespace simpleServer {








void parseHttp(Stream stream, HTTPHandler handler) {


	RefCntPtr<HTTPRequestData> req(new HTTPRequestData);

	req->parseHttp(stream, handler);

}

static StrViewA trim(StrViewA src) {
	while (!src.empty() && isspace(src[0])) src = src.substr(1);
	while (!src.empty() && isspace(src[src.length-1])) src = src.substr(0,src.length-1);
	return src;
}

bool HTTPRequestData::parseHeaders(StrViewA hdr) {
	hdrMap.clear();
	auto splt = hdr.split("\r\n");
	reqLine = splt();
	parseReqLine(reqLine);
	while (!splt) {
		StrViewA line = splt();
		auto kvsp = line.split(":");
		StrViewA key = trim(splt());
		StrViewA value = trim(StrViewA(splt));
		hdrMap.insert(std::make_pair(key, value));
	}
	return version == "HTTP/1.0" || version == "HTTP/1.1";
}

void HTTPRequestData::runHandler(const Stream& stream, const HTTPHandler& handler) {
	try {
		Stream stream2 = prepareRequest(stream);
		handler(stream2, HTTPRequest(this));
	} catch (const HTTPStatusException &e) {
		(void)e;
		//TODO handle exception
	} catch (const std::exception &e) {
		(void)e;
		//TODO handle exception
	}

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
					me->runHandler(stream,handler);
				}
			}
		});
	} else {
		BinaryView b = stream.read();
		while (!b.empty() && me->acceptLine(stream,b)) {
			b = stream.read();
		}
		runHandler(stream, handler);
	}

}

bool HTTPRequestData::acceptLine(Stream stream, BinaryView data) {
	std::size_t searchPos = requestHdrLineBuffer.size();
	if (searchPos>=3) {
		searchPos-=3;
	} else {
		searchPos = 0;
	}

	requestHdrLineBuffer.insert(requestHdrLineBuffer.end(),data.begin(),data.end());
	StrViewA hdrline(requestHdrLineBuffer.data(), requestHdrLineBuffer.size());
	auto pos = hdrline.indexOf("\r\n\r\n",searchPos);
	if (pos != hdrline.npos) {
		BinaryView remain(hdrline.substr(pos+4));
		auto ofs = data.indexOf(remain,0);
		stream.putBack(data.substr(ofs));
	} else {
		return true;
	}
}

Stream HTTPRequestData::prepareRequest(const Stream& stream) {
	do {
		StrViewA hdr(requestHdrLineBuffer.data(), requestHdrLineBuffer.size());
		auto p = hdr.indexOf("\r\n\t");
		if (p == hdr.npos) {
			parseHeaders(hdr);
			return  prepareStream(stream);
		} else {
			auto beg = requestHdrLineBuffer.begin() + p;
			auto end = beg + 3;
			requestHdrLineBuffer.erase(beg,end);
		}
	} while (true);
	return stream;
}


std::size_t simpleServer::HTTPRequestData::Hash::operator ()(
		StrViewA text) const {
	std::_Hash_impl::hash(text.data, text.length);
}

void HTTPRequestData::parseReqLine(StrViewA line) {
	auto splt = line.split(" ");
	method = splt();
	path = splt();
	version = splt();
}

Stream HTTPRequestData::prepareStream(const Stream& stream) {
	HeaderValue cc = operator[]("Connection");
	if (cc == "keep-alive") {
		keepAlive = true;
	}else if (cc == "close") {
		keepAlive = false;
	} else {
		keepAlive = version == "HTTP/1.1";
	}

	HeaderValue te = operator[]("Transfer-Encoding");
	if (te == "chunked") {
		return ChunkedStream<>::create(stream);
	} else {
		HeaderValue cl = operator[]("Content-Length");
		bool hasLength = false;
		long length;
		if (cl.defined()) {
			if (isdigit(cl[0])) {
				length = std::strtol(te.data,0,10);
			}
		}
		return LimitedStream::create(stream, length,0);
	}
}

}

