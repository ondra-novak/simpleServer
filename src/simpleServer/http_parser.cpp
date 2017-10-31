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
class ChunkedStreamWrap: public ChunkedStream<16384> {
public:

	ChunkedStreamWrap(const Fn &fn, const Stream &source):ChunkedStream(source),fn(fn) {}
	~ChunkedStreamWrap() noexcept {}

	void onRelease() {
		try {
			writeEof();
			implFlush();
			ChunkedStream<16384>::onRelease();
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
	~LimitedStreamWrap() noexcept {}

	void onRelease() {
		try {
			writeEof();
			LimitedStream::onRelease();
			fn();
		} catch (...) {}
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


bool HTTPRequest::parseHttp(Stream stream, HTTPHandler handler, bool keepAlive) {


	RefCntPtr<HTTPRequestData> req(new HTTPRequestData);

	return req->parseHttp(stream, handler, keepAlive);

}


void HTTPRequestData::runHandler(const Stream& stream, const HTTPHandler& handler) {

	if (versionStr == "HTTP/0.9") {
		version = http09;
	} else if (versionStr == "HTTP/1.0") {
		version = http10;
	} else if (versionStr == "HTTP/1.1") {
		version = http11;
	} else {
		sendErrorPage(505);
		return;
	}
	keepAlive = version == http11;

	try {
		reqStream= prepareStream(stream);
		handler(HTTPRequest(this));
	} catch (const HTTPStatusException &e) {
		if (!responseSent) {
			sendErrorPage(e.getStatusCode(), e.getStatusMessage());
		}
	} catch (const std::exception &e) {
		sendErrorPage(500, StrViewA("InternalError"), e.what());
	}

}


bool HTTPRequestData::parseHttp(Stream stream, HTTPHandler handler, bool keepAlive) {

	originStream=stream;
	responseSent=false;
	hdrs.clear();
	keepAliveHandler = handler;

	if (stream.canRunAsync()) {

		RefCntPtr<HTTPRequestData> me(this);
		hdrs.parseAsync(stream,[=](AsyncState st){

			if (st == asyncOK) {
				me->parseReqLine(me->hdrs.getFirstLine());
				me->runHandler(stream, handler);
			}

		});
		return false;

	} else {
		if (hdrs.parse(stream)) {
			parseReqLine(hdrs.getFirstLine());
			runHandler(stream, handler);
			return keepAlive;
		} else {
			return false;
		}
	}

}



void HTTPRequestData::parseReqLine(StrViewA line) {
	auto splt = line.split(" ");
	method = splt();
	path = splt();
	versionStr = splt();
}

HTTPRequestData::HdrMap::const_iterator HTTPRequestData::begin() const {
	return hdrs.begin();
}

HTTPRequestData::HdrMap::const_iterator HTTPRequestData::end() const {
	return hdrs.end();
}

HeaderValue HTTPRequestData::operator [](StrViewA key) const {
	return  hdrs[key];
}

StrViewA HTTPRequestData::getMethod() const {
	return method;
}

StrViewA HTTPRequestData::getPath() const {
	return path;
}

HttpVersion HTTPRequestData::getVersion() const {
	return version;
}

StrViewA HTTPRequestData::getRequestLine() const {
	return hdrs.getFirstLine();
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

	s.flush();
	handleKeepAlive();

}

void HTTPRequestData::sendErrorPage(int statusCode) {
	sendErrorPage(statusCode, getStatuCodeMsg(statusCode), StrViewA());
}

void HTTPRequestData::sendErrorPage(int statusCode, StrViewA statusMessage, StrViewA desc) {
	if (statusCode == 204) {
		sendResponseLine(204,getStatuCodeMsg(204));
		originStream << CRLF;
	} else {

	std::ostringstream body;
	body << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"
			"<html xmlns=\"http://www.w3.org/1999/xhtml\">"
			"<head>"
			"<title>" << statusCode << " " << statusMessage <<"</title>"
			"</head>"
			"<body>"
			"<h1>"  << statusCode << " " << statusMessage <<"</h1>"
			"<p><![CDATA[" << desc << "]]></p>"
			"<hr />"
			"<small><em>Powered by Bredy's simpleServer - C++x11 mini-http-server - <a href=\"https://github.com/ondra-novak/simpleServer\">sources available under MIT licence</a></em></small>"
			"</body>"
			"</html>";

	sendResponse(StrViewA("application/xhtml+xml"), BinaryView(StrViewA(body.str())), statusCode, statusMessage);
	}
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
	s.flush();
	handleKeepAlive();

}

Stream HTTPRequestData::sendResponse(const HTTPResponse& resp) {
	sendResponseLine(resp.getCode(), resp.getStatusMessage());
	return sendHeaders(&resp, nullptr, nullptr);

}

void HTTPRequestData::redirect(StrViewA url) {
}

Stream HTTPRequestData::prepareStream(const Stream& stream) {

	HeaderValue con = operator[](CONNECTION);
	if (con == "keep-alive") keepAlive = true;
	else if (con == "close") keepAlive = false;

	HeaderValue te = operator[](TRANSFER_ENCODING);
	if (te == "chunked") {
		return new ChunkedStream<16>(stream);
	} else {
		HeaderValue cl = operator[](CONTENT_LENGTH);
		bool hasLength = false;
		long length;
		if (cl.defined()) {
			if (isdigit(cl[0])) {
				length = std::strtol(cl.data,0,10);
			}
		}
		return new LimitedStream(stream, length,0,0);
	}
}


HTTPResponse::HTTPResponse(int code):code(code), message(pool.add(getStatuCodeMsg(code)))
{
}

HTTPResponse::HTTPResponse(int code, StrViewA response):code(code),message(pool.add(message)) {
}


void HTTPResponse::clear() {
	SendHeaders::clear();
	message = Pool::String();
}

int HTTPResponse::getCode() const {
	return code;
}

HTTPResponse::HTTPResponse(const HTTPResponse& other)
:SendHeaders(other),code(other.code),message(pool.add(other.message.getView())) {
}

StrViewA HTTPResponse::getStatusMessage() const {
	return message.getView();
}


void HTTPRequestData::sendResponseLine(int statusCode, StrViewA statusMessage) {
	if (responseSent) {
		throw std::runtime_error("HTTP server: Response already sent (or started)");
	}
	responseSent = true;
	originStream << versionStr << " " << statusCode << " " << statusMessage
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
		bool hasClose = false;
	};

	Flags flags;
	std::size_t bodyLimit = -1;
	bool usechunked = false;


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
			} else if (key == CONNECTION) {
				if (value == "close") {
					keepAlive = false;
					flags.hasClose = true;
				} else if (value == "keep-alive") {
					keepAlive = true;
					flags.hasClose = true;
				}
			}

			originStream << key << ": " << value << CRLF;
			return true;
		});

	}


	if (!flags.hasClose) {
		if (!flags.hasCtl && version != http11) {
			keepAlive = false;
		}

		if (keepAlive) {
			if (version != http11) {
				originStream << CONNECTION << ": " << "keep-alive" << CRLF;
				flags.hasClose = true;
			}
		} else {
			originStream << CONNECTION << ": " << "close" << CRLF;
		}
	}

	if (!flags.hasCtt) {
		originStream << CONTENT_TYPE << ": text/plain\r\n";
	}
	if (!flags.hasCtl && !flags.hasTE && keepAlive) {
		originStream << TRANSFER_ENCODING << ": chunked\r\n";
		usechunked = true;
	}

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

void HTTPRequestData::readBodyAsync(std::size_t maxSize, HTTPHandler completion) {
	userBuffer.clear();
	readBodyAsync_cont1(maxSize, completion);
}


void HTTPRequestData::readBodyAsync_cont1(std::size_t maxSize, HTTPHandler completion) {
	PHTTPRequestData me(this);
		std::size_t end = userBuffer.size();
		std::size_t remain = maxSize - end;
		if (remain > 4096) remain = 4096;
		if (remain == 0) {

			keepAlive = false;
			sendErrorPage(413);
			return;

		} else {

			userBuffer.resize(end+4096);
			reqStream.readASync(MutableBinaryView(&userBuffer[end],remain),
				[me, maxSize,completion](AsyncState st, const BinaryView data) {

				me->userBuffer.resize(me->userBuffer.size()-4096+data.length);
				if (data.empty()) {
					completion(HTTPRequest(me));
				} else {
					me->readBodyAsync_cont1(maxSize, completion);
				}

			});
		}
}


void HTTPRequestData::handleKeepAlive() {
	reqStream = nullptr;

	if (keepAliveHandler != nullptr && keepAlive) {
		parseHttp(originStream, keepAliveHandler,true);
	}
}


}

