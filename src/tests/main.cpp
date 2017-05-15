/*
 * main.cpp
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include <unistd.h>

#include "../simpleServer/TCPListener.h"
#include "../simpleServer/mt.h"
#include "testClass.h"
#include "../simpleServer/prioqueue.h"
#include <condition_variable>
#include <mutex>

using namespace simpleServer;


void platformTests(TestSimple &tst);

int main(int argc, char *argv[]) {

	TestSimple tst;

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

	tst.test("Listener.openRandomPort.localhost","ok") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost,port);
		if (port) out << "ok";
	};
	tst.test("Listener.openRandomPort.network","ok") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(network,port);
		if (port) out << "ok";
	};
	tst.test("Listener.openRandomPort.localhost6","ok") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost6,port);
		if (port) out << "ok";
	};
	tst.test("Listener.openRandomPort.network6","ok") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(network6,port);
		if (port) out << "ok";
	};
	tst.test("Listener.acceptConn","ok") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost,port);
		runThreads(1,[port] {
			Connection con = Connection::connect(NetAddr::create("0",port,NetAddr::IPv4));
		});
		Connection con2 = *server.begin();
		if (con2.getHandle() != nullptr) out << "ok";
	};

	tst.test("Listener.receiveMsg","test message") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost,port);
		runThreads(1,[port] {
			Connection con = Connection::connect(NetAddr::create("0",port,NetAddr::IPv4));
			StrViewA msg("test message");
			con(BinaryView(msg));
			con.closeOutput();
		});
		Connection con2 = *server.begin();
		BinaryView data = con2(0);
		while (!data.empty()) {
			out << StrViewA(data);
			data = con2(data.length+1);
		}
	};
	tst.test("Listener.receiveMsg2","test message") >> [](std::ostream &out) {
		unsigned int port = 0;
		TCPListener server(localhost,port);
		runThreads(1,[port] {
			Connection con = Connection::connect(NetAddr::create("0",port,NetAddr::IPv4));
			StrViewA msg("test message");
			con(BinaryView(msg));
			con.closeOutput();
		});
		Connection con2 = *server.begin();
		BinaryView data = con2(0);
		while (!data.empty()) {
			out.put(data[0]);
			data = con2(1);
			if (data.empty()) data = con2(0);
		}
	};
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
	};

	return tst.didFail()?1:0;

}
