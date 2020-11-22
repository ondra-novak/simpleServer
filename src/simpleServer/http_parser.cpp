#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <cstring>
#include "http_parser.h"

#include <fcntl.h>
#include <string.h>
#include <chrono>

#include <cstdlib>




#include "chunkedStream.h"
#include "exceptions.h"
#include "stringview.h"

#include "limitedStream.h"
#include "shared/logOutput.h"



namespace simpleServer {

using ondra_shared::TaskCounter;
using ondra_shared::AbstractLogProvider;
using ondra_shared::unsignedToString;
using ondra_shared::logDebug;
using ondra_shared::logInfo;


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
		"308 Permanent Redirect",
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
static const char* SET_COOKIE = "Set-Cookie";
static const char* HOST = "Host";
static const char* CRLF = "\r\n";



HTTPRequestData::HTTPRequestData(const PHTTPCounters &cntrs)
	:log(AbstractLogProvider::create(), TaskCounter<HTTPRequestData>("http:")),counters(cntrs) {
	report_beginRequest();
}
HTTPRequestData::HTTPRequestData(const PHTTPCounters &cntrs, LogObject curLog)
	:log(curLog.getProvider()->create(), TaskCounter<HTTPRequestData>("http:")),counters(cntrs)  {
	report_beginRequest();
}

void HTTPRequestData::report_beginRequest() {
	req_start = std::chrono::steady_clock::now();
}

void HTTPRequestData::report_endRequest() {
	if (counters != nullptr) {
		auto end_time = std::chrono::steady_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end_time - req_start).count()/100;
		counters->report(dur);
	}

}


template<typename Fn>
class ChunkedStreamWrap: public ChunkedStream<16384> {
public:

	ChunkedStreamWrap(const Fn &fn, const Stream &source):ChunkedStream(source),fn(fn) {}
	~ChunkedStreamWrap() noexcept {
		try {
			writeEof();
			implFlush();
			fn();
		} catch (...) {

		}

	}

public:
	Fn fn;
};

template<typename Fn>
class LimitedStreamWrap: public LimitedStream {
public:

	LimitedStreamWrap(const Fn &fn, const Stream &source,std::size_t writeLimit)
		:LimitedStream(source,0,writeLimit,0),fn(fn) {
	}
	~LimitedStreamWrap() noexcept {
		try {
			writeEof();
			fn();
		} catch (...) {
		}
	}
public:
	Fn fn;
};

static StrViewA getStatusCodeMsg(int code) {
	char num[100];
	sprintf(num,"%d",code);
	StrViewA codestr(num);
	if (codestr.length == 3) {

		auto f = std::lower_bound(
				std::begin(statusMessages),
				std::end(statusMessages),
				codestr, [](StrViewA a, StrViewA b){
			return a.substr(0,3) < b.substr(0,3);
		});
		if (f != std::end(statusMessages)) {
			if (f->substr(0,3) == codestr)
				return f->substr(4);
		}

	}
	return "Unexpected status";
}


bool HTTPRequest::parseHttp(Stream stream, HTTPHandler handler, bool keepAlive, const PHTTPCounters &counters) {


	RefCntPtr<HTTPRequestData> req(new HTTPRequestData(counters));

	return req->parseHttp(stream, handler, keepAlive);

}


void HTTPRequestData::runHandler(const Stream& stream, const HTTPHandler& handler) {

	report_beginRequest();


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
		if (!responseSent) {
			sendErrorPage(500, StrViewA("InternalError"), e.what());
		}
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
	r.append(host.data, host.length);
	r.append(path.data, path.length);
	return r;
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body) {
	sendResponse(contentType, body, 200,"OK");
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body,
		int statusCode) {
	sendResponse(contentType, body, statusCode, getStatusCodeMsg(statusCode));
}

void HTTPRequestData::sendResponse(StrViewA contentType, BinaryView body,
		int statusCode, StrViewA statusMessage) {



	sendResponseLine(statusCode, statusMessage);
	Stream s = sendHeaders(statusCode,nullptr, &contentType, &body.length);

	s.write(BinaryView(body));

	s.flush();

}

void HTTPRequestData::sendErrorPage(int statusCode) {
	sendErrorPage(statusCode, getStatusCodeMsg(statusCode), StrViewA());
}

void HTTPRequestData::sendErrorPage(int statusCode, StrViewA statusMessage, StrViewA desc) {

	if (statusMessage.empty()) statusMessage = getStatusCodeMsg(statusCode);
	if (statusCode == 204 || statusCode == 304 || statusCode / 100 == 1) {
		sendResponse(HTTPResponse(statusCode,statusMessage).contentLength(0));
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
				"<small><em>Powered by Bredy's simpleServer - C++x17 simpleServer- <a href=\"https://github.com/ondra-novak/simpleServer\">sources available under MIT licence</a></em></small>"
				"</body>"
				"</html>";

		if (statusCode >= 500) keepAlive = false;
		sendResponse(StrViewA("application/xhtml+xml"), BinaryView(StrViewA(body.str())), statusCode, statusMessage);
	}
}

Stream HTTPRequestData::sendResponse(StrViewA contentType) {
	return sendResponse(contentType, 200,"OK");
}

Stream HTTPRequestData::sendResponse(StrViewA contentType, int statusCode) {
	return sendResponse(contentType, statusCode,getStatusCodeMsg(statusCode));
}

Stream HTTPRequestData::sendResponse(StrViewA contentType, int statusCode,
		StrViewA statusMessage) {

	sendResponseLine(statusCode, statusMessage);
	return sendHeaders(statusCode, nullptr, &contentType, nullptr);


}

void HTTPRequestData::sendResponse(const HTTPResponse& resp, StrViewA body) {
	sendResponseLine(resp.getCode(), resp.getStatusMessage());
	Stream s = sendHeaders(resp.getCode(), &resp, nullptr, &body.length);
	s.write(BinaryView(body));
	s.flush();


}

Stream HTTPRequestData::sendResponse(const HTTPResponse& resp) {
	sendResponseLine(resp.getCode(), resp.getStatusMessage());
	return sendHeaders(resp.getCode(), &resp, nullptr, nullptr);

}

void HTTPRequestData::redirect(StrViewA url, Redirect type) {

	HTTPResponse resp((int)type);
	resp("Location", url);

	sendResponse(resp,StrViewA());


}

Stream HTTPRequestData::prepareStream(const Stream& stream) {

	host = operator[](HOST);

	HeaderValue con = operator[](CONNECTION);
	if (con == "keep-alive") keepAlive = true;
	else if (con == "close") keepAlive = false;

	HeaderValue te = operator[](TRANSFER_ENCODING);
	if (te == "chunked") {
		return new ChunkedStream<16>(stream);
	} else {
		HeaderValue cl = operator[](CONTENT_LENGTH);
//		bool hasLength = false;
		long length = 0;
		if (cl.defined()) {
			if (isdigit(cl[0])) {
				length = std::strtol(cl.data,0,10);
			}
		}
		return new LimitedStream(stream, length,0,0);
	}
}


HTTPResponse::HTTPResponse(int code):code(code), message(pool.add(getStatusCodeMsg(code)))
{
}

HTTPResponse::HTTPResponse(int code, StrViewA response):code(code),message(pool.add(response)) {
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
	KeepAliveFn(RefCntPtr<HTTPRequestData> h, AsyncProvider p):h(h),p(p) {}
	void operator()() const {

		RefCntPtr<HTTPRequestData> h(this->h);

		try {
			//We must read rest of the body, to keep protocol in sync
			//However if the rest body is too large, it is better to close connection instead
			//So only two reads are performed. If the second read returns a data, connection is closed
			//This allows to continue in keepalive when only small part of body left unprocessed

			Stream stream = h->getBodyStream();
			if (p!= nullptr) {
				if (stream == nullptr) {
					p->runAsync([h]{
						h->originStream->flush();
						h->handleKeepAlive();
					});
				} else {

					//use async in advise
					stream->readAsync([h,stream](AsyncState st, BinaryView ) {
						//flush any output buffers
						h->originStream->flush();
						//continue if the body is processed
						if (st == asyncEOF) h->handleKeepAlive();
						else if (st == asyncOK) {
							//discard data and try once extra read
							stream->readAsync([h,stream](AsyncState st, BinaryView ) {
								//if eof reached, continue in keep alive
								if (st == asyncEOF) h->handleKeepAlive();
							});
						}
						//discard connection at all
					});
				}
			}	else {
				if (stream != nullptr) {
					//perform synchronously read
					BinaryView b = stream->read();
					//if non-empty - read again
					if (!b.empty()) {
						//read again
						b = stream->read();
						//if not empty, drop keepalive
						if (!b.empty()) return;
					}
				}
				h->originStream->flush();
				//continue keepalive
				h->handleKeepAlive();
			}
		} catch (...) {}

	}
protected:
	RefCntPtr<HTTPRequestData> h;
	AsyncProvider p;

};

Stream HTTPRequestData::sendHeaders(int code, const HTTPResponse* resp,
		const StrViewA* contentType, const size_t* contentLength) {

	intptr_t lgCtxLen = -1;
	StrViewA lgCtxType;

	struct Flags {
		bool hasCtt = false;
		bool hasCtl = false;
		bool hasTE = false;
		bool hasClose = false;
	};

	Flags flags;
	std::size_t bodyLimit = -1;
	std::size_t zero = 0;

	bool usechunked = false;

	if (code == 204) {

		//when code 204, server should not generate any content
		//so transfer encoding and content type should not apper in headers

		contentType = 0;
		contentLength = &zero;
		flags.hasCtl = false;
		flags.hasCtt = true;
		flags.hasTE = true;
	}


	if (contentType) {
		originStream << CONTENT_TYPE << ": " << *contentType << CRLF;
		flags.hasCtt = true;
		lgCtxType = *contentType;
	}
	if (contentLength) {
		originStream << CONTENT_LENGTH << ": " << *contentLength << CRLF;
		flags.hasCtl = true;
		lgCtxLen = *contentLength;
		bodyLimit = *contentLength;
	}

	if (resp) {

		resp->forEach([&](StrViewA key, StrViewA value)->bool {

			if (key == CONTENT_TYPE) {
				if (flags.hasCtt) return true;
				flags.hasCtt = true;
				lgCtxType =value;
			}
			else if (key == CONTENT_LENGTH) {
				if (flags.hasCtl) return true;
				flags.hasCtl = true;
				bodyLimit = std::strtol(value.data,0,10);
				lgCtxLen = bodyLimit;
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
			} else if (key == SET_COOKIE) {
				//exception - Set-Cookie is split to multiple headers
				//however - we don't suport duplicate keys
				//so it is possible to define cookies as comma-separated list
				//
				//This part splits comma separated list into separate lines
				auto splt = value.split(",");
				while (!!splt) {
					StrViewA row = splt().trim(isspace);
					originStream << key << ": " << row << CRLF;
				}
				return true;
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


	KeepAliveFn nxfn(this,originStream.getAsyncProvider());

	log.progress("$1 $2 $3 $4 $5 $6", host, method, path, code, lgCtxType, lgCtxLen);

	//when code 204, server should not generate any content
	//so transfer encoding and content type should not apper in headers
	if (code == 204) {

		originStream << CRLF;

		//Create empty limited stream, no content will allowed to the stream
		return new LimitedStreamWrap<KeepAliveFn>(nxfn, originStream, 0);

	} else {

		if (!flags.hasCtt) {
			originStream << CONTENT_TYPE << ": text/plain\r\n";
		}
		//do not put chunked in case that 101 is reported
		//also when function has content length
		//or already put Trasfer-Encoding
		//or is not keepAlive
		if (!flags.hasCtl && !flags.hasTE && keepAlive && code != 101) {
			originStream << TRANSFER_ENCODING << ": chunked\r\n";
			usechunked = true;
		}

		originStream << CRLF;

		if (method == "HEAD") {
			//In HEAD mode, headers are complete, but no content should be generated
			return new LimitedStreamWrap<KeepAliveFn>(nxfn, originStream, 0);
		} else if (usechunked) {
			//use chunked protocol
			return new ChunkedStreamWrap<KeepAliveFn>(nxfn, originStream);
		} else if (bodyLimit!=(std::size_t)-1) {
			//is limit defined, use limit stream
			return new LimitedStreamWrap<KeepAliveFn>(nxfn,originStream,bodyLimit);
		} else {
			//use original stream
			return originStream;
		}
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

Stream HTTPRequestData::getBodyStream() {
	return reqStream;
}

void HTTPRequestData::readBodyAsync(std::size_t maxSize, HTTPHandler completion) {
	userBuffer.clear();
	readBodyAsync_cont1(maxSize, completion);
}

bool HTTPRequestData::redirectToFolderRoot(Redirect type) {
	StrViewA p = getPath();
	if (!p.empty() && p[p.length-1] == '/') return false;
	std::string path = p;
	path.append("/");
	redirect(path,type);
	return true;
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
			reqStream.readAsync(MutableBinaryView(&userBuffer[end],remain),
				[me, maxSize,completion](AsyncState , const BinaryView data) {

				me->userBuffer.resize(me->userBuffer.size()-4096+data.length);
				if (data.empty()) {
					try {
						completion(HTTPRequest(me));
					} catch (const HTTPStatusException &e) {
						if (!me->responseSent) {
							me->sendErrorPage(e.getStatusCode(), e.getStatusMessage());
						}
					} catch (const std::exception &e) {
						if (!me->responseSent) {
							me->sendErrorPage(500, StrViewA("InternalError"), e.what());
						}
					}
				} else {
					me->readBodyAsync_cont1(maxSize, completion);
				}

			});
		}
}


void HTTPRequestData::handleKeepAlive() {
	reqStream = nullptr;
	report_endRequest();
//	logDebug("Called handleKeepAlive: $1", (std::uintptr_t)this);

	if (keepAliveHandler != nullptr && keepAlive) {
		parseHttp(originStream, keepAliveHandler,true);
	}
}

static std::pair<StrViewA, StrViewA> mimeTypes[] = {

       {"txt","text/plain"},
       {"htm","text/html"},
       {"html","text/html"},
       {"php","text/html"},
       {"css","text/css"},
       {"js","application/javascript"},
       {"json","application/json"},
       {"xml","application/xml"},
       {"swf","application/x-shockwave-flash"},
       {"flv","video/x-flv"},

       // images
       {"png","image/png"},
       {"jpe","image/jpeg"},
       {"jpeg","image/jpeg"},
       {"jpg","image/jpeg"},
       {"gif","image/gif"},
       {"bmp","image/bmp"},
       {"ico","image/vnd.microsoft.icon"},
       {"tiff","image/tiff"},
       {"tif","image/tiff"},
       {"svg","image/svg+xml"},
       {"svgz","image/svg+xml"},

       // archives
       {"zip","application/zip"},
       {"rar","application/x-rar-compressed"},
       {"exe","application/x-msdownload"},
       {"msi","application/x-msdownload"},
       {"cab","application/vnd.ms-cab-compressed"},

       // audio/video
       {"mp3","audio/mpeg"},
       {"qt","video/quicktime"},
       {"mov","video/quicktime"},

       // adobe
       {"pdf","application/pdf"},
       {"psd","image/vnd.adobe.photoshop"},
       {"ai","application/postscript"},
       {"eps","application/postscript"},
       {"ps","application/postscript"},

       // ms office
       {"doc","application/msword"},
       {"rtf","application/rtf"},
       {"xls","application/vnd.ms-excel"},
       {"ppt","application/vnd.ms-powerpoint"},

       // open office
       {"odt","application/vnd.oasis.opendocument.text"},
       {"ods","application/vnd.oasis.opendocument.spreadsheet"}
};

bool HTTPRequestData::sendFile(StrViewA content_type,StrViewA pathname, bool etag, std::size_t cache_secs) {
	char *fname = (char *)alloca(pathname.length+1);
	std::memcpy(fname, pathname.data, pathname.length);
	fname[pathname.length] = 0;

	HTTPResponse resp(200);

	if (content_type.empty()) {
		auto pos = pathname.lastIndexOf(".");
		if (pos != pathname.npos) {
			StrViewA ext = pathname.substr(pos+1);
			for (auto &&itm : mimeTypes) {
				if (itm.first == ext) {
					content_type = itm.second;
					break;
				}
			}
			if (content_type.empty()) {
				content_type ="application/octet-stream";
			}
		}
	}

	if (etag) {

		struct stat statbuf;
		if (stat(fname,&statbuf) == -1) {
			return false;
		} else {
			static char hexChars[]="0123456789ABCDEF";

			char *hexBuff = (char *)alloca(sizeof(statbuf.st_mtim)*2+3);
			char *p = hexBuff;
			*p++='"';
			BinaryView data(reinterpret_cast<const unsigned char *>(&statbuf.st_mtim), sizeof (statbuf.st_mtim));
			for (unsigned int c : data) {
				*p++ = hexChars[c>>4];
				*p++ = hexChars[c & 0xF];
			}
			*p++='"';
			*p = 0;
			StrViewA curEtag(hexBuff, p-hexBuff);
			HeaderValue prevEtags = (*this)["If-None-Match"];
			auto spl = prevEtags.split(",");
			while (spl) {
				StrViewA tag = spl();
				tag = tag.trim(&isspace);
				if (tag == curEtag) {
					sendErrorPage(304);
					return true;
				}
			}
			resp("ETag", curEtag);
			if (cache_secs) resp.cacheFor(cache_secs);
		}
	}

	std::fstream file(fname, std::ios::binary | std::ios::in );
	if (!file) {
		return false;
	} else {
		file.seekg(0,std::ios::end);
		std::size_t sz = file.tellg();
		if (sz == 0) {
			sendErrorPage(204);
		} else {
			unsigned char buff[4096];
			file.seekg(0,std::ios::beg);
			int p = file.get();
			if (p == EOF) {
				sendErrorPage(403);
			} else{
				file.putback(p);
				resp.contentLength(sz);
				resp.contentType(content_type);
				Stream out = sendResponse(resp);

				while (sz && !(!file)) {
					file.read(reinterpret_cast<char *>(buff),4096);
					std::size_t cnt = std::min<std::size_t>(sz,file.gcount());
					out.write(BinaryView(buff, cnt),writeWholeBuffer);
					sz -= cnt;
				}
				out.flush();
			}
		}

	}
	return true;
}

StrViewA HTTPRequestData::getHost() const {
	return host;
}

template<typename It>
bool HTTPRequestData::allowMethodsImpl(const It &beg,const It &end) {
	StrViewA m = getMethod();
	std::size_t chrs = 0;
	for (auto it = beg; it != end; ++it) {
		StrViewA n = *it;
		chrs+=n.length+2;
		if (n.length == m.length) {
			std::size_t i = 0;
			while (i < n.length && toupper(m[i]) == toupper(n[i]))
				++i;
			if (i == n.length) return true;
		}
	}

	bool sep = false;
	std::string allowValue;
	allowValue.reserve(chrs);
	for (auto it = beg; it != end; ++it) {
		if (sep) allowValue.append(", ");
		else sep = true;
		StrViewA n = *it;
		allowValue.append(n.data, n.length);
	}
	Stream s = this->sendResponse(HTTPResponse(405)("Allow",allowValue).contentLength(0));
	s.flush();
	return false;
}


bool HTTPRequestData::allowMethods(std::initializer_list<StrViewA> methods) {
	return allowMethodsImpl(methods.begin(),methods.end());
}

void HTTPCounters::report(std::size_t reqTime_ms) {
	if (reqTime_ms<long_respone_ms) {
		++reqCount;
		reqTime+=(reqTime_ms);
		reqTime2+=(reqTime_ms*reqTime_ms);
	} else if (reqTime_ms < very_long_respone_ms) {
		++long_reqCount;
		long_reqTime+=(reqTime_ms);
		long_reqTime2+=(reqTime_ms*reqTime_ms);
	} else  {
		++very_long_reqCount;
		very_long_reqTime+=(reqTime_ms);
		very_long_reqTime2+=(reqTime_ms*reqTime_ms);
	}
}

HTTPCounters::Data HTTPCounters::getCounters() const {
	return {
		reqCount.load(),
		reqTime.load(),
		reqTime2.load(),
		long_reqCount.load(),
		long_reqTime.load(),
		long_reqTime2.load(),
		very_long_reqCount.load(),
		very_long_reqTime.load(),
		very_long_reqTime2.load()
	};
}

HTTPCounters::HTTPCounters()
:reqCount(0), reqTime(0), reqTime2(0)
,long_reqCount(0), long_reqTime(0), long_reqTime2(0)
,very_long_reqCount(0), very_long_reqTime(0), very_long_reqTime2(0)
{

}

}
