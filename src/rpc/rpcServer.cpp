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
#include "../simpleServer/websockets_stream.h"
#include "../simpleServer/query_parser.h"

#include "../simpleServer/logOutput.h"
#include "resources.h"

namespace simpleServer {

using ondra_shared::LogLevel;
using ondra_shared::SharedLogObject;
using ondra_shared::AbstractLogProvider;


class WSStreamWithContext: public _details::WebSocketStreamImpl {
public:

	typedef _details::WebSocketStreamImpl Super;
	using Super::WebSocketStreamImpl;

	auto getConnContext() const {return ctx;}

protected:
	PRpcConnContext ctx = new RpcConnContext;
};

class WebSocketHandlerWithContext: public WebSocketHandler{
public:
	using WebSocketHandler::WebSocketHandler ;

	virtual WebSocketStream createStream(Stream sx) const {
		return new WSStreamWithContext(sx);
	}
	virtual ~WebSocketHandlerWithContext() {}
};



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
				Stream out = httpreq.sendResponse("application/json");
				RpcRequest rrq = RpcRequest::create(rdata,[httpreq,logObj,out](const Value &v, const RpcRequest &req){

					handleLogging(logObj,v,req);

					try {

						v.serialize(out);
						out << "\r\n";
						return out.flush();

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
	enableDirect = cfg.enableDirect;
	h.enableConsole(cfg.enableConsole);
	if (cfg.enableWS) {
		WebSocketHandlerWithContext ws(h);
		auto h2 = [=](HTTPRequest req, StrViewA vpath) mutable {
			if (!ws(req,vpath)) return h(req,vpath);
			else return true;
		};
		mapRecords.push_back(Item(path,HTTPMappedHandler(h2)));
	} else {
		mapRecords.push_back(Item(path,HTTPMappedHandler(h)));
	}


}


void RpcHttpServer::addPath(String path, simpleServer::HTTPMappedHandler hndl) {
	mapRecords.push_back(std::make_pair(path, hndl));
}

void RpcHttpServer::setHostMapping(const String &mapping) {
	hostMapping = mapping;
}


void RpcHttpServer::directRpcAsync(Stream s) {
	directRpcAsync2(s,new RpcConnContext);
}
void RpcHttpServer::directRpcAsync2(simpleServer::Stream s, PRpcConnContext ctx) {


	auto sendFn =[=](Value v) {
		try {
			v.serialize(s);
			s << "\n";
			s.flush();
			return true;
		} catch (...) {
			return false;
		}

	};


	try {
		BinaryView b = s.read(true);
		while (!b.empty() && isspace(b[0])) b = b.substr(1);
		if (!b.empty()) {
			s.putBack(b);
			Value jsonReq = Value::parse(s);
			RpcRequest req = RpcRequest::create(jsonReq,sendFn, RpcFlags::notify, ctx);
			ctx->store("__last_jsonrpc_ver",req.getVersionField());
			this->operator ()(req);
		}
		s.readAsync([=](simpleServer::AsyncState st, const ondra_shared::BinaryView &b) {
			if (st == asyncTimeout) {
				Value ver = ctx->retrieve("__last_jsonrpc_ver");
				RpcRequest req = RpcRequest::create({Value(),Value(),Value(),Value(),ver},sendFn,RpcFlags::notify);
				req.sendNotify("ping",Value());
				s.readAsync([=](simpleServer::AsyncState st, const ondra_shared::BinaryView &b) {
					if (st == asyncOK) {
						s.putBack(b);
						directRpcAsync2(s,ctx);
					}
				});
			} else if (st == asyncOK) {
				s.putBack(b);
				directRpcAsync2(s,ctx);
			}
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
	if (enableDirect) {
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
	}
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

		WSStreamWithContext::Super *wsx = wsstream;
		WSStreamWithContext *wswc = static_cast<WSStreamWithContext *>(wsx);
		PRpcConnContext connctx;
		if (wswc) connctx = wswc->getConnContext();

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
		},RpcFlags::notify, connctx);
		rpcserver(rrq);
	}
}


} /* namespace hflib */

