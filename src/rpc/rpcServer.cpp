/*
 * rpcServer.cpp
 *
 *  Created on: 31. 10. 2017
 *      Author: ondra
 */

#include "rpcServer.h"
#include <imtjson/serializer.h>
#include <imtjson/parser.h>
#include "../simpleServer/websockets_stream.h"
#include "../simpleServer/asyncProvider.h"
#include "../simpleServer/http_hostmapping.h"
#include "../simpleServer/query_parser.h"

#include "../simpleServer/logOutput.h"
#include "resources.h"

namespace simpleServer {

using ondra_shared::LogLevel;
using ondra_shared::SharedLogObject;
using ondra_shared::AbstractLogProvider;






RpcHandler::RpcHandler(RpcServer& rpcserver)
	:rpcserver(rpcserver) {
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


static void handleLogging(const SharedLogObject logObj, const Value &v, const RpcRequest &req) noexcept {
	if (logObj.isLogLevelEnabled(LogLevel::progress)) {
		try {

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
		} catch (...) {

		}
	}
}




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

					handleLogging(logObj,v,req);

					try {

						Stream out = httpreq.sendResponse("application/json");
						v.serialize(out);
						out.flush();
						return true;

					} catch (...) {
						return false;
					}
				}, RpcFlags::preResponseNotify);
				srv(rrq);
			}
		});
	} else if (vpath.empty()) {
		req.redirectToFolderRoot();
	} else {
		Resource *selRes = nullptr;
		StrViewA fname;
		QueryParser qp(vpath);
		auto splt = qp.getPath().split("/");
		while (splt) fname = splt();
		if (fname.empty()) fname="index.html";

		if (fname == "index.html") selRes = consoleEnabled?&client_index_html:nullptr;
		else if (fname == "styles.css") selRes = consoleEnabled?&client_styles_css:nullptr;
		else if (fname == "rpc.js") selRes = &client_rpc_js;

		if (selRes == nullptr) {
			req.sendErrorPage(404);
		} else {
			req.sendResponse(selRes->contentType, selRes->data);
		}

	}

	return true;
}


void RpcHttpServer::addRPCPath(String path) {
	Config cfg;
	addRPCPath(path,cfg);
}
void RpcHttpServer::addRPCPath(String path, const Config &cfg) {
	RpcHandler h(*this);
	if (cfg.maxReqSize) h.setMaxReqSize(cfg.maxReqSize);
	h.enableConsole(cfg.enableConsole);
	WebSocketHandler ws(h);
	auto h2 = [=](HTTPRequest req, StrViewA vpath) mutable {
		if (!ws(req,vpath)) return h(req,vpath);
		else return true;
	};

	mapRecords.push_back(Item(path,HTTPMappedHandler(h2)));

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
			try {
				v.serialize(s);
				s << "\n";
				s.flush();
				return true;
			} catch (...) {
				return false;
			}

		}, RpcFlags::notify);
		this->operator ()(req);
		s.readAsync([=](simpleServer::AsyncState, const ondra_shared::BinaryView &b) {
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
		try {
			int b = s.peek();
			if (b == '{') {//starting with RPC protocol

				directRpcAsync(s);
				return true;
			} else {
				return false;
			}
		} catch (...) {
			return true;
		}
	};
	HostMappingHandler hostMap;
	hostMap.setMapping(hostMapping);
	HttpStaticPathMapperHandler stHandler(hndl);
	(*this)>>(hostMap>>HTTPMappedHandler(stHandler));
}

void RpcHandler::operator ()(simpleServer::HTTPRequest httpreq, WebSocketStream wsstream) const {

	if (wsstream.getFrameType() == WSFrameType::text) {
		Value jreq;
		try {
			 jreq = Value::fromString(wsstream.getText());
		} catch (std::exception &e) {
			Value genError = Object("error",rpcserver.formatError(-32700,"Parse error",Value()));
			wsstream.postText(genError.stringify());
			return;
		}

		SharedLogObject logObj(*httpreq->log, "RPC");
		RpcRequest rrq = RpcRequest::create(jreq,[wsstream,logObj](const Value &v, const RpcRequest &req){
			WebSocketStream ws(wsstream);
			if (!v.defined()) {
				if (ws.isClosed()) return false;
				try {
					ws.ping(BinaryView());
					return true;
				} catch (...) {
					return false;
				}
			}

			handleLogging(logObj,v,req);
			try {
				ws.postText(v.stringify());
				return true;
			} catch (...) {
				return false;
			}
		},RpcFlags::preResponseNotify|RpcFlags::postResponseNotify);
		rpcserver(rrq);
	}
}


} /* namespace hflib */

