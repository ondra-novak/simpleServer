#include <imtjson/parser.h>
#include <imtjson/serializer.h>
#include "rpcClient.h"


namespace simpleServer {

using namespace json;

using ondra_shared::BinaryView;

HttpJsonRpcClient::HttpJsonRpcClient(
		PClient client, std::string url, bool async,json::RpcVersion version)
	:AbstractRpcClient(version)
	,client(std::move(client))
	,url(std::move(url))
	,async(async) {}

void HttpJsonRpcClient::sendRequest(json::Value request) {

	buffer.clear();
	request.serialize([&](char c){buffer.push_back((unsigned char)c);});
	BinaryView data(buffer.data(),buffer.size());


	headers("Accept","application/json");
	headers("Content-Type","application/json");

	if (async) {
		client->request_async("POST",url,std::move(headers),data,
			[=](AsyncState st, HttpResponse && resp) {
				if (st == asyncOK) {
					parseResponse(std::move(resp));
				} else {
					cancelAllPendingCalls(RpcResult(nullptr, true, nullptr));
				}
		});
	} else {
		HttpResponse resp(client->request("POST",url,std::move(headers),data));
		if (resp.getStatus() == 200) {
			parseResponse(std::move(resp));
		} else {
			throw HTTPStatusException(resp.getStatus(), resp.getMessage());
		}
	}
}

void HttpJsonRpcClient::parseResponse(HttpResponse&& resp) {
	try {
		if (resp.getStatus() == 200) {
			Stream b = resp.getBody();
			BinaryView data = b.read(false);
			while (!data.empty()) {
				if (isspace(data[0])) {
					b.putBack(data.substr(1));
				} else {
					b.putBack(data);
					Value r = Value::parse(resp.getBody());
					auto type = processResponse(r);
					switch (type) {
					case AbstractRpcClient::success:
						break;
					case AbstractRpcClient::request:
					case AbstractRpcClient::notification:
						onNotify(RpcNotify(r));
						break;
					case AbstractRpcClient::unexpected:
						onUnexpected(r);
						break;
					}
				}
				data = b.read(false);
			}
		} else {
			throw HTTPStatusException(resp.getStatus(), resp.getMessage());
		}
	} catch (...) {
		cancelAllPendingCalls(RpcResult(nullptr,true,nullptr));
	}
}


WebSocketJsonRpcClient::WebSocketJsonRpcClient(
		WebSocketStream wsstream, json::RpcVersion version)
	:AbstractRpcClient(version),wsstream(wsstream)
{
}

void WebSocketJsonRpcClient::sendRequest(json::Value request) {
	buffer.clear();
	request.serialize([&](char c){
		buffer.push_back(c);
	});
	StrViewA text(buffer.data(),buffer.size());
	wsstream.postText(text);
}

static void requestErrorResponse(const json::Value& request,
		std::function<void(json::Value)> response) {


	Object error;
	error("jsonrpc", request["jsonrpc"])
		 ("id",request["id"])
		 ("error",Object("code",-32601)
				       ("message","Method not foud"));
	response(error);

}

void WebSocketJsonRpcClient::onRequest(const json::Value& request,
		std::function<void(json::Value)> response) {
	requestErrorResponse(request, response);

}

void WebSocketJsonRpcClient::parseFrame() {
	if (wsstream.getFrameType() == WSFrameType::text) {
		Value j(Value::fromString(wsstream.getText()));
		switch(processResponse(j)) {
		case AbstractRpcClient::success: break;
		case AbstractRpcClient::notification: onNotify(RpcNotify(j));break;
		case AbstractRpcClient::request: onRequest(j,
				[=](Value v) {
					wsstream.postText(v.toString());
				});break;
		case AbstractRpcClient::unexpected: onUnexpected(j);break;
		}
	}

}

bool WebSocketJsonRpcClient::parseResponse() {
	if (!wsstream.readFrame()) return false;
	parseFrame();
	return true;
}

void WebSocketJsonRpcClient::parseAllResponses() {
	while (parseResponse()) {
	}
}

void WebSocketJsonRpcClient::parseResponseAsync(
		IAsyncProvider::CompletionFn compFn) {

	wsstream.readAsync([=](AsyncState st){
		if (st == asyncOK) {
			parseFrame();
		} else if (st == asyncTimeout) {
			wsstream.ping(BinaryView());
			wsstream.readAsync([=](AsyncState st){
				if (st == asyncOK) parseFrame();
				compFn(st);

			});
		} else {
			compFn(st);
		}
	});

}

void WebSocketJsonRpcClient::parseAllResponsesAsync(
		IAsyncProvider::CompletionFn compFn) {
	wsstream.readAsync([=](AsyncState st){
		if (st == asyncOK) {
			parseFrame();
			parseAllResponsesAsync(compFn);
		} else if (st == asyncTimeout) {
			wsstream.ping(BinaryView());
			wsstream.readAsync([=](AsyncState st){
				if (st == asyncOK) {
					parseFrame();
					parseAllResponsesAsync(compFn);
				} else{
					compFn(st);
				}
			});
		} else {
			compFn(st);
		}
	});
}

void WebSocketJsonRpcClient::parseAllResponsesAsync() {
	parseAllResponsesAsync([](AsyncState){});
}

StreamJsonRpcClient::StreamJsonRpcClient(Stream stream, json::RpcVersion version)
	:AbstractRpcClient(version),stream(stream)
{
}

void StreamJsonRpcClient::sendRequest(json::Value request) {
	request.serialize(stream);
	stream << "\n";
	stream.flush();
}

void StreamJsonRpcClient::onRequest(const json::Value& request,
		std::function<void(json::Value)> response) {
	requestErrorResponse(request, response);
}

bool StreamJsonRpcClient::parseResponse() {
	BinaryView b = stream.read(false);
	while (!b.empty() && isspace(b[0])) b = b.substr(1);
	if (b.empty()) return false;
	stream.putBack(b);
	Value r = Value::parse(stream);
	parseFrame(r);
	return true;
}

void StreamJsonRpcClient::parseAllResponses() {
	while (parseResponse()) {}
}

void StreamJsonRpcClient::parseResponseAsync(IAsyncProvider::CompletionFn compFn) {
	stream.readAsync([=](AsyncState st, BinaryView b) {
		if (st == asyncOK) {
			stream.putBack(b);
			parseResponse();
		}
		compFn(st);
	});
}

void StreamJsonRpcClient::parseAllResponsesAsync(
		IAsyncProvider::CompletionFn &&compFn, std::string &&ping_cmd) {
	stream.readAsync([
		  compFn = std::move(compFn),
		  ping_cmd=std::move(ping_cmd),
		  this
	   ](AsyncState st, BinaryView b) mutable {
		if (st == asyncOK) {
			stream.putBack(b);
			parseResponse();
			parseAllResponsesAsync(std::move(compFn), std::move(ping_cmd));
		} else if (st == asyncTimeout) {
			this->operator ()(ping_cmd, json::array) >> [](RpcResult) {};
			stream.readAsync([
					  compFn = std::move(compFn),
					  ping_cmd=std::move(ping_cmd),
					  this
				](AsyncState st, BinaryView b)mutable{
					if (st == asyncOK) {
						stream.putBack(b);
						parseResponse();
						parseAllResponsesAsync(std::move(compFn), std::move(ping_cmd));
					} else{
						compFn(st);
					}
				});
		} else {
			compFn(st);
		}
	});
}

void StreamJsonRpcClient::parseAllResponsesAsync() {
	parseAllResponsesAsync([](AsyncState){});
}

void StreamJsonRpcClient::parseFrame(json::Value j) {
	switch(processResponse(j)) {
	case AbstractRpcClient::success: break;
	case AbstractRpcClient::notification: onNotify(RpcNotify(j));break;
	case AbstractRpcClient::request: onRequest(j,
			[=](Value v) {
				Sync _(lock);
				sendRequest(v);
			});break;
	case AbstractRpcClient::unexpected: onUnexpected(j);break;
	}
}


void WebSocketJsonRpcClient::setStream(WebSocketStream wsstream,
		const RpcResult& cancelResult) {
	Sync _(lock);
	cancelAllPendingCalls(cancelResult);
	this->wsstream = wsstream;
}

void StreamJsonRpcClient::setStream(Stream stream,
		const RpcResult& cancelResult) {
	Sync _(lock);
	cancelAllPendingCalls(cancelResult);
	this->stream = stream;
}


}
