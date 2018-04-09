#include <cstring>
#include <netdb.h>
#include <sys/un.h>
 #include <arpa/inet.h>
#include "localAddr.h"
#include "../address.h"

#include "../exceptions.h"

#include "../stringview.h"

using ondra_shared::StringView;


namespace simpleServer {

class AddressAddrInfo: public INetworkAddress {
public:
	AddressAddrInfo(struct addrinfo *addr):addr(addr) {}

	virtual std::string toString(bool resolve = false) const override;
	virtual BinaryView toSockAddr() const override;
	virtual RefCntPtr<INetworkAddress> getNextAddr() const override;

	~AddressAddrInfo() {
		if (addr) freeaddrinfo(addr);
	}

	virtual const INetworkAddress &unproxy() const override {return *this;}


protected:

	struct addrinfo *addr;

};


class AddressAddrInfoSlave: public AddressAddrInfo {
public:
	AddressAddrInfoSlave(struct addrinfo *addr, RefCntPtr<AddressAddrInfo> master)
		:AddressAddrInfo(addr),master(master) {}

	~AddressAddrInfoSlave() noexcept {
		addr = nullptr;
	}

	virtual const INetworkAddress &unproxy() const {return *this;}
protected:
//	template<typename T> friend class RefCntPtr;



	RefCntPtr<AddressAddrInfo> master;

};

RefCntPtr<INetworkAddress> AddressAddrInfo::getNextAddr() const
{
	if (addr->ai_next) return new AddressAddrInfoSlave(addr->ai_next, const_cast<AddressAddrInfo *>(this));
	else return nullptr;
}


class AddressSockAddr: public INetworkAddress {
public:

	AddressSockAddr(const struct sockaddr *sa, int len):len(len) {
		std::memcpy(&this->sa, sa, len);
	}

	void *operator new(std::size_t sz, const int &salen) {
		std::size_t totalsz = sz+salen;
		return ::operator new(totalsz);
	}

	void operator delete(void *ptr, const int &) {
		::operator delete(ptr);
	}

	void operator delete(void *ptr, std::size_t ) {
		::operator delete(ptr);
	}

	virtual std::string toString(bool resolve) const override;
	virtual BinaryView toSockAddr() const override;

	virtual RefCntPtr<INetworkAddress> getNextAddr() const override {
		return nullptr;
	}
	virtual const INetworkAddress &unproxy() const override {return *this;}



protected:
	int len;
	sockaddr sa;
};


std::string AddressAddrInfo::toString(bool ) const {
	if (addr->ai_canonname == 0) {
		char addrbuff[256];
		char portbuff[256];
		switch (addr->ai_family) {
		case AF_INET:
			inet_ntop(addr->ai_family,
					&reinterpret_cast<const sockaddr_in *>(addr->ai_addr)->sin_addr,
					addrbuff,sizeof(addrbuff));
			snprintf(portbuff, sizeof(portbuff), "%d", htons(reinterpret_cast<const sockaddr_in *>(addr->ai_addr)->sin_port));
			return std::string(addrbuff)+":"+portbuff;
		case AF_INET6:
			inet_ntop(addr->ai_family,
					&reinterpret_cast<const sockaddr_in6 *>(addr->ai_addr)->sin6_addr,
					addrbuff,sizeof(addrbuff));
			snprintf(portbuff, sizeof(portbuff), "%d", htons(reinterpret_cast<const sockaddr_in6 *>(addr->ai_addr)->sin6_port));
			return std::string("[")+addrbuff+"]:"+portbuff;
		case AF_UNIX:
			return std::string("unix://")+reinterpret_cast<const sockaddr_un *>(addr->ai_addr)->sun_path;
		default:
			return "<unknown address>";
		}
	} else {
		return addr->ai_canonname;
	}
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

static NetAddr createUnixAddress(StrViewA file) {
	unsigned int perms = 0;
	auto seppos = file.indexOf(":");
	if (seppos != file.npos) {
		perms = 0;
		if (file.length - seppos < 4) {
			throw SystemException(EINVAL,std::string("Permission must have 3 octal numbers:")+std::string(file));
		}
		for (unsigned int i = 0; i < 3; i++) {
			unsigned int v = file[seppos+i+1]-'0';
			if (v > 7) {
				throw SystemException(EINVAL,std::string("Permission must be octal number:")+std::string(file));
			}
			perms = perms * 8 + v;
		}
		file = file.substr(0,seppos);
	}


	return NetAddr(new NetAddrSocket(file,perms));
}



NetAddr NetAddr::create(StrViewA addr, unsigned int defaultPort, AddressType type) {


	if (addr.indexOf(",") != addr.npos) {
		auto spl = addr.split(",");
		StrViewA a = spl();
		a = a.trim(isspace);
		NetAddr addr = create(a, defaultPort, type);
		while (spl) {
			a = spl();
			a = a.trim(isspace);
			NetAddr b = create(a, defaultPort, type);
			addr = addr + b;
		}
		return addr;
	}

	if (addr.substr(0,7) == "unix://" && addr.length>8) {
		return createUnixAddress(addr.substr(6+(addr[7]=='.'?1:0)));
	}

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

	req.ai_flags |=  AI_CANONIDN;


	if (addr.empty()) {
		req.ai_flags |=  AI_PASSIVE;
		node = nullptr;
	} else {
		if (addr[0] == '[') {

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
		node = copyString(alloca(prepNode.length+1),prepNode);

	}

	if (prepPort.empty()) {
		svc = copyNumberToStr(alloca(30), defaultPort);
	} else {
		svc = copyString(alloca(prepPort.length+1), prepPort);
	}

	int e = getaddrinfo(node,svc, &req, &result);
	if (e) {
		if (e == EAI_SYSTEM) throw SystemException(errno);
		else throw GaiError(e);
	}

	//HACK: Some OS returns duplicated records. Perform deduplication
	{
		struct addrinfo *tmp = nullptr, *x = result;
		while (x) {
			auto z = x->ai_next;
			x->ai_next = tmp;
			auto p = tmp;
			while (p) {
				if (p->ai_addrlen == x->ai_addrlen
						&& p->ai_family == x->ai_family
						&& memcmp(p->ai_addr,x->ai_addr,x->ai_addrlen) == 0)
					break;
				p = p->ai_next;
			}
			if (p == nullptr) {
				tmp = x;
			}
			x = z;
		}

		result = 0;
		while (tmp) {
			auto x = tmp;
			tmp = tmp->ai_next;
			x->ai_next = result;
			result = x;
		}
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
	strcat(n,":");
	return std::string(n).append(svcbuff);
}

BinaryView AddressSockAddr::toSockAddr() const {
	return BinaryView(reinterpret_cast<const unsigned char *>(&sa), len);
}

class ChainedNetworkAddr: public INetworkAddress {
public:

	ChainedNetworkAddr(NetAddr master, NetAddr slave, NetAddr next):master(master),slave(slave),next(next) {}

	virtual std::string toString(bool resolve = false) const override {
		return slave.toString(resolve);
	}
	virtual BinaryView toSockAddr() const override {
		return slave.toSockAddr();
	}

	virtual RefCntPtr<INetworkAddress> getNextAddr() const override {
		auto ret = slave.getNextAddr();
		if (ret == nullptr) return next.getHandle();
		else {
			return new ChainedNetworkAddr(master, ret, next);
		}
	};

	~ChainedNetworkAddr() noexcept {}

	virtual const INetworkAddress &unproxy() const override {return (slave.getHandle())->unproxy();}

protected:
	NetAddr master, slave, next;

//	template<typename T> friend class RefCntPtr;



};

NetAddr NetAddr::operator +(const NetAddr& other) const {
	return NetAddr(new ChainedNetworkAddr(*this,*this, other));
}

RefCntPtr<INetworkAddress> NetAddr::getNextAddr() const {
	return addr->getNextAddr();
}


}

