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
#include "../simpleServer/common/mtcounter.h"
#include "../simpleServer/http_parser.h"
#include "../simpleServer/http_server.h"

using namespace simpleServer;


void platformTests(TestSimple &tst);


class AsyncReader {
public:

	AsyncReader(Stream sx, std::ostream &out, MTCounter &event):sx(sx),out(out),event(event) {}
	void operator()(AsyncState, const BinaryView &data) const {
		if (data.empty()) event.dec();
		else {
			out << StrViewA(data);
			sx.readASync(*this);
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

		if (req->getPath() == "/") {
			Stream out = req->sendResponse("text/plain");
			for (int i = 0; i < 10000; i++)
				out << "Hello world! ";
			out << "... Konec";
		} else {
			req->sendErrorPage(404);
		}


	};

	wait.zeroWait();


}

int main(int argc, char *argv[]) {

	TestSimple tst;

	runServerTest();


	tst.test("queue.removeAt","98,56,54,44,30,23,22,21,20,20,11,10,1 - 98,54,44,30,22,21,20,20,11,10") >> [](std::ostream &out) {

		int items[] = {10,30,20,1,23,44,21,22,56,20,11,54,98};
		PrioQueue<int> r,t;
		for (auto v:items) r.push(v);

		auto printq = [&](PrioQueue<int> r){
			out << r.top();
			r.pop();
			while (!r.empty()) {
				out << "," << r.top();
				r.pop();
			}
		};

		printq(r);
		r.remove_at(3);
		r.remove_at(2);
		r.remove_at(7);
		out << " - ";
		printq(r);
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
		runThreads(1,[srvAddr] {
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			Stream con = tcpConnect(srvAddr,30000);
		});
		Stream con2 = *server.begin();
		if (con2 != nullptr) out << "ok";
	};

	tst.test("Listener.receiveMsg","test message") >> [](std::ostream &out) {
		StreamFactory server = TCPListen::create(true,0);
		NetAddr srvAddr = TCPStreamFactory::getLocalAddress(server);
		runThreads(1,[srvAddr] {
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
				s.readASync(AsyncReader(s,out,event));
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
