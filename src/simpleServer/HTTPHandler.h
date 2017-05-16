#pragma once

namespace simpleServer {


class HTTPRequest;

class HTTPRequestImpl: public RefCntObj {
protected:

	friend class HTTPRequest;



	typedef std::vector<char> InHdrData;
	typedef std::pair<StrViewA, StrViewA> InKeyValue;

	typedef std::pair<StrViewA, unsigned int> OutKey;
	typedef std::pair<OutKey, std::string> OutKeyValue;

	typedef std::vector<InKeyValue> InHdrs;
	typedef std::priority_queue<OutKeyValue> OutHdrs;

	typedef std::function<BinaryView(unsigned int)> InputStream;
	typedef std::function<void(BinaryView)> OutputStream;
	typedef std::function<void(Connection::Callback)> AsyncInputStream;
	typedef std::function<void(BinaryView,Connection::Callback)> AsyncOutputStream;

	unsigned int status;
	StrViewA statusMessage;
	StrViewA outProtocol;

	StrViewA method;
	StrViewA protocol;
	StrViewA path;

	InHdrData inHdrData;
	InHdrs inHdrs;

	OutHdrs outHdrs;

	bool headersSent;

	Connection connection;
	InputStream read;
	OutputStream write;
	AsyncInputStream asyncRead;
	AsyncOutputStream asyncWrite;


protected:


};

class HeaderValue: public StrViewA {
public:
	bool defined;
	HeaderValue():defined(false) {}
	HeaderValue(const StrViewA &str):defined(true),StrViewA(str) {}
};

typedef RefCntPtr<HTTPRequestImpl> PHTTPRequestImpl;

class HTTPRequest  {
public:

	typedef HTTPRequestImpl::HttpCallback HttpCallback;
	typedef HTTPRequestImpl::InputStream InputStream;
	typedef HTTPRequestImpl::OutputStream OutputStream;
	typedef HTTPRequestImpl::AsyncInputStream AsyncInputStream;
	typedef HTTPRequestImpl::AsyncOutputStream AsyncOutputStream;

	StrViewA getMethod() const;
	StrViewA getProtocol() const;
	StrViewA getPath() const;
	HeaderValue getHeader(StrViewA field) const;
	std::size_t getContentLength() const;
	std::vector<unsigned char> getBody() const;

	BinaryView operator()(unsigned int commit) const;
	void operator()(const BinaryView &data) const;

	template<typename Callback>
	void operator()(Callback cb) const;
	template<typename Callback>
	void operator()(const BinaryView &data, Callback cb) const;

	void setStatus(unsigned int code, HeaderValue text) const;
	void setHeader(StrViewA header, std::string value) const;
	void setContentType(StrViewA ctx) const;
	void setContentLength(std::size_t sz) const;
	void forceHTTP10(bool force10) const;
	void isKeepalive() const;

	void sendHeaders() const;
	bool headersSent() const;






protected:

	PHTTPRequestImpl owner;

};

class HTTPHandler {
public:






};


}
