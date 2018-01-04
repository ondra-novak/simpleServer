#pragma once
#include <string>

#include "shared/refcnt.h"
#include "shared/stringview.h"

namespace simpleServer {

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;
using ondra_shared::StrViewA;
using ondra_shared::BinaryView;

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
	virtual const INetworkAddress &unproxy() const = 0;



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

	RefCntPtr<INetworkAddress> getNextAddr() const;
	RefCntPtr<INetworkAddress> getHandle() const {return addr;}

	static NetAddr create(StrViewA addr, unsigned int defaultPort, AddressType type = IPvAll);
	static NetAddr create(const BinaryView &sockAddr);

	///Allows to combine multiple addresses into single NetAddr object.
	/** The stream factories can use this multiple addresses as benefit while talking with other side.
	 *
	 * For example for connected stream, the address can contain alternative targets which only one need
	 * to be active. For listening stream, multiple ports can be opened for listening under single stream instance
	 *
	 * @param other other network address
	 * @return Object which contains multiple addresses. Each addres can be retrieved by function getNextAddr()
	 */
	NetAddr operator+ (const NetAddr &other) const;

protected:

	RefCntPtr<INetworkAddress> addr;

};



}
