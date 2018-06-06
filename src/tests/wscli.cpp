/*
 * wscli.cpp
 *
 *  Created on: Apr 12, 2018
 *      Author: ondra
 */

#include <thread>
#include <iostream>
#include "../simpleServer/websockets_stream.h"
#include "../simpleServer/http_client.h"



using namespace simpleServer;
using ondra_shared::StrViewA;

void showHelp(char *arg0) {
	std::cerr << "Usage:" << std::endl
			<< std::endl
			<< arg0 << " <switches> <url> "<<std::endl
			<< std::endl
			<< "-H  <key:value> "			<< std::endl
			<< "--header <key:value> specify custom header"<< std::endl
			<< std::endl
			<< "--separator          defines a sequence of characters which are used to separate" <<  std::endl
			<< "                     each message. Every message must end with the separator to" <<  std::endl
			<< "                     send the message to the other side. The separator is also used" <<  std::endl
			<< "                     to separate incoming messages. Default value is \\n" <<  std::endl
			<< std::endl
			<< "--hex_separator      allows to define separator as sequence of hex numbers" <<  std::endl
			<< std::endl
			<< "--user_agent         set user agent" <<  std::endl
			<< "--proxy              use proxy" <<  std::endl
			<< std::endl
			<< "url                  websocket url, starting with ws:// or wss://" <<  std::endl
			<< "                     (http:// or https:// is also accepted)" <<  std::endl
			<< std::endl
			<< "--help               this help page." <<  std::endl;

}

class HexReader {
public:
	HexReader(StrViewA txt):txt(txt) {}
	class Iterator {
	public:
		Iterator(StrViewA txt, std::uintptr_t pos):txt(txt),pos(pos) {}
		static unsigned char hexToNum(char c) {
			return c >='0' && c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:0;
		}
		unsigned char operator*() const {
			StrViewA t = txt.substr(pos*2,2);
			unsigned char out;
			out = (hexToNum(t[0]) << 4) | hexToNum(t[1]);
			return out;
		}
		Iterator &operator++() {++pos;return *this;}
		Iterator &operator--()  {--pos;return *this;}
		bool operator==(const Iterator &other) {return txt.data == other.txt.data && pos == other.pos;}
		bool operator!=(const Iterator &other) {return !operator ==(other);}
	protected:
		StrViewA txt;
		std::uintptr_t pos;
	};
	Iterator begin() const {return Iterator(txt,0);}
	Iterator end() const {return Iterator(txt,txt.length/2);}
protected:
	StrViewA txt;
};

std::string parseHex(StrViewA data) {
	std::string out;
	for (unsigned char c: HexReader(data)) {
		out.push_back((char)c);
	}
	return out;
}

static bool readmsg(std::vector<char> &buff, StrViewA separator) {
	int i = std::cin.get();
	buff.clear();
	std::size_t l = 0;
	char lastchar = separator[separator.length-1];
	bool done = false;
	while (i != -1) {
		char c = (char)i;
		buff.push_back(c);
		++l;
		if (c == lastchar && StrViewA(buff).substr(l-separator.length) == separator) {
			buff.resize(l - separator.length);
			return true;

		}
		i = std::cin.get();
	}
	return false;
}

int main(int argc, char **argv) {

	try {
		std::string msgseparator("\n");
		std::string url;
		std::string proxy;
		std::string userAgent ("wscli (https://www.github.com/ondra-novak/simpleServer)");
		SendHeaders hdrs;

		for (int i = 1; i < argc; i++) {
			StrViewA a(argv[i]);
			if (a == "--separator") {
				if (++i < argc) msgseparator = argv[i];
			} else if (a == "--hex_separator") {
				if (++i < argc) msgseparator = parseHex(argv[i]);
			} else if (a == "--user_agent" || a == "-a") {
				if (++i < argc) userAgent = argv[i];
			} else if (a == "--proxy" || a == "-p") {
				if (++i < argc) proxy = argv[i];
			} else if (a == "-H" || a == "--header") {
				if (++i < argc) {
					std::string hdr = argv[i];
					auto sep = hdr.find(":");
					if (sep != hdr.npos) {
						StrViewA key(hdr.data(), sep);
						StrViewA value(hdr.data()+ sep+1, hdr.size()-sep-1);
						key = key.trim(isspace);
						value = value.trim(isspace);
						hdrs(key,value);
					}
				}

			} else if (a == "-h" || a == "--help" || a == "-?" || a == "/?") {
				showHelp(argv[0]);
				return 1;
			} else if (a[0] == '-') {
				throw std::runtime_error(std::string("Unknown switch: ").append(a.data,a.length));
			} else {
				url = a;
			}
		}


		if (url.empty()) {
			showHelp(argv[0]);
			return 1;
		}

		if (msgseparator.empty())
			throw std::runtime_error("Separator cannot be empty");


		IHttpProxyProvider *proxyp = nullptr;
		if (!proxy.empty()) proxyp = newBasicProxyProvider(proxy, false,false);

		HttpClient client(userAgent,newHttpsProvider(), proxyp);
		WebSocketStream ws = connectWebSocket(client, url, std::move(hdrs));

		std::cout << msgseparator;
		std::cout.flush();
		std::thread reader([&]{
			bool cont = true;
			while (cont && ws.readFrame()) {
				switch (ws.getFrameType()) {
				case WSFrameType::binary: break;
				case WSFrameType::connClose: cont = false;break;
				case WSFrameType::ping:break;
				case WSFrameType::text: std::cout << ws.getText() << msgseparator;
										std::cout.flush();
										break;
				}
			}
			fclose(stdout);
		});

		std::vector<char> buff;
		while (readmsg(buff, msgseparator)) {
			ws.postText(buff);

		}

		ws.close();

		reader.join();



	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 2;
	}

}
