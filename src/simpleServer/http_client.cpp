/*
 * http_client.cpp
 *
 *  Created on: Mar 29, 2018
 *      Author: ondra
 */

#include "tcp.h"
#include "chunkedStream.h"
#include "http_client.h"
#include "limitedStream.h"
#include "base64.h"
#include <random>

#include "websockets_stream.h"
namespace simpleServer {

HttpClientParser::HttpClientParser(Stream connection, bool useHTTP10)
:conn(connection),status(0),useHttp10(useHTTP10) {}

void HttpClientParser::send(StrViewA method, StrViewA uri, SendHeaders&& headers) {
	prepareRequest(method,uri,headers);
	conn.write(buffer);
}

void HttpClientParser::send(StrViewA method, StrViewA uri,
		SendHeaders&& headers, BinaryView data) {

	headers.contentLength(data.length);

	prepareRequest(method,uri,headers);
	conn.write(buffer);
	if (!testConnection()) throw ConnectionReset();
	conn.write(data);
}

void HttpClientParser::send(StrViewA method, StrViewA uri,
		SendHeaders&& headers, StrViewA data) {
	send(method, uri, std::move(headers), BinaryView(data));
}

std::size_t HttpClientParser::prepareRequestForStream(
		const StrViewA& method, const StrViewA& uri, SendHeaders&& headers) {
	std::size_t bodyLen(-1);
	HeaderValue h = headers["Content-Length"];
	if (h.defined()) {
		bodyLen = h.getUInt();
	} else {
		headers("Transfer-Encoding", "chunked");
	}
	prepareRequest(method, uri, headers);
	return bodyLen;
}

Stream HttpClientParser::sendStream(StrViewA method, StrViewA uri, SendHeaders&& headers) {


	auto bodyLen = prepareRequestForStream( method, uri,std::move(headers));
	conn.write(buffer);
	if (bodyLen != std::size_t(-1)) {
		return new LimitedStream(conn, 0, bodyLen, 0);
	} else {
		return new ChunkedStream<65536>(conn);
	}
}

void HttpClientParser::sendAsync(StrViewA method, StrViewA url, SendHeaders&& headers, const Callback& cb) {
	if (conn.canRunAsync()) {
		auto bodyLen = prepareRequestForStream( method, url, std::move(headers));
		Stream c(conn);
		Callback cbc(cb);
		RefCntPtr<HttpClientParser> me(this);
		conn.writeAsync(buffer, [me,bodyLen,cbc](AsyncState st, BinaryView ){
			if (st == asyncOK) {
				if (!me->testConnection()) {
					try {
						throw ConnectionReset();
					} catch (...) {
						cbc(asyncError, Stream());
					}
				}
				if (bodyLen != std::size_t(-1)) {
					cbc(st,new LimitedStream(me->conn, 0, bodyLen, 0));
				} else {
					cbc(st,new ChunkedStream<65536>(me->conn));
				}
			} else {
				cbc(st, Stream());
			}
		}, true);
	} else {
		Stream r;
		try {
			r = sendStream(method,url,std::move(headers));
		} catch (...) {
			cb(asyncError, r);
			return;
		}
		cb(asyncOK,r);
	}
}

Stream HttpClientParser::read() {
	conn.flush();
	hdrs.clear();
	return parseResponse(hdrs.parse(conn));
}

Stream HttpClientParser::parseResponse(bool success) {
	keepAlive = false;
	if (success) {
		StrViewA fl = hdrs.getFirstLine();
		auto splt = fl.split(" ",2);
		HeaderValue ver = splt();
		HeaderValue strst = splt();
		HeaderValue stmsg = splt();
		this->status = strst.getUInt();
		this->msg = stmsg;
		this->httpver = ver;
		keepAlive = ver == "HTTP/1.1";
		HeaderValue hc = hdrs["Connection"];
		if (hc == "close") keepAlive = false;
		else if (hc == "keep-alive") keepAlive = true;
		else if (hc.defined()) keepAlive = false;

		if (this->status == 101) {
			keepAlive = false;
		}

		HeaderValue te = hdrs["Transfer-Encoding"];
		if (te == "chunked") {
			return body = new ChunkedStream<65536>(conn);
		} else {
			HeaderValue cl = hdrs["Content-Length"];
			if (cl.defined()) {
				std::uintptr_t len = cl.getUInt();
				return body = new LimitedStream(conn,len,0,0);
			} else {
				if (status == 204) {
					return body = new LimitedStream(conn,0,0,0);
				} else  {
					keepAlive = false;
					return body = conn;
				}
			}
		}

	} else {
		status = 0;
		msg = "Connection lost";
		return body = new LimitedStream(conn, 0,0,0);
	}
}

void HttpClientParser::readAsync(const Callback& cb) {
	if (conn.canRunAsync()) {
		conn.flush();
		RefCntPtr<HttpClientParser> me(this);
		hdrs.parseAsync(conn, [=](AsyncState st) {
			if (st == asyncOK) {
				cb(st,me->parseResponse(true));
			} else if (st == asyncEOF) {
				cb(asyncOK,me->parseResponse(false));
			} else {
				cb(st, nullptr);
			}
		});
	} else {
		Stream r;
		try {
			r = read();
		} catch (...) {
			cb(asyncError, r);
		}
		cb(asyncOK, r);
	}
}


int HttpClientParser::getStatus() const {
	return status;
}

StrViewA HttpClientParser::getMessage() const {
	return msg;
}

const ReceivedHeaders& HttpClientParser::getHeaders() const {
	return hdrs;
}

void HttpClientParser::prepareRequest(StrViewA method, StrViewA uri, const SendHeaders& headers) {
	buffer.clear();
	auto buffWrite = [&](BinaryView x) {for(auto &&y:x) buffer.push_back(y);};
	const BinaryView spc((StrViewA(" ")));
	const BinaryView nl((StrViewA("\r\n")));


	buffWrite(BinaryView(method));
	buffWrite(spc);
	buffWrite(BinaryView(uri));
	buffWrite(spc);
	if (useHttp10) buffWrite(BinaryView(StrViewA("HTTP/1.0")))
			; else buffWrite(BinaryView(StrViewA("HTTP/1.1")));
	buffWrite(nl);
	headers.generate(buffWrite);

}


Stream HttpClientParser::getBody() const {
	return body;
}

void HttpClientParser::discardBody() {
	while (!body.read(false).empty()) {}
}

bool HttpClientParser::getKeepAlive() const {
	return keepAlive;
}

void HttpClientParser::discardBodyAsync(
					const IAsyncProvider::CompletionFn& fn) {

	if (body.canRunAsync()) {
		IAsyncProvider::CompletionFn c(fn);
		RefCntPtr<HttpClientParser> me(this);
		body.readAsync([me,c](AsyncState st, BinaryView) {
			if (st == asyncOK){
				me->discardBodyAsync(c);
			} else {
				c(st);
			}
		});
	} else {
		try {
			discardBody();
		} catch (...) {
			fn(asyncError);
			return;
		}
		fn(asyncOK);
	}

}

std::string HttpClientParser::ConnectionReset::getMessage() const {
	return "HTTP connection reset";
}

bool HttpClientParser::testConnection() const {
	try {
		return !AbstractStream::isEof(conn.read(true));
	} catch (...) {
		return false;
	}
}

std::size_t HttpConnectionInfo::Hash::operator ()(const HttpConnectionInfo& nfo) const {
	return std::hash<std::string>()(nfo.addrport);
}

bool HttpConnectionInfo::operator ==(const HttpConnectionInfo& other) const {
	return addrport == other.addrport && https == other.https;
}
HttpResponse::~HttpResponse() {
	if (pool != nullptr && parser->getKeepAlive()) {
		auto pl = pool;
		auto pr = parser;
		auto cn = connInfo;
		parser->discardBodyAsync([pl,pr,cn](AsyncState st){
			if (st == asyncOK && pr->testConnection()) {
				pl->addToPool(cn,pr);
			}
		});
	}
}

class NoProxyProvider: public IHttpProxyProvider {
public:
	virtual ParsedUrl translate(StrViewA url) override;
};


IHttpProxyProvider::ParsedUrl NoProxyProvider::translate(StrViewA url) {
	StrViewA stripped;
	bool https;
	if (url.substr(0,7) == "http://") {
		https = false;
		stripped = url.substr(7);
	} else if (url.substr(0,8) == "https://") {
		https = true;
		stripped = url.substr(8);
	} else {
		throw UnsupportedURLSchema(url);
	}

	std::uintptr_t spltpath = stripped.indexOf("/");
	StrViewA authaddr = stripped.substr(0,spltpath);
	StrViewA req = stripped.substr(spltpath);
	auto amp = authaddr.lastIndexOf("@");
	StrViewA auth;
	StrViewA addrport;
	if (amp == authaddr.npos) addrport = authaddr;
	else {
		auth = authaddr.substr(0,amp);
		addrport = authaddr.substr(amp+1);
	}
	if (req.empty()) req = "/";
	ParsedUrl res;
	res.addrport = addrport;
	res.host = addrport;
	res.auth = base64encode(BinaryView(auth),base64_standard,false);
	res.force_http10 = false;
	res.https = https;
	res.path = req;
	return res;
}

IHttpProxyProvider* newNoProxyProvider() {
	return new NoProxyProvider;
}

class BasicProxyProvider: public IHttpProxyProvider {
public:

	BasicProxyProvider(const StrViewA &addrport, bool secure, bool force_http10)
		:addrport(std::move(addrport)), secure(secure),force_http10(force_http10) {}

	ParsedUrl translate(StrViewA url) override;
public:
	std::string addrport;
	bool secure;
	bool force_http10;
};


IHttpProxyProvider::ParsedUrl BasicProxyProvider::translate(StrViewA url) {

	ParsedUrl res;
	res.addrport = this->addrport;
	res.https = this->secure;
	res.force_http10 = this->force_http10;
	res.path = url;
	auto tmp = NoProxyProvider().translate(url);
	res.host = tmp.host;
	return res;
}

IHttpProxyProvider* newBasicProxyProvider(const StrViewA &addrport, bool secure, bool force_http10) {

	return new BasicProxyProvider(addrport, secure, force_http10);

}

HttpClient::HttpClient(const StrViewA& userAgent, IHttpsProvider* https, IHttpProxyProvider* proxy)
	:pool(new PoolControl)
	,userAgent(userAgent)
	,httpsProvider(https)
	,proxyProvider(proxy?proxy:newNoProxyProvider())
{

}

template<typename Fn>
auto HttpClient::forConnection(const ParsedUrl &nfo, Fn &&fn) -> decltype(fn(std::declval<PHttpConn >())) {
	PHttpConn conn = pool->getConnection(nfo);
	if (conn != nullptr) {
		try {
			return fn(conn);
		} catch (...) {
			//ignore error, drop connection and try again
		}
	}

	if (nfo.https && httpsProvider == nullptr) {
		throw HttpsIsNotEnabled(nfo.addrport);
	}


	NetAddr addr = NetAddr::create(nfo.addrport,nfo.https?443:80,NetAddr::IPvAll);
	auto factory = TCPConnect::create(addr,connect_timeout,iotimeout);
	Stream s = factory->create();
	s->setAsyncProvider(asyncProvider);
	if (nfo.https) {
		s = httpsProvider->connect(s, nfo.addrport);
	}
	return fn(new HttpClientParser(s,nfo.force_http10));
}

template<typename Fn>
void HttpClient::forConnectionAsync(const ParsedUrl &nfo,  Fn &&fn)  {

	if (asyncProvider == nullptr) {
		try {
			forConnection(nfo, std::move(fn));
		} catch (...) {
			fn(nullptr);
		}
	}

	PHttpConn conn = pool->getConnection(nfo);
	if (conn != nullptr) {
		try {
			return fn(conn);
		} catch (...) {
			//ignore error, drop connection and try again
		}
	}

	if (nfo.https && httpsProvider == nullptr) {
		throw HttpsIsNotEnabled(nfo.addrport);
	}


	NetAddr addr = NetAddr::create(nfo.addrport,nfo.https?443:80,NetAddr::IPvAll);
	auto factory = TCPConnect::create(addr,connect_timeout,iotimeout);
	Fn fn2(fn);
	std::string hostname(nfo.addrport);
	bool force10 = nfo.force_http10;
	std::shared_ptr<IHttpsProvider> https(nfo.https?httpsProvider:std::shared_ptr<IHttpsProvider>(nullptr));
	factory->createAsync(asyncProvider,[fn2,hostname,https,force10] (AsyncState st, Stream conn) mutable {
		if (st == asyncOK) {
			try {
				if (https != nullptr) {
					conn = https->connect(conn, hostname);
				}
			} catch (...) {
				fn2(nullptr);
			}
			fn2(new HttpClientParser(conn, force10));
		} else {
			fn2(nullptr);
		}
	});



}


void HttpClient::send(PHttpConn conn, StrViewA method, const ParsedUrl &parsed, SendHeaders &&hdrs) {
	hdrs("Host",parsed.host);
	conn->send(method, parsed.path, std::move(hdrs));
}

HttpClient&& HttpClient::setAsyncProvider(AsyncProvider provider) {
	this->asyncProvider= provider;
	return std::move(*this);
}

HttpClient&& HttpClient::setIOTimeout(int t_ms) {
	this->iotimeout = t_ms;return std::move(*this);
}

HttpClient&& HttpClient::setConnectTimeout(int t_ms) {
	this->connect_timeout = t_ms;return std::move(*this);
}

void HttpClient::send(PHttpConn conn, StrViewA method, const ParsedUrl &parsed, SendHeaders &&hdrs, const BinaryView &data) {
	hdrs("Host",parsed.host);
	conn->send(method, parsed.path, std::move(hdrs),data);
}


HttpResponse HttpClient::request(const StrViewA& method, const StrViewA& url, SendHeaders&& headers) {
	auto parsed = proxyProvider->translate(url);
	return forConnection(parsed,[&](PHttpConn conn){
		send(conn,method,parsed,std::move(headers));
		conn->read();
		return HttpResponse(RefCntPtr<AbstractHttpConnPool>::staticCast(pool), conn, parsed);
	});
}

HttpResponse HttpClient::request(const StrViewA& method, const StrViewA& url, SendHeaders&& headers, BinaryView data) {
	auto parsed = proxyProvider->translate(url);
	return forConnection(parsed,[&](PHttpConn conn){
		send(conn,method,parsed,std::move(headers),data);
		conn->read();
		return HttpResponse(RefCntPtr<AbstractHttpConnPool>::staticCast(pool), conn, parsed);
	});

}

HttpResponse HttpClient::request(const StrViewA& method, const StrViewA& url, SendHeaders&& headers, StrViewA data) {
	auto parsed = proxyProvider->translate(url);
	return forConnection(parsed,[&](PHttpConn conn){
		send(conn,method,parsed,std::move(headers),BinaryView(data));
		conn->read();
		return HttpResponse(RefCntPtr<AbstractHttpConnPool>::staticCast(pool), conn, parsed);
	});
}

void HttpClient::request_async(const StrViewA &method,
		const StrViewA &url, SendHeaders &&headers, const BinaryView &data,
		ResponseCallback cb) {

	RefCntPtr<AbstractHttpConnPool> p = RefCntPtr<AbstractHttpConnPool>::staticCast(pool);

	auto parsed = proxyProvider->translate(url);

	forConnection(parsed,[&](PHttpConn conn){
		send(conn,method,parsed,std::move(headers),data);
		conn->readAsync([=](AsyncState st, Stream){
			if (st == asyncOK) {
				cb(st, HttpResponse(p, conn, parsed));
			} else {
				cb(st, HttpResponse(nullptr, conn, parsed));
			}
		});;
	});


}


void HttpClient::request_async(const StrViewA& method,
		const StrViewA& url,
		SendHeaders&& headers,
		const RequestCallback cb) {

	auto parsed = proxyProvider->translate(url);

	std::string m(method);
	std::string u(url);
	SendHeaders h(std::move(headers));
	RequestCallback c(cb);
	RefCntPtr<AbstractHttpConnPool> p = RefCntPtr<AbstractHttpConnPool>::staticCast(pool);

	return forConnectionAsync(parsed,[m,u,h,c,p,parsed](PHttpConn conn)mutable{
		if (conn == nullptr) {
			c(asyncError, Stream(), nullptr);
		} else {
			conn->sendAsync(m,u,std::move(h),[c,conn,p,parsed](AsyncState st, Stream s){
				if (st == asyncOK) {
					c(st,s,[conn,parsed,p](ResponseCallback rcb) {
						conn->readAsync([conn,p,parsed,rcb](AsyncState st, Stream) {
							if (st == asyncOK)
								rcb(st, HttpResponse(p,conn,parsed));
							else {
								rcb(st, HttpResponse(nullptr,conn,parsed));
							}
						});
					});
				} else {
					c(st, Stream(),nullptr);
				}
			});
		}
		});



}

RefCntPtr<HttpClientParser> HttpClient::PoolControl::getConnection(
		const HttpConnectionInfo& key) {
	Sync _(lock);
	auto it = connMap.find(key);
	if (it == connMap.end()) return nullptr;
	else {
		auto ret = it->second;
		connMap.erase(it);
		return ret;
	}
}

void HttpClient::PoolControl::addToPool(
		const HttpConnectionInfo& ident, RefCntPtr<HttpClientParser> parser) {
	Sync _(lock);
	connMap.insert(std::make_pair(ident, parser));
}

void HttpClient::setHttpsProvider(IHttpsProvider* provider) {
	this->httpsProvider = std::shared_ptr<IHttpsProvider>(provider);
}

void HttpClient::setProxyProvider(IHttpProxyProvider* provider) {
	this->proxyProvider = std::unique_ptr<IHttpProxyProvider>(provider);
}

static void prepareWS(std::default_random_engine &rnd, SendHeaders &hdrs, StrViewA &url, std::string &tmp) {
	if (url.substr(0,5) == "ws://") {
		tmp.clear();
		tmp.append("http://");
		tmp.append(url.data+5,url.length-5);
		url = tmp;
	} else if (url.substr(0,6) == "wss://") {
		tmp.clear();
		tmp.append("https://");
		tmp.append(url.data+6,url.length-6);
		url = tmp;
	}

	 std::random_device rd;
  	 rnd.seed(rd());
	 std::uniform_int_distribution<> dist(32,127);

	 char key[24];
	 int i,j;
	 auto enc = base64encode([&](char c){key[i++] = c;}, base64_standard,false);

	 for (i=0,j=0; j<16; ++j) {
			enc((unsigned char)dist(rnd));
	 }
	 enc.finish();

	 hdrs("Connection","upgrade")
		   ("Upgrade","websocket")
		   ("Sec-WebSocket-Version","13")
		   ("Sec-WebSocket-Key",StrViewA(key,24));
}


WebSocketStream connectWebSocket(HttpClient &client, StrViewA url, SendHeaders &&hdrs) {
	std::string tmp;
	std::default_random_engine rnd;
	prepareWS(rnd, hdrs,url, tmp);
	HttpResponse resp = client.request("GET",url,std::move(hdrs));
	if (resp.getStatus() != 101 || resp.getHeaders()["Upgrade"] != "websocket") {
		throw HTTPStatusException(resp.getStatus(), resp.getMessage());
	}
	return WebSocketStream(new _details::WebSocketStreamImpl(resp.getBody(),rnd));
}
void connectWebSocketAsync(HttpClient &client, StrViewA url, SendHeaders &&hdrs, std::function<void(AsyncState, WebSocketStream)> cb) {
	std::string tmp;
	std::default_random_engine rnd;
	prepareWS(rnd, hdrs,url, tmp);
	client.request_async("GET",url,std::move(hdrs),
			[cb,rnd](AsyncState st, Stream, HttpClient::AsyncResponse rsp){
		if (st == asyncOK) {
			rsp([cb,rnd](AsyncState st, HttpResponse resp){
				if (st == asyncOK) {
					try {
						if (resp.getStatus() != 101 || resp.getHeaders()["Upgrade"] != "websocket") {
							throw HTTPStatusException(resp.getStatus(), resp.getMessage());
						} else {
							cb(st, WebSocketStream(new _details::WebSocketStreamImpl(resp.getBody(),rnd)));
						}
					} catch (...) {
						cb(asyncError, WebSocketStream());
					}
				} else {
					cb(st, WebSocketStream());
				}
			});
		} else {
			cb(st, WebSocketStream());
		}
	});

}



} /* namespace simpleServer */
