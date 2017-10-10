#pragma once
#include <string>

#include "refcnt.h"
#include "stringview.h"

namespace simpleServer {

class INetworkAddress: public RefCntObj {
public:

	virtual std::string toString(bool resolve = false) const = 0;
	virtual BinaryView toSockAddr() const = 0;
	virtual ~INetworkAddress() {}

	///in case of the address contains multiple records, this function returns next record
	/**
	 * @return ref pointer to next address, or nullptr
	 */
	virtual RefCntPtr<INetworkAddress> getNextAddr() const = 0;


};

typedef RefCntPtr<INetworkAddress> PNetworkAddress;

class NetAddr {
public:

	NetAddr(RefCntPtr<INetworkAddress> addr):addr(addr) {}

	std::string toString(bool resolve = false) const;
	BinaryView toSockAddr() const;

	enum AddressType {
		IPvAll,
		IPv4,
		IPv6
	};

	RefCntPtr<INetworkAddress> getNextAddr();

	static NetAddr create(StrViewA addr, unsigned int defaultPort, AddressType type = IPvAll);
	static NetAddr create(const BinaryView &sockAddr);

protected:

	RefCntPtr<INetworkAddress> addr;


};


}
