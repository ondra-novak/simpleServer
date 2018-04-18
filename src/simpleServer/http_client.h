/*
 * http_client.h
 *
 *  Created on: Mar 29, 2018
 *      Author: ondra
 */

#ifndef SRC_SIMPLESERVER_SRC_SIMPLESERVER_HTTP_CLIENT_H_
#define SRC_SIMPLESERVER_SRC_SIMPLESERVER_HTTP_CLIENT_H_
#include "shared/stringview.h"
#include "abstractStream.h"
#include "http_headers.h"
#include "address.h"
#include <memory>
#include <mutex>
#include <unordered_map>

using ondra_shared::StrViewA;
using ondra_shared::BinaryView;

namespace simpleServer {

class HttpClientParser: public RefCntObj {
public:
	HttpClientParser(Stream connection, bool useHttp10 = false);

	typedef std::function<void(AsyncState state, Stream stream)> Callback;

	void send(StrViewA method, StrViewA uri, SendHeaders &&headers);
	void send(StrViewA method, StrViewA uri, SendHeaders &&headers, BinaryView data);
	void send(StrViewA method, StrViewA uri, SendHeaders &&headers, StrViewA data);
	Stream sendStream(StrViewA method, StrViewA uri, SendHeaders &&headers);
	///Sends request asynchronously
	/**
	 *
	 * @param method method GET, POST, PUT, ...
	 * @param url target url
	 * @param headers headers
	 * @param cb callback function
	 *
	 * @note function can be used with synchronous connection, however in this case, operation
	 * is executed synchronously (callback is called in current thread)
	 *
	 * @note you have to destroy stream before the response can be read
	 */
	void sendAsync(StrViewA method, StrViewA uri, SendHeaders &&headers, const Callback &cb);

	Stream read();
	///Read response asynchronously
	/**
	 *
	 * @note You have to close request before the response can be ready.
	 *
	 * @param cb callback function containing response stream
	 *
	 * @note function can be used with synchronous connection, however in this case, operation
	 * is executed synchronously (callback is called in current thread)
	 */
	void readAsync(const Callback &cb);

	int getStatus() const;
	StrViewA getMessage() const;
	Stream getBody() const;
	Stream getConnection() const;


	void discardBody();

	///Discards body asynchronously
	/**
	 * @note function can be used with synchronous connection, however in this case, operation
	 * is executed synchronously (callback is called in current thread)
	 **/

	void discardBodyAsync(const IAsyncProvider::CompletionFn &fn);
	bool getKeepAlive() const;



	const ReceivedHeaders &getHeaders() const;

	class ConnectionReset: public Exception {
	public:
		virtual  std::string getMessage() const;
	};

	bool testConnection() const;

protected:
	Stream conn;
	int status;
	StrViewA msg;
	StrViewA httpver;
	ReceivedHeaders hdrs;
	Stream body;
	std::vector<unsigned char> buffer;
	bool useHttp10;
	bool keepAlive;

	void prepareRequest(StrViewA method, StrViewA uri, const SendHeaders &headers);
	Stream parseResponse(bool success);

private:
	std::size_t prepareRequestForStream(
			const ondra_shared::StrViewA& method,
			const ondra_shared::StrViewA& uri, SendHeaders&& headers);
};






struct HttpConnectionInfo {
	std::string addrport;
	bool https;

	struct Hash {
		std::size_t operator()(const HttpConnectionInfo &nfo) const;
	};
	bool operator==(const HttpConnectionInfo &other)const;
	bool operator!=(const HttpConnectionInfo &other)const {return !operator==(other);}

};

class AbstractHttpConnPool: public RefCntObj {
public:
	virtual void addToPool(const HttpConnectionInfo &ident, RefCntPtr<HttpClientParser> parser) = 0;
	virtual ~AbstractHttpConnPool() {}
};


class IHttpProxyProvider {
public:

	struct ParsedUrl: public HttpConnectionInfo {
		std::string path;
		std::string host;
		std::string auth;
		bool force_http10;
	};

	virtual ParsedUrl translate(StrViewA url) = 0;
	virtual ~IHttpProxyProvider() {}
};

class IHttpsProvider {
public:
	virtual Stream connect(Stream conn, StrViewA hostname) = 0;

};


class HttpResponse {
public:
	HttpResponse(RefCntPtr<AbstractHttpConnPool> pool,
				RefCntPtr<HttpClientParser> parser,
				const HttpConnectionInfo &connInfo
				):pool(parser->getKeepAlive()?pool:nullptr),parser(parser),connInfo(connInfo) {
	}
	~HttpResponse();

	int getStatus() const {return parser->getStatus();}
	StrViewA getMessage() const {return parser->getMessage();}
	Stream getBody() const {return parser->getBody();}
	const ReceivedHeaders &getHeaders() const {return parser->getHeaders();}
	void disableKeepAlive() {pool = nullptr;}


protected:
	RefCntPtr<AbstractHttpConnPool> pool;
	RefCntPtr<HttpClientParser> parser;
	HttpConnectionInfo connInfo;

};

class SSLClientFactory;

///Creates https provider, this function requires openssl library
/**
 * @param sslfactory pointer to custom client factory. If nullptr is supplied, then
 *  default sslfactory will be used
 * @return pointer to https provider. To destroy the object, you have to delete the pointer
 */
IHttpsProvider *newHttpsProvider(SSLClientFactory *sslfactory = nullptr);
///Creates proxy provider which doesnt provide any proxy.
IHttpProxyProvider *newNoProxyProvider();
///Creates proxy provider which sends all trafic through the specified proxy
/**
 *
 * @param addrport address:port
 * @param secure set true to use secure connection
 * @return proxy provider
 */
IHttpProxyProvider *newBasicProxyProvider(const StrViewA &addrport, bool secure, bool force_http10);

class HttpClient {
public:

	typedef RefCntPtr<HttpClientParser> PHttpConn;
	typedef IHttpProxyProvider::ParsedUrl ParsedUrl;
	///Creates http client
	/**
	 * @param userAgent specify user agent
	 * @param https pointer to https provider. The ownership is transfered to the HttpClient. Do not share the provider
	 *   with other instances. If this pointer is null, then https is not available
	 * @param proxy pointer to proxy provider. The ownership is transfered to the HttpClient. Do not share the provider
	 *   with other instances. If this pointer is null, then newNoProxyProvider() is used
	 */
	HttpClient(const StrViewA &userAgent = StrViewA("simpleServer::HttpClient (https://www.github.com/ondra-novak/simpleServer)"),
				IHttpsProvider *https = nullptr,
				IHttpProxyProvider *proxy = nullptr);

	virtual HttpResponse request(const StrViewA &method, const StrViewA &url, SendHeaders &&headers);
	virtual HttpResponse request(const StrViewA &method, const StrViewA &url, SendHeaders &&headers, BinaryView data);
	virtual HttpResponse request(const StrViewA &method, const StrViewA &url, SendHeaders &&headers, StrViewA data);

	typedef std::function<void(AsyncState, HttpResponse &&)> ResponseCallback;
	typedef std::function<void(ResponseCallback)> AsyncResponse;
	typedef std::function<void(AsyncState, Stream, AsyncResponse)> RequestCallback;

	virtual void request_async(const StrViewA &method, const StrViewA &url, SendHeaders &&headers, RequestCallback cb);
	///Performs asynchronous request
	/**
	 * @param method
	 * @param url
	 * @param headers
	 * @param data - body data
	 * @param cb callback.
	 *
	 * @note the request is still generated synchronously, however
	 * the response is read asynchronously.
	 */
	virtual void request_async(const StrViewA &method, const StrViewA &url, SendHeaders &&headers, const BinaryView &data, ResponseCallback cb);

	///Sets I/O timeout (default is infinite)
	HttpClient &&setIOTimeout(int t_ms);
	///Sets connect timeout (default is infinite)
	HttpClient &&setConnectTimeout(int t_ms);
	///Assign asynchronous provider.
	/** This is required to allow asynchronous operations. Note that
	 * pointer's ownership is not transfered. Caller also must not destroy
	 * the provider when it is in use.
	 *
	 * @param provider pointer to provider. Set nullptr to disable asynchronous functions
	 * @return
	 */
	HttpClient &&setAsyncProvider(AsyncProvider provider);


	void setHttpsProvider(IHttpsProvider *provider);
	void setProxyProvider(IHttpProxyProvider *provider);


protected:

	class PoolControl: public AbstractHttpConnPool {
	public:
		PoolControl() {}

		PHttpConn getConnection(const HttpConnectionInfo &key);
		virtual void addToPool(const HttpConnectionInfo &ident, PHttpConn parser) override;

	protected:

		typedef std::unordered_multimap<HttpConnectionInfo, PHttpConn , HttpConnectionInfo::Hash> ConnMap;
		typedef std::unique_lock<std::mutex> Sync;
		ConnMap connMap;
		std::mutex lock;

	};

	template<typename Fn>
	auto forConnection(const ParsedUrl &nfo,  Fn &&fn) -> decltype(fn(std::declval<PHttpConn >()));
	template<typename Fn>
	void forConnectionAsync(const ParsedUrl &nfo,  Fn &&fn);


	RefCntPtr<PoolControl> pool;
	std::string userAgent;
	std::shared_ptr<IHttpsProvider> httpsProvider;
	std::unique_ptr<IHttpProxyProvider> proxyProvider;
	int iotimeout = -1;
	int connect_timeout = 30000;
	AsyncProvider asyncProvider;


	void send(PHttpConn conn, StrViewA method, const ParsedUrl &parsed, SendHeaders &&hdrs);
	void send(PHttpConn conn, StrViewA method, const ParsedUrl &parsed, SendHeaders &&hdrs, const BinaryView &data);
};

class WebSocketStream;

WebSocketStream connectWebSocket(HttpClient &client, StrViewA url, SendHeaders &&hdrs = SendHeaders());
void connectWebSocketAsync(HttpClient &client, StrViewA url, SendHeaders &&hdrs, std::function<void(AsyncState, WebSocketStream)> cb);





} /* namespace simpleServer */

#endif /* SRC_SIMPLESERVER_SRC_SIMPLESERVER_HTTP_CLIENT_H_ */
