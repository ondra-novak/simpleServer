#pragma once

#include <memory>
#include <imtjson/rpc.h>
#include "../simpleServer/http_client.h"
#include "../simpleServer/websockets_stream.h"

namespace simpleServer {

class HttpResponse;


///JSONRPC client using HTTP protocol
/**
 * It can be used to send single JSONRPC command to the server, while
 * server is able to send a response. This client also supports
 * pre-response and post-response notifications, but the notification
 * must be send along with the response as single HTTP response.
 *
 * Synchronous requests can throw HttpStatusException, if the
 * server responds by different status then 200. Asynchronous
 * requests are replied with empty error object. However they
 * are called in catch handler, so you can capture the
 * exception in the callback function.
 *
 */
class HttpJsonRpcClient: public json::AbstractRpcClient {
public:
	typedef std::shared_ptr<HttpClient> PClient;

	///Construct RPC client
	/**
	 * @param client shared pointer to HttpClient
	 * @param url url to RPC server
	 * @param async set true to perform asynchronous operations.
	 *    set false to perform synchronous operations.
	 * @param vesion JSONRPC version
	 *
	 * Asynchronous operations are not possible when source client
	 * is no AsyncProvider defined.
	 */
	HttpJsonRpcClient(PClient client, std::string url,
			bool async = false,
			json::RpcVersion::Type version = json::RpcVersion::ver2);


	virtual void sendRequest(json::Value request);

	///Headers template
	SendHeaders headers;

	virtual void onNotify(const json::Notify &) {}
	virtual void onUnexpected(const json::Value &) {}

protected:
	PClient client;
	std::string url;
	bool async;
	std::vector<unsigned char> buffer;

	void parseResponse(HttpResponse &&resp);


};



class WebSocketJsonRpcClient: public json::AbstractRpcClient {
public:

	WebSocketJsonRpcClient(WebSocketStream wsstream, json::RpcVersion::Type version = json::RpcVersion::ver2);

	virtual void sendRequest(json::Value request);

	///Called when notification is received
	virtual void onNotify(const json::Notify &) {}
	virtual void onUnexpected(const json::Value &) {}
	///Override this function to handle server's requests
	/**
	 * On bidirectional connection server and client can swap their roles
	 * Server can send request to the client while it acting as
	 * client and the client is acting as server. Once
	 * the request is received, it is parsed and this function is called.
	 * Function expects, that the callback (specified as the second argument)
	 * will be called with response. If the client ignores the request
	 * server may block waiting on response. Default implementation
	 * responds with error "Method not found"
	 */
	virtual void onRequest(const json::Value &request, std::function<void(json::Value)> response);

	///Parse single response
	/** Parses single response and returns. Function blocks
	 * @retval true processed response
	 * @retval false stream closed
	 */
	bool parseResponse();
	///Parses responses in cycle until the end of stream is reached
	void parseAllResponses();
	///Parses single response asynchronously. Function will not block
	/**
	 * @param compFn function called after frame is parsed
	 */
	void parseResponseAsync(IAsyncProvider::CompletionFn compFn);
	///Parses all responses asynchronously. Function will not block
	/**
	 * @param compFn function is called when stream is closed
	 *
	 * @note function handles timeout by sending ping to the server.
	 * If the pong is not received within another timeout period,
	 * the timeout error is exposed to the callback function
	 *
	 **/
	void parseAllResponsesAsync(IAsyncProvider::CompletionFn compFn);
	///Parses all responses asynchronously. Function will not block
	void parseAllResponsesAsync();

	const WebSocketStream &getStream() const {
		return wsstream;
	}

protected:
	WebSocketStream wsstream;
	std::vector<char> buffer;

	///parses current frame
	/**
	 * Override this function to allow custom processing of the frames
	 * Call the original function to parse the frame as the JSONRPC
	 * message
	 */
	virtual void parseFrame();
};

class StreamJsonRpcClient: public json::AbstractRpcClient {
public:

	StreamJsonRpcClient(Stream stream, json::RpcVersion::Type version = json::RpcVersion::ver2);

	virtual void sendRequest(json::Value request);

	///Called when notification is received
	virtual void onNotify(const json::Notify &) {}
	virtual void onUnexpected(const json::Value &) {}
	///Override this function to handle server's requests
	/**
	 * On bidirectional connection server and client can swap their roles
	 * Server can send request to the client while it acting as
	 * client and the client is acting as server. Once
	 * the request is received, it is parsed and this function is called.
	 * Function expects, that the callback (specified as the second argument)
	 * will be called with response. If the client ignores the request
	 * server may block waiting on response. Default implementation
	 * responds with error "Method not found"
	 */
	virtual void onRequest(const json::Value &request, std::function<void(json::Value)> response);

	///Parse single response
	/** Parses single response and returns. Function blocks
	 * @retval true processed response
	 * @retval false stream closed
	 */
	bool parseResponse();
	///Parses responses in cycle until the end of stream is reached
	void parseAllResponses();
	///Parses single response asynchronously. Function will not block
	/**
	 * @param compFn function called after frame is parsed
	 */
	void parseResponseAsync(IAsyncProvider::CompletionFn compFn);
	///Parses all responses asynchronously. Function will not block
	/**
	 * @param compFn function is called when stream is closed
	 *
	 * @note function handles timeout by sending ping to the server.
	 * If the pong is not received within another timeout period,
	 * the timeout error is exposed to the callback function
	 *
	 **/
	void parseAllResponsesAsync(IAsyncProvider::CompletionFn compFn);
	///Parses all responses asynchronously. Function will not block
	void parseAllResponsesAsync();

protected:
	Stream stream;
	std::vector<char> buffer;
	virtual void parseFrame(json::Value j);

};


}
