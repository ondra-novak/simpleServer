/*
 * rpcServer.cpp
 *
 *  Created on: 31. 10. 2017
 *      Author: ondra
 */

#include "rpcServer.h"
#include <imtjson/serializer.h>
#include <imtjson/parser.h>
#include "../simpleServer/asyncProvider.h"
#include "../simpleServer/http_hostmapping.h"

#include "shared/logOutput.h"


namespace simpleServer {

using ondra_shared::LogLevel;
using ondra_shared::SharedLogObject;
using ondra_shared::AbstractLogProvider;






RpcHandler::RpcHandler(RpcServer& rpcserver)
	:rpcserver(rpcserver) {
}

RpcHandler::RpcHandler(RpcServer& rpcserver, String clientRoot)
	:rpcserver(rpcserver),clientRoot(clientRoot) {
}

void RpcHandler::operator ()(simpleServer::HTTPRequest req) const {

	operator()(req, req.getPath());
}

class RpcServerEnum: public RpcServer {
public:

	template<typename Fn>
	void forEach(const Fn &fn) {
		for(auto &x : mapReg) {
			fn(x.second->name);
		}
	}
};





bool RpcHandler::operator ()(simpleServer::HTTPRequest req, const StrViewA &vpath) const {

	StrViewA method = req.getMethod();
	if (method == "POST") {
		RpcServer &srv(rpcserver);
		req.readBodyAsync(maxReqSize, [&srv](HTTPRequest httpreq){
			auto x = httpreq.getUserBuffer();
			if (x.empty()) {

				RpcServerEnum &enm = static_cast<RpcServerEnum &>(srv);
				Array methods;
				enm.forEach([&methods](Value v){methods.push_back(v);});
				Stream out = httpreq.sendResponse("application/json");
				Value(methods).serialize(out);
				out.flush();

			} else {
				Value rdata = Value::fromString(StrViewA(BinaryView(x)));
				SharedLogObject logObj(*httpreq->log, "RPC");
				RpcRequest rrq = RpcRequest::create(rdata,[httpreq,logObj](const Value &v, const RpcRequest &req){

					if (logObj.isLogLevelEnabled(LogLevel::progress)) {

						Value diagData = req.getDiagData();
						Value args = req.getArgs();
						Value method = req.getMethodName();
						Value context = req.getContext();
						if (!diagData.defined() && req.isErrorSent()) {
							diagData = v["error"];
						}
						if (!diagData.defined()) diagData = nullptr;
						if (!context.defined()) context = nullptr;
						Value output = {method,args,context,diagData};
						logObj.progress("$1", output.toString());
					}



					try {

						Stream out = httpreq.sendResponse("application/json");
						v.serialize(out);
						out.flush();


					} catch (...) {
						//nothing to do - we cannot handle this
					}
				}, RpcFlags::preResponseNotify);
				srv(rrq);
			}
		});
	} else if (vpath.empty()) {
		req.redirectToFolderRoot();
	} else {
		if (clientRoot.empty()) {
			return false;
		} else {
			StrViewA fname;
			auto splt = vpath.split("/");
			while (splt) fname = splt();
			if (fname.empty()) fname="index.html";
			String pathname = { clientRoot,"/",fname};

			req.sendFile(pathname);
		}



	}

	return true;
}


void RpcHttpServer::addRPCPath(String path, String clientRoot, std::size_t maxReqSize) {
	RpcHandler h(*this, clientRoot);
	if (maxReqSize) h.setMaxReqSize(maxReqSize);
	mapRecords.push_back(Item(path, h));

}


void RpcHttpServer::addPath(String path, simpleServer::HTTPMappedHandler hndl) {
	mapRecords.push_back(std::make_pair(path, hndl));
}

void RpcHttpServer::setHostMapping(const String &mapping) {
	hostMapping = mapping;
}

void RpcHttpServer::directRpcAsync(Stream s) {

	try {
		s.setIOTimeout(-1);
		Value jsonReq = Value::parse(s);
		RpcRequest req = RpcRequest::create(jsonReq,[=](Value v) {
			v.serialize(s);
			s << "\n";
			s.flush();

		}, RpcFlags::notify);
		this->operator ()(req);
		s.readAsync([=](simpleServer::AsyncState st, const ondra_shared::BinaryView &b) {
			s.putBack(b);
			directRpcAsync(s);
		});

	} catch (...) {

	}

}

void RpcHttpServer::start() {
	std::vector<HttpStaticPathMapper::MapRecord> reglist;
	reglist.reserve(mapRecords.size());
	for (auto &&x: mapRecords) {
		HttpStaticPathMapper::MapRecord k;
		k.path = x.first;
		k.handler = x.second;
		reglist.push_back(k);
	}
	HttpStaticPathMapper hndl(std::move(reglist));
	this->preHandler = [=](Stream s) {
		int b = s.peek();
		if (b == '{') {//starting with RPC protocol

			directRpcAsync(s);
			return true;
		} else {
			return false;
		}
	};
	HostMappingHandler hostMap;
	hostMap.setMapping(hostMapping);
	HttpStaticPathMapperHandler stHandler(hndl);
	(*this)>>(hostMap>>HTTPMappedHandler(stHandler));
}

} /* namespace hflib */


