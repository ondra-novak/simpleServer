#include <sstream>
#include "http_parser.h"

#include <cstdlib>
#include <iosfwd>



#include "chunkedStream.h"
#include "exceptions.h"

#include "limitedStream.h"

namespace simpleServer {



static StrViewA statusMessages[] = {
		"100 Continue",
		"101 Switching Protocols",
		"200 OK",
		"201 Created",
		"202 Accepted",
		"203 Non-Authoritative Information",
		"204 No Content",
		"205 Reset Content",
		"206 Partial Content",
		"300 Multiple Choices",
		"301 Moved Permanently",
		"302 Found",
		"303 See Other",
		"304 Not Modified",
		"305 Use Proxy",
		"307 Temporary Redirect",
		"400 Bad Request",
		"401 Unauthorized",
		"402 Payment Required",
		"403 Forbidden",
		"404 Not Found",
		"405 Method Not Allowed",
		"406 Not Acceptable",
		"407 Proxy Authentication Required",
		"408 Request Timeout",
		"409 Conflict",
		"410 Gone",
		"411 Length Required",
		"412 Precondition Failed",
		"413 Request Entity Too Large",
		"414 Request-URI Too Long",
		"415 Unsupported Media Type",
		"416 Requested Range Not Satisfiable",
		"417 Expectation Failed",
		"426 Upgrade Required",
		"500 Internal Server Error",
		"501 Not Implemented",
		"502 Bad Gateway",
		"503 Service Unavailable",
		"504 Gateway Timeout",
		"505 HTTP Version Not Supported"
};


static const char* CONTENT_TYPE = "Content-Type";
static const char* CONTENT_LENGTH = "Content-Length";
static const char* TRANSFER_ENCODING = "Transfer-Encoding";
static const char* CONNECTION = "Connection";
static const char* CRLF = "\r\n";


/*
static const char *DAY_NAMES[] =
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *MONTH_NAMES[] =
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };




std::string Rfc1123_DateTimeNow()
{
    const int RFC1123_TIME_LEN = 29;
    time_t t;
    struct tm tm;
    char buf[RFC1123_TIME_LEN+1];

    time(&t);
    gmtime_s(&tm, &t);

    strftime(buf, RFC1123_TIME_LEN+1, "---, %d --- %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf+8, MONTH_NAMES[tm.tm_mon], 3);

    return buf;
}
*/


template<typename Fn>
class ChunkedStreamWrap: public ChunkedStream<> {
public:

	ChunkedStreamWrap(const Fn &fn, const Stream &source):ChunkedStream(source),fn(fn) {}
	~ChunkedStreamWrap() {
		try {
			writeEof();
			source.flush(writeAndFlush);
			fn();
		} catch (...) {}
	}
public:
	Fn fn;
};

template<typename Fn>
class LimitedStreamWrap: public LimitedStream {
public:

	LimitedStreamWrap(const Fn &fn, const Stream &source,std::size_t writeLimit):LimitedStream(source,0,writeLimit,0),fn(fn) {}
	~LimitedStreamWrap() {
		writeEof();
		source.flush(writeAndFlush);
		fn();
	}
public:
	Fn fn;
};

static StrViewA getStatuCodeMsg(int code) {
	char num[100];
	sprintf(num,"%d",code);
	StrViewA codestr(num);
	if (codestr.length == 3) {

		auto f = std::lower_bound(
				std::begin(statusMessages),
				std::end(statusMessages),
				codestr, [](StrViewA a, StrViewA b){
			return a.substr(0,3) < b.substr(0.3);
		});
		if (f != std::end(statusMessages)) {
			if (f->substr(0,3) == codestr)
				return f->substr(4);
		}

	}
	return "Unexpected status";
}


void HTTPRequest::parseHttp(Stream stream, HTTPHandler handler, bool keepAlive) {


	RefCntPtr<HTTPRequestData> req(new HTTPRequestData);

	req->parseHttp(stream, handler, keepAlive);

}

static StrViewA trim(StrViewA src) {
	while (!src.empty() && isspace(src[0])) src = src.substr(1);
	while (!src.empty() && isspace(src[src.length-1])) src = src.substr(0,src.length-1);
	return src;
}

bool HTTPRequestData::parseHeaders(StrViewA hdr) {
	hdrMap.clear();
	auto splt = hdr.split(CRLF);
	reqLine = splt();
	parseReqLine(reqLine);
	while (splt) {
		StrViewA line = splt();
		auto kvsp = line.split(":");
		StrViewA key = trim(kvsp());
		StrViewA value = trim(StrViewA(kvsp));
		hdrMap.insert(std::make_pair(key, value));
	}
	return version == "HTTP/1.0" || version == "HTTP/1.1";
}

void HTTPRequestData::runHandler(const Stream& stream, const HTTPHandler& handler) {
	try {
		reqStream= prepareRequest(stream);
		handler(HTTPRequest(this));
	} catch (const HTTPStatusException &e) {
		if (!responseSent) {
			sendErrorPage(e.getStatusCode(), e.getStatusMessage());
		}
	} catch (const std::exception &) {
		sendErrorPage(500);
	}

}

void HTTPRequestData::parseHttpAsync(Stream stream, HTTPHandler handler) {
	//async
	RefCntPtr<HTTPRequestData> me(this);
	stream.readASync(
			[me, handler, stream](AsyncState st, const BinaryView& data) {
				if (st == asyncOK) {
					if (me->acceptLine(stream, data))
					me->parseHttpAsync(stream, handler);
					else {
						me->runHandler(stream, handler);
					}
				}
			});
}

void HTTPRequestData::parseHttp(Stream stream, HTTPHandler handler, bool keepAlive) {

	originStream=stream;
	responseSent=false;
	hdrMap.clear();
	requestHdrLineBuffer.clear();
	keepAliveHandler = handler;

	if (stream.canRunAsync()) {
		//async
		parseHttpAsync(stream, handler);
	} else {
		BinaryView b = stream.read();
		while (!b.empty() && acceptLine(stream,b)) {
			b = stream.read();
		}
		if (!b.empty())
			runHandler(stream, handler);
	}

}

bool HTTPRequestData::acceptLine(Stream stream, BinaryView data) {

	while (!data.empty() && requestHdrLineBuffer.empty() && isspace(data[0])) {
		data = data.substr(1);
	}

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
		return false;
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


std::size_t HTTPRequestData::Hash::operator ()(
		StrViewA text) const {
	std::_Hash_impl::hash(text.data, text.length);
}

void HTTPRequestData::parseReqLine(StrViewA line) {
	auto splt = line.split(" ");
	method = splt();
	path = splt();
	version = splt();
}

HTTPRequestData::HdrMap::const_iterator HTTPRequestData::begin() const {
	return hdrMap.begin();
}

HTTPRequestData::HdrMap::const_iterator HTTPRequestData::end() const {
	return hdrMap.end();
}

HeaderValue HTTPRequestData::operator [](StrViewA key) const {
	auto x = hdrMap.find(key);
	if (x == hdrMap.end()) {
		return HeaderValue();
	} else {
		return HeaderValue(x->second);
	}

}

StrViewA HTTPRequestData::getMethod() const {
	return method;
}

StrViewA HTTPRequestData::getPath() const {
	return path;
}

StrViewA HTTPRequestData::getVersion() const {
	return version;
}

StrViewA HTTPRequestData::getRequestLine() const {
	return reqLine;
}

std::string HTTPRequestData::getURI(bool secure) const {
	std::string r(secure?"https://":"http://");
	HeaderValue host = operator[]("Host");
	r.append(host.data, host.length);
	r.append(path.data, path.length);
	return r;
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body) {
	sendResponse(contentType, body, 200,"OK");
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body,
		int statusCode) {
	sendResponse(contentType, body, statusCode, getStatuCodeMsg(statusCode));
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body,
		int statusCode, StrViewA statusMessage) {



	sendResponseLine(statusCode, statusMessage);
	Stream s = sendHeaders(nullptr, &contentType, &body.length);

	s.write(BinaryView(body));

}

void HTTPRequestData::sendErrorPage(int statusCode) {
	sendResponse(statusCode, getStatuCodeMsg(statusCode));
}

void HTTPRequestData::sendErrorPage(int statusCode, StrViewA statusMessage, StrViewA desc) {
	std::ostringstream body;
	body << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
			"<!DOCTYPE html>"
			"<html>"
			"<head>"
			"<title>" << statusCode << " " << statusMessage <<"</title>"
			"</head>"
			"<body>"
			"<h1>"  << statusCode << " " << statusMessage <<"</h1>"
			"<p><![CDATA[" << desc << "]]></p>"
			"</body>"
			"</html>";

	sendResponse(StrViewA("application/xhtml+xml;charset=UTF-8"), BinaryView(StrViewA(body.str())), statusCode, statusMessage);
}

Stream HTTPRequestData::sendResponse(StrViewA contentType) {
	return sendResponse(contentType, 200,"OK");
}

Stream HTTPRequestData::sendResponse(StrViewA contentType, int statusCode) {
	return sendResponse(contentType, statusCode,getStatuCodeMsg(statusCode));
}

Stream HTTPRequestData::sendResponse(StrViewA contentType, int statusCode,
		StrViewA statusMessage) {

	sendResponseLine(statusCode, statusMessage);
	return sendHeaders(nullptr, &contentType, nullptr);


}

void HTTPRequestData::sendResponse(const HTTPResponse& resp, StrViewA body) {
	sendResponseLine(resp.getCode(), resp.getStatusMessage());
	Stream s = sendHeaders(&resp, nullptr, &body.length);
	s.write(BinaryView(body));

}

Stream HTTPRequestData::sendResponse(const HTTPResponse& resp) {
	sendResponseLine(resp.getCode(), resp.getStatusMessage());
	return sendHeaders(&resp, nullptr, nullptr);

}

void HTTPRequestData::redirect(StrViewA url) {
}

Stream HTTPRequestData::prepareStream(const Stream& stream) {

	HeaderValue te = operator[](TRANSFER_ENCODING);
	if (te == "chunked") {
		return ChunkedStream<>::create(stream);
	} else {
		HeaderValue cl = operator[](CONTENT_LENGTH);
		bool hasLength = false;
		long length;
		if (cl.defined()) {
			if (isdigit(cl[0])) {
				length = std::strtol(te.data,0,10);
			}
		}
		return new LimitedStream(stream, length,0,0);
	}
}


HTTPResponse::HTTPResponse(int code)
{
}

HTTPResponse::HTTPResponse(int code, StrViewA response) {
}

HTTPResponse& HTTPResponse::header(const StrViewA key,
		const StrViewA value) {
}

HTTPResponse& HTTPResponse::contentLength(std::size_t sz) {
}

void HTTPResponse::clear() {
}

int HTTPResponse::getCode() const {
}

StrViewA HTTPResponse::getStatusMessage() const {
}


void HTTPRequestData::sendResponseLine(int statusCode, StrViewA statusMessage) {
	if (responseSent) {
		throw std::runtime_error("HTTP server: Response already sent (or started)");
	}
	responseSent = true;
	originStream << version << " " << statusCode << " " << statusMessage
			<< CRLF;

}

class HTTPRequestData::KeepAliveFn {
public:
	KeepAliveFn(RefCntPtr<HTTPRequestData> h):h(h) {}
	void operator()() const {
		h->handleKeepAlive();
	}
protected:
	RefCntPtr<HTTPRequestData> h;

};

Stream HTTPRequestData::sendHeaders(const HTTPResponse* resp,
		const StrViewA* contentType, const size_t* contentLength) {


	struct Flags {
		bool hasCtt = false;
		bool hasCtl = false;
		bool hasTE = false;
//		bool hasDate = false;
		bool closeConn = false;
	};

	Flags flags;
	std::size_t bodyLimit = -1;
	bool usechunked = false;
	bool oldver = version == "HTTP/1.0";


	HeaderValue cc = operator[](CONNECTION);
	if (cc == "keep-alive") {
		flags.closeConn = false;
	}else if (cc == "close") {
		flags.closeConn  = true;
	} else {
		flags.closeConn  = version == "HTTP/1.0";
	}


	if (contentType) {
		originStream << CONTENT_TYPE << ": " << *contentType << CRLF;
		flags.hasCtt = true;
	}
	if (contentLength) {
		originStream << CONTENT_LENGTH << ": " << *contentLength << CRLF;
		flags.hasCtl = true;
	}

	if (resp) {

		resp->forEach([&](StrViewA key, StrViewA value)->bool {

			if (key == CONTENT_TYPE) {
				if (flags.hasCtt) return true;
				flags.hasCtt = true;
			}
			else if (key == CONTENT_LENGTH) {
				if (flags.hasCtl) return true;
				flags.hasCtl = true;
				bodyLimit = std::strtol(value.data,0,10);
			}
			else if (key == TRANSFER_ENCODING) {
				if (flags.hasTE) return true;
				flags.hasTE = true;
/*			} else if (key == "Server") {
				flags.hasServer = true;*/
	/*		} else if (key == "Date") {
				flags.hasDate = true;*/
			} else if (key == CONNECTION) {
				if (value == "close") {
					flags.closeConn = true;
				} else if (value == "keep-alive") {
					flags.closeConn = false;
				}
			}

			originStream << key << ": " << value << CRLF;
			return true;
		});

	}
	if (!flags.hasCtt) {
		originStream << CONTENT_TYPE << ": text/plain\r\n";
	}
	if (!flags.hasCtl && !flags.hasTE && !flags.closeConn) {
		originStream << TRANSFER_ENCODING << ": chunked\r\n";
		usechunked = true;
	}
	if (flags.closeConn) {
		originStream << CONNECTION << ": close\r\n";
	} else if (oldver && flags.hasCtl) {
		originStream << CONNECTION << ": keep-alive\r\n";
	}
/*	if (!flags.hasDate) {
		originStream << "Date" <<": " << Rfc1123_DateTimeNow() << "\r\n";
	}*/

	originStream << CRLF;



	if (usechunked) {
		return new ChunkedStreamWrap<KeepAliveFn>(KeepAliveFn(this),originStream);
	} else if (bodyLimit!=-1) {
		return new LimitedStreamWrap<KeepAliveFn>(KeepAliveFn(this),originStream,bodyLimit);
	} else {
		return originStream;
	}


}

void HTTPRequestData::sendResponse(StrViewA contentType, StrViewA body) {
	sendResponse(contentType,BinaryView(body));
}

void HTTPRequestData::sendResponse(StrViewA contentType, StrViewA body,int statusCode) {
	sendResponse(contentType, BinaryView(body), statusCode);
}

void HTTPRequestData::sendResponse(StrViewA contentType, StrViewA body,int statusCode, StrViewA statusMessage) {
	sendResponse(contentType, BinaryView(body),  statusCode, statusMessage);
}

void HTTPRequestData::handleKeepAlive() {
	reqStream = nullptr;

	if (keepAliveHandler != nullptr) {
		parseHttp(originStream, keepAliveHandler,true);
	}
}


}
