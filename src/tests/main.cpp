/*
 * main.cpp
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include <unistd.h>

#include "../simpleServer/mt.h"
#include "testClass.h"
#include "../simpleServer/prioqueue.h"
#include <condition_variable>
#include <mutex>
#include "../simpleServer/tcp.h"
#include "../simpleServer/threadPoolAsync.h"
#include "../simpleServer/mtcounter.h"
#include "../simpleServer/http_parser.h"
#include "../simpleServer/http_server.h"

#include "../simpleServer/http_client.h"
#include "../simpleServer/linux/ssl_exceptions.h"
#include "../simpleServer/shared/mtcounter.h"
#include "../simpleServer/websockets_stream.h"



using namespace simpleServer;


void platformTests(TestSimple &tst);


class AsyncReader {
public:

	AsyncReader(Stream sx, std::ostream &out, MTCounter &event):sx(sx),out(out),event(event) {}
	void operator()(AsyncState, const BinaryView &data) const {
		if (data.empty()) event.dec();
		else {
			out << StrViewA(data);
			sx.readAsync(*this);
		}
	}

protected:
	mutable Stream sx;
	std::ostream &out;
	MTCounter &event;

};

void runServerTest() {


	MTCounter wait;
	wait.inc();
	MiniHttpServer mserver(NetAddr::create("",8787),0,0);
	mserver >> [](HTTPRequest req) {

		if (req->getMethod() == "POST") {

			req->readBodyAsync(1024*1024,[](HTTPRequest req){

				Stream out = req->sendResponse(HTTPResponse(200)
						.contentType("text/plain")
						.contentLength(req->userBuffer.size()));
				out->write(BinaryView(req->userBuffer));

			});
		} else {

			if (req->getPath() == "/") {
				Stream out = req->sendResponse("text/plain");
				for (int i = 0; i < 10000; i++)
					out << "Hello world! ";
				out << "... Konec";
			} else if (req->getPath() == "/204") {
				req->sendResponse("text/plain","empty",204);
			} else {
				req->sendFile("text/plain",req->getPath().substr(1));
			}
		}

	};

	wait.zeroWait();


}



int main(int argc, char *argv[]) {

	TestSimple tst;

//	runServerTest();


	tst.test("HttpClient.GET","Y") >>[](std::ostream &out) {
		HttpClient client;
		HttpResponse resp = client.request("GET","http://httpbin.org/get",std::move(SendHeaders()("Accept","application/json")));
		if (resp.getStatus() == 200) {
			Stream b = resp.getBody();
			std::string content = b.toString();
			if (StrViewA(content).indexOf(R"("url": "http://httpbin.org/get")") != StrViewA::npos) {
				out.put('Y');
			} else {
				out << content;
			}
		} else {
			out << "Status:" << resp.getStatus();
		}
	};

	tst.test("HttpClient.https.GET","Y") >>[](std::ostream &out) {
		HttpClient client;
		client.setHttpsProvider(newHttpsProvider());
		HttpResponse resp = client.request("GET","https://httpbin.org/get",std::move(SendHeaders()("Accept","application/json")));
		if (resp.getStatus() == 200) {
			Stream b = resp.getBody();
			std::string content = b.toString();
			if (StrViewA(content).indexOf(R"("url": "https://httpbin.org/get")") != StrViewA::npos) {
				out.put('Y');
			} else {
				out << content;
			}
		} else {
			out << "Status:" << resp.getStatus();
		}
	};

	tst.test("HttpClient.https.untrusted","Error in certificate '/C=US/ST=California/L=Walnut Creek/O=Lucas Garron/CN=*.badssl.com': Host mismatch (code: 62)") >>[](std::ostream &out) {
		HttpClient client;
		client.setHttpsProvider(newHttpsProvider());
		try {
			HttpResponse resp = client.request("GET","https://wrong.host.badssl.com/",SendHeaders());
			out << "Status:" << resp.getStatus();
		} catch (const SSLCertError &e) {
			out << e.what();
		}
	};

	tst.test("HttpClient.initWebsocket","0") >>[](std::ostream &out) {
		HttpClient client;
		client.setHttpsProvider(newHttpsProvider());
		client.setAsyncProvider(ThreadPoolAsync::create());
		AsyncState lastst;
		std::exception_ptr e;
		ondra_shared::MTCounter mtc(1);
		connectWebSocketAsync(client,"wss://echo.websocket.org/",SendHeaders(),[&](AsyncState st, WebSocketStream) {
			e = std::current_exception();
			lastst = st;
			mtc.dec();
		});
		mtc.wait();
		if (e != nullptr) std::rethrow_exception(e);
		out << (int)lastst;
	};
	tst.test("HttpClient.websocket.echo","This is test message") >>[](std::ostream &out) {

		HttpClient client;
		client.setHttpsProvider(newHttpsProvider());
		WebSocketStream ws = connectWebSocket(client,"wss://echo.websocket.org/", SendHeaders());
		ws.postText("This is test message");
		ws.readFrame();
		if (ws.getFrameType() == WSFrameType::text) {
			out << ws.getText();
			ws.close();
			ws.readFrame();
		}
	};


	tst.test("Listener.openRandomPort.localhost","127.0.0.1") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true,0);
		NetAddr localAddr = TCPStreamFactory::getLocalAddress(server);
		std::string addr =  localAddr.toString(false);
		auto splt = addr.find(':');
		out << addr.substr(0,splt);
	};
	tst.test("Listener.openRandomPort.network","0.0.0.0") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(false,0);
		NetAddr localAddr = TCPStreamFactory::getLocalAddress(server);
		std::string addr =  localAddr.toString(false);
		auto splt = addr.find(':');
		out << addr.substr(0,splt);
	};
	tst.test("Listener.acceptConn","ok") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true,0);
		NetAddr srvAddr = TCPStreamFactory::getLocalAddress(server);
		runThread([srvAddr] {
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			Stream con = tcpConnect(srvAddr,30000);
		});
		Stream con2 = *server.begin();
		if (con2 != nullptr) out << "ok";
	};

	tst.test("Listener.receiveMsg","test message") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true,0);
		NetAddr srvAddr = TCPStreamFactory::getLocalAddress(server);
		runThread([srvAddr] {
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			Stream con = tcpConnect(srvAddr,30000);
			StrViewA msg("test message");
			con.write(BinaryView(msg),writeWholeBuffer);
			con.writeEof();
		});
		Stream con2 = server();
		BinaryView data = con2.read(false);
		while (!data.empty()) {
			out << StrViewA(data);
			data = con2.read();
		}
	};

	tst.test("Listener.async.receiveMsg","test message") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true,0);
		NetAddr srvAddr = TCPStreamFactory::getLocalAddress(server);
		AsyncProvider async = ThreadPoolAsync::create();
		MTCounter event(1);

		server(async, [&](AsyncState, Stream s){
			if (s != nullptr) {
				s.readAsync(AsyncReader(s,out,event));
			}
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		Stream s = tcpConnect(srvAddr,30000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		StrViewA msg("test message");
		s.write(BinaryView(msg));
		s.closeOutput();
		event.zeroWait();
		async.stop();
	};

/*
	tst.test("Http.server1","") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true, 8901);
		NetAddr srvAddr = TCPStreamFactory::getLocalAddress(server);
		AsyncProvider async = ThreadPoolAsync::create();




	}
*/
	/*
	tst.test("Listener.async.receiveMsg","test message") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost,port);
		AsyncDispatcher d = AsyncDispatcher::start();
		server.asyncAccept(d,[d,&out](AsyncState state, const Connection *con){
			if (state == asyncOK) {
				Connection c = *con;
				c.asyncRead(d,[&out,c](AsyncState state, BinaryView d) {
					BinaryView data(d);
					while (!data.empty()) {
						out << StrViewA(data);
						data = c(data.length+1);
					}
					c(data);
				});
			}
		});

		Connection c2 = Connection::connect(NetAddr::create("0",port,NetAddr::IPv4));
		StrViewA msg("test message");
		c2(BinaryView(msg));
		c2.closeOutput();
		c2(0);
	};
	tst.test("Async.timeout","44") >> [](std::ostream &out) {

		std::vector<TCPListener> listeners;
		unsigned int port;
		unsigned int counter1 = 0, counter2=0;;
		for (int i = 0; i < 4; i++) listeners.push_back(TCPListener(localhost,port));
		AsyncDispatcher d = AsyncDispatcher::start();
		std::mutex mx;
		std::condition_variable cv;
		for(auto l :listeners) {
			l.asyncAccept(d,[&](AsyncState st, const Connection *){
				std::lock_guard<std::mutex> _(mx);
				if (st == asyncTimeout) {
					counter1++;
				}
				counter2++;
				cv.notify_one();
			}, rand()%1000+1000);
		}
		std::unique_lock<std::mutex> _(mx);
		cv.wait(_,[&]{return counter2 >= 4;});
		out << counter1 << counter2;
	};*/

	return tst.didFail()?1:0;

}
