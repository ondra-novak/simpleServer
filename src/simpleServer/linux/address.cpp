#include <cstring>
#include <netdb.h>
#include "../address.h"

#include "../exceptions.h"


namespace simpleServer {

class AddressAddrInfo: public INetworkAddress {
public:
	AddressAddrInfo(struct addrinfo *addr):addr(addr) {}

	virtual std::string toString(bool resolve = false) const;
	virtual BinaryView toSockAddr() const;
	~AddressAddrInfo() {
		freeaddrinfo(addr);
	}


protected:

	struct addrinfo *addr;

};


class AddressSockAddr: public INetworkAddress {
public:

	AddressSockAddr(const struct sockaddr *sa, int len):len(len) {
		std::memcpy(&this->sa, sa, len);
	}

	void *operator new(std::size_t sz, const int &salen) {
		std::size_t totalsz = sizeof(AddressAddrInfo)+salen;
		return ::operator new(totalsz);
	}

	void operator delete(void *ptr, const int &salen) {
		::operator delete(ptr);
	}

	void operator delete(void *ptr, std::size_t sz) {
		::operator delete(ptr);
	}

	virtual std::string toString(bool resolve) const;
	virtual BinaryView toSockAddr() const;


protected:
	sockaddr sa;
	int len;
};


std::string AddressAddrInfo::toString(bool resolve) const {
	return addr->ai_canonname;
}

BinaryView AddressAddrInfo::toSockAddr() const {
	return BinaryView(reinterpret_cast<const unsigned char *>(addr->ai_addr), addr->ai_addrlen);
}


std::string NetAddr::toString(bool resolve) const {
	return addr->toString(resolve);
}

BinaryView NetAddr::toSockAddr() const {
	return addr->toSockAddr();
}

static char *copyString(void *abuff, StrViewA str) {
	char *buff = reinterpret_cast<char *>(abuff);
	std::memcpy(buff, str.data, str.length);
	buff[str.length] = 0;
	return buff;
}

static char *copyNumberToStr2(char *buff, unsigned int number) {
	if (number) {
		char *c = copyNumberToStr2(buff, number/10);
		*c = number % 10 + '0';
		return c+1;
	} else {
		return buff;
	}
}

static char *copyNumberToStr(void *abuff, unsigned int number) {
	char *buff = reinterpret_cast<char *>(abuff);
	if (number == 0) strcpy(buff,"0");
	else *copyNumberToStr2(buff, number) = 0;
	return buff;
}

class GaiError: public SystemException {
public:
	GaiError(int e):SystemException(e) {}
	std::string getMessage() const {
		return gai_strerror(this->err);
	}
};


NetAddr NetAddr::create(StrViewA addr, unsigned int defaultPort, AddressType type) {

	struct addrinfo req, *result;
	std::memset(&req,0,sizeof(req));
	req.ai_socktype = SOCK_STREAM;

	StrViewA prepNode;
	StrViewA prepPort;

	const char *node = 0;
	const char *svc = 0;


	switch (type) {
		default:
		case IPvAll: 	req.ai_family = AF_UNSPEC;break;
		case IPv4: 		req.ai_family = AF_INET;break;
		case IPv6: 		req.ai_family = AF_INET6;break;
	}


	if (addr.empty()) {
		req.ai_flags |=  AI_PASSIVE;
	} else if (addr[0] == '[') {
		std::size_t sep = addr.lastIndexOf("]:");
		if (sep == addr.npos) {
			if (addr[addr.length-1] == ']') {
				prepNode = addr.substr(1, addr.length-2);
			} else {
				sep = addr.lastIndexOf(":");
				if (sep == addr.npos) {
					prepNode = addr;
				} else {
					prepNode = addr.substr(0,sep);
					prepPort = addr.substr(sep+1);
				}
			}
		} else {
			prepNode = addr.substr(0,sep);
			prepPort = addr.substr(sep+2);
		}
	} else {
		std::size_t sep = addr.lastIndexOf(":");
		if (sep == addr.npos) {
			prepNode = addr;
		} else {
			prepNode = addr.substr(0,sep);
			prepPort = addr.substr(sep+1);
		}
	}

	if (prepPort.empty()) {
		svc = copyNumberToStr(alloca(30), defaultPort);
	} else {
		svc = copyString(alloca(prepPort.length+1), prepPort);
	}
	node = copyString(alloca(prepNode.length+1),prepNode);

	int e = getaddrinfo(node,svc, &req, &result);
	if (e) {
		if (e == EAI_SYSTEM) throw SystemException(errno);
		else throw GaiError(e);
	}
	return PNetworkAddress::staticCast(RefCntPtr<AddressAddrInfo>(new AddressAddrInfo(result)));
}

NetAddr NetAddr::create(const BinaryView &addr) {
	return  PNetworkAddress::staticCast(RefCntPtr<AddressSockAddr>(
			new(addr.length) AddressSockAddr(reinterpret_cast<const struct sockaddr *>(addr.data),addr.length)));
}



std::string AddressSockAddr::toString(bool resolve) const {
	char namebuff[1024];
	char svcbuff[1024];
	int res = getnameinfo(&sa, len, namebuff+1, sizeof(namebuff)-3, svcbuff,sizeof(svcbuff),
			resolve?0:(NI_NUMERICHOST|NI_NUMERICSERV));
	if (res != 0) {
		throw GaiError(res);
	}
	char *n = namebuff+1;
	if (strchr(n,':') != 0) {
		namebuff[0] = '[';
		strcat(namebuff,"]");
		n = namebuff;
	}
	strcat(namebuff,":");
	return std::string(namebuff).append(svcbuff);
}

BinaryView AddressSockAddr::toSockAddr() const {
	return BinaryView(reinterpret_cast<const unsigned char *>(&sa), len);
}


}

