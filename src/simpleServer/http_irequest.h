#pragma once

namespace simpleServer {


class IHttpRequest : public RefCntObj {
public:


	virtual StrViewA getMethod() const = 0;
	virtual StrViewA getHttpVersion() const = 0;
	virtual StrViewA getPath() const = 0;
	virtual StrViewA getHost() const = 0;
	virtual HeaderValue getHeader(StrViewA name) const = 0;


};



}
