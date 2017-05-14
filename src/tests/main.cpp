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

using namespace simpleServer;

void readData(Connection conn) {

		std::string buffer;
		BinaryView b = conn(0);
		while(!b.empty()) {
			buffer.append(reinterpret_cast<const char *>(b.data),b.length);
			conn(b.length);
			b = conn(0);
		}
		std::cout << buffer << std::endl;
}

void writeData(Connection conn) {
	StrViewA s("secret message");
	conn(BinaryView(s));
	conn.closeOutput();

}

void basicTest() {

	unsigned int port;
	TCPListener listener(localhost, port);
	forkThread([&] {
		for (Connection conn: listener) {
			readData(conn);
		}
	},[&] {

		Connection conn = Connection::connect(NetAddr::create("[::1]",port));
		writeData(conn);
		sleep(1);
		listener.stop();
	});


}


void runEchoThread(Connection conn) {
	BinaryView b = conn(0);
	while (!b.empty()) {
		conn(b);
		conn(b.length);
		b = conn(0);
	}
}


void echoTest() {

	unsigned int port;
	TCPListener listener(localhost, port);
	std::cout << "Echo is running on port: " << port << std::endl;
	runThreads(10,[&] {
		for (Connection conn: listener) {
			runEchoThread(conn);
		}
	},[&] {

		std::cout << "Press enter to stop server....";
		std::cin.get();
		listener.stop();
	});

}


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



}
