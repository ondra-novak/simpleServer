

#pragma once
#include <imtjson/rpc.h>
#include <imtjson/string.h>
#include "../simpleServer/http_parser.h"
#include "../simpleServer/http_pathmapper.h"
#include "../simpleServer/http_server.h"
#include "../simpleServer/webSocketsHandler.h"
#include "../simpleServer/logOutput.h"


namespace simpleServer {
using ondra_shared::LogObject;

using namespace json;

class RpcHandler {
public:

	RpcHandler(RpcServer &rpcserver);



	void operator()(simpleServer::HTTPRequest req) const;

	bool operator()(simpleServer::HTTPRequest req, const simpleServer::StrViewA &vpath) const;

	void operator()(simpleServer::HTTPRequest req, WebSocketStream wsstream) const;

	void setMaxReqSize(std::size_t maxReqSize) {
		this->maxReqSize=maxReqSize;
	}
	void enableConsole(bool e) {
		consoleEnabled = e;
	}

protected:


	RpcServer &rpcserver;
	std::size_t maxReqSize=10*1024*1024;
	bool consoleEnabled = false;



};


class RpcHttpServer: public simpleServer::MiniHttpServer, public json::RpcServer {
public:

	using simpleServer::MiniHttpServer::MiniHttpServer;

	struct Config {
		bool enableConsole = true;
		bool enableWS = true;
		bool enableDirect = true;
		std::size_t maxReqSize = 0;
	};


	void addRPCPath(String path);
	void addRPCPath(String path, const Config &cfg);
	void addPath(String path, simpleServer::HTTPMappedHandler hndl);
	void setHostMapping(const String &mapping);
	void addStats(String path, std::function<json::Value()> customStats = nullptr);

	void start();


protected:

	typedef std::pair<String, simpleServer::HTTPMappedHandler>  Item;
	std::vector<Item> mapRecords;
	String hostMapping;

	bool enableDirect = true;
	std::size_t direct_timeout;



	class DirectRpcConnContext: public RpcConnContext {
	public:

		static const char *ident;

		using Ident = ondra_shared::TaskCounter<DirectRpcConnContext>;

		LogObject logObj;
		DirectRpcConnContext();

	};
	void directRpcAsync(simpleServer::Stream s);
	void directRpcAsync2(simpleServer::Stream s, RefCntPtr<DirectRpcConnContext> ctx);
};

class HttpRpcConnContext: public json::RpcConnContext {
public:

	HttpRpcConnContext(const simpleServer::HTTPRequest &req, bool secure):req(req),secure(secure) {}
	virtual Value retrieve(StrViewA key) const;
	void exportToHeader(SendHeaders &hdrs);

	bool isSecure() const {return secure;}


protected:
	mutable Value cookies;
	simpleServer::HTTPRequest req;
	bool secure;
};



} /* namespace hflib */
