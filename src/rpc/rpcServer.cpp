/*
 * rpcServer.cpp
 *
 *  Created on: 31. 10. 2017
 *      Author: ondra
 */

#include "rpcServer.h"
#include <imtjson/serializer.h>
#include <imtjson/parser.h>
#include "../simpleServer/urlencode.h"
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

	RefCntPtr<HttpRpcConnContext> ctx;
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


static void handleLogging(const LogObject &logObj, const Value &v, const RpcRequest &req) noexcept {
	if (logObj.isLogLevelEnabled(LogLevel::info)) {
		try {
			Value diagData = req.getDiagData();
			//do not log notifications
			if ((v.type() != json::object || v["method"].defined()) && !diagData.defined()) return;
			Value args = req.getArgs();
			Value method = req.getMethodName();
			Value context = req.getContext();
			if (!diagData.defined() && req.isErrorSent()) {
				diagData = v["error"];
			}
			if (!diagData.defined()) diagData = nullptr;
			if (!context.defined()) context = nullptr;
			Value output = {method,args,context,diagData};
			logObj.info("$1", output.toString());
		} catch (...) {

		}
	}
}

Value HttpRpcConnContext::retrieve(std::string_view key) const {
	retrieve(StrViewA(key));
}

Value HttpRpcConnContext::retrieve(StrViewA key) const {
	Value sup = RpcConnContext::retrieve(key);
	if (sup.defined()) return sup;
	if (!cookies.defined())  {
			Object datain;
			std::string buff;
			std::string vkey;
			auto buffput = [&](char c) {buff.push_back(c);};
			HeaderValue cookie = req["Cookie"];
			if (cookie.defined())  {
				auto parts = cookie.split("; ");
				while (!!parts) {
					UrlDecode<decltype(buffput) &> dec(buffput);

					auto subparts = StrViewA(parts()).split("=",2);
					StrViewA key = subparts();
					StrViewA value = subparts();

					buff.clear();
					for (auto &&c:key) dec(c);
					std::swap(vkey,buff);

					buff.clear();
					for (auto &&c:value) dec(c);

					try {
						datain.set(vkey, Value::fromString(buff));
					} catch (...) {
						datain.set(vkey, buff);
					}
				}
			}
			{
				auto headers = datain.object("_headers");
				for(auto &&c: req) {
					headers.set(c.first, c.second);
				}
			}
			cookies = datain;
	}
	return cookies[key];
}


void HttpRpcConnContext::exportToHeader(SendHeaders &hdrs) {
	if (this->data.size()) {
		std::ostringstream buff;
		bool sep = false;
		auto buffput = [&](char c){buff.put(c);};
		for (Value v : this->data) {
			if (sep) {buffput(',');buffput(' ');}
			StrViewA key = v.getKey();
			String value = v.stringify();
			UrlEncode<decltype(buffput) &> enc(buffput);
			for (auto &&c: key) enc(c);
			buffput('=');
			for (auto &&c: value) enc(c);
		}
		hdrs("Set-Cookie", buff.str());
	}
}

void RpcHandler::updateHeaders(bool cors, HTTPResponse &resp, const HeaderValue &origin, bool options)  {
	if (cors && origin.defined()) {
		resp("Access-Control-Allow-Origin", origin);
		if (options) {
			resp("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
			resp("Access-Control-Allow-Headers", "Content-Type");
			resp("Access-Control-Max-Age","86400");
		}
	}
	if (!options){
		resp.contentType("application/json");
	}
}

bool RpcHandler::operator ()(simpleServer::HTTPRequest req, const StrViewA &vpath) const {

	if (!req.allowMethods({"GET","POST","OPTIONS"})) return true;
	StrViewA method = req.getMethod();
	HeaderValue origin = req["Origin"];
	if (method == "OPTIONS") {
		HTTPResponse resp(204);
		updateHeaders(corsEnabled, resp, origin, true);
		req.sendResponse(resp, "");
		return true;
	}
	if (method == "POST") {
		RpcServer &srv(rpcserver);
		req.readBodyAsync(maxReqSize, [&srv, origin, cors = corsEnabled](HTTPRequest httpreq){
			auto x = httpreq.getUserBuffer();
			if (x.empty()) {

				RpcServerEnum &enm = static_cast<RpcServerEnum &>(srv);
				Array methods;
				enm.forEach([&methods](Value v){methods.push_back(v);});
				HTTPResponse resp(200);
				updateHeaders(cors, resp, origin, false);
				Stream out = httpreq.sendResponse(resp);
				Value(methods).serialize(out);
				out.flush();

			} else {
				Value rdata = Value::fromString(StrViewA(BinaryView(x)));
				Stream out;
				RefCntPtr<HttpRpcConnContext> ctx = new HttpRpcConnContext(httpreq,false);
				RpcRequest rrq = RpcRequest::create(rdata,[httpreq,cors, origin, logObj = LogObject(httpreq->log,"RPC"),out,ctx](
							const Value &v, const RpcRequest &req) mutable {

					if (out == nullptr) {
						HTTPResponse hdrs(200);
						ctx->exportToHeader(hdrs);
						updateHeaders(cors, hdrs, origin, false);
						out = httpreq.sendResponse(hdrs);
					}
					handleLogging(logObj,v,req);

					try {

						if (v.defined()) {
							v.serialize(out);
						}
						out << "\r\n";
						return out.flush();

					} catch (...) {
						return false;
					}
				}, RpcFlags::preResponseNotify, PRpcConnContext::staticCast(ctx));
				srv(rrq);
			}
		});
	} else if (vpath.empty()) {
		req.redirectToFolderRoot();
	} else {
		const Resource *selRes = nullptr;
		StrViewA fname;
		QueryParser qp(vpath);
		auto splt = qp.getPath().split("/");
		while (splt) fname = splt();
		if (fname.empty()) fname="index.html";

		if (fname == "index.html") selRes = consoleEnabled?&client_index_html:nullptr;
		else if (fname == "styles.css") selRes = consoleEnabled?&client_styles_css:nullptr;
		else if (fname == "rpc.js") selRes = &client_rpc_js;
		else if (fname == "methods") {
			StrViewA cb = qp["callback"];
			Value methods;
			{
				RpcServer &srv(rpcserver);
				RpcServerEnum &enm = static_cast<RpcServerEnum &>(srv);
				Array methodsArr;
				enm.forEach([&methodsArr](Value v){methodsArr.push_back(v);});
				methods = methodsArr;
			}
			if (cb.empty()) {
				HTTPResponse resp(200);
				updateHeaders(corsEnabled, resp, origin, false);
				auto stream = req.sendResponse(resp);
				methods.serialize(stream);
			} else {
				auto stream = req.sendResponse("text/javascript");
				stream << cb << "(";
				methods.serialize(stream);
				stream << ");\r\n";
			}
			return true;
		}

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
	h.enableCORS(cfg.enableCORS);
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



RpcHttpServer::DirectRpcConnContext::DirectRpcConnContext()
	:logObj(AbstractLogProvider::create(),Ident("TCP")) {}

/*
template<typename WrFn>
void logPrintValue(WrFn &wr, const HttpIdent &ident) {
	wr("http:");
	unsignedToString(ident.instanceId,wr,36,4);
}
*/


void RpcHttpServer::directRpcAsync(Stream s) {
	directRpcAsync2(s,new DirectRpcConnContext);
}
void RpcHttpServer::directRpcAsync2(simpleServer::Stream s, RefCntPtr<DirectRpcConnContext> ctx) {


	auto sendFn =[=](Value v, RpcRequest req) {
		try {
			if (v.defined()) v.serialize(s);
			s << "\n";
			s.flush();
			handleLogging(ctx->logObj,v,req);
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
			RpcRequest req = RpcRequest::create(jsonReq,sendFn, RpcFlags::notify, PRpcConnContext::staticCast(ctx));
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
	if (!hostMapping.empty()) {
		HostMappingHandler hostMap;
		hostMap.setMapping(hostMapping);
		HttpStaticPathMapperHandler stHandler(hndl);
		(*this)>>(hostMap>>HTTPMappedHandler(stHandler));
	} else {
		AutoHostMappingHandler hostMap;
		HttpStaticPathMapperHandler stHandler(hndl);
		(*this)>>(hostMap>>HTTPMappedHandler(stHandler));

	}
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
		if (wswc->ctx == nullptr) wswc->ctx = new HttpRpcConnContext(httpreq, true);
		connctx = PRpcConnContext::staticCast(wswc->ctx);


		RpcRequest rrq = RpcRequest::create(jreq,[wsstream,logObj = LogObject(httpreq->log, "RPC")](const Value &v, const RpcRequest &req){
			WebSocketStream ws(wsstream);
			if (!v.defined()) {
				return !(ws.isClosed());
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

void RpcHttpServer::addStats(String path, std::function<json::Value()> customStats) {
	addPath(path, [cntr = this->counters, customStats = std::move(customStats)](simpleServer::HTTPRequest req, StrViewA ){

		auto data = cntr->getCounters();
		json::Value out = json::Object
				("short_requests", data.requests)
				("short_time", data.reqtime*0.1)
				("short_time_sqr", data.reqtime2*0.01)
				("long_requests", data.long_requests)
				("long_time", data.long_reqtime*0.1)
				("long_time_sqr", data.long_reqtime2*0.01)
				("very_long_requests", data.very_long_requests)
				("very_long_time", data.very_long_reqtime*0.1)
				("very_long_time_sqr", data.very_long_reqtime2*0.01)
				("total_requests", (data.very_long_requests+data.long_requests+data.requests))
				("total_time", (data.very_long_reqtime+data.long_reqtime+data.reqtime)*0.1)
				("total_time_sqr", (data.very_long_reqtime2+data.long_reqtime2+data.reqtime2)*0.01);

		if (customStats) {
			auto custom = customStats();
			out = out.merge(custom);
		}

		auto s = req.sendResponse("application/json");
		out.serialize(std::move(s));
		return true;
	});
}

} /* namespace hflib */

