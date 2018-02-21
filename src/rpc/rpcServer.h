

#pragma once
#include <imtjson/rpc.h>
#include <imtjson/string.h>
#include "../simpleServer/http_parser.h"
#include "../simpleServer/http_pathmapper.h"
#include "../simpleServer/http_server.h"
#include <shared/logOutput.h>


namespace simpleServer {
using ondra_shared::LogObject;

using namespace json;

class RpcHandler {
public:

	RpcHandler(RpcServer &rpcserver);
	RpcHandler(RpcServer &rpcserver, String clientRoot);



	void operator()(simpleServer::HTTPRequest req) const;

	bool operator()(simpleServer::HTTPRequest req, const simpleServer::StrViewA &vpath) const;

	void setMaxReqSize(std::size_t maxReqSize) {
		this->maxReqSize=maxReqSize;
	}

protected:


	RpcServer &rpcserver;
	String clientRoot;
	std::size_t maxReqSize=10*1024*1024;



};


class RpcHttpServer: public simpleServer::MiniHttpServer, public json::RpcServer {
public:

	using simpleServer::MiniHttpServer::MiniHttpServer;


	void addRPCPath(String path, String clientRoot, std::size_t maxReqSize = 0);
	void addPath(String path, simpleServer::HTTPMappedHandler hndl);
	void setHostMapping(const String &mapping);

	void start();


protected:

	typedef std::pair<String, simpleServer::HTTPMappedHandler>  Item;
	std::vector<Item> mapRecords;
	String hostMapping;

	void directRpcAsync(simpleServer::Stream s);


};



} /* namespace hflib */
