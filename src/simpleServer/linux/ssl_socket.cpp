/*
 * ssl_socket.cpp
 *
 *  Created on: Mar 26, 2018
 *      Author: ondra
 */

#include <mutex>
#include <condition_variable>
#include "tcpStream.h"
#include "ssl_socket.h"
#include <poll.h>
#include "async.h"
#include "../dispatcher.h"
#include <openssl/err.h>
#include <sstream>

#include "../http_client.h"
#include "ssl_exceptions.h"


namespace simpleServer {

#ifndef SSLv23_method
#define TLS_server_method TLSv1_2_server_method
#define TLS_client_method TLSv1_2_client_method
#endif


#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif

class SSLTcpStream: public TCPStream {
public:

	SSLTcpStream(SSL_CTX *ctx, RefCntPtr<TCPStream> srcStream)
		:TCPStream(srcStream->getSocket(), srcStream->getIOTimeout(), srcStream->getPeerAddr())
		,ctx(ctx),orgStream(srcStream),nextTicket(1) {
		ssl = SSL_new(ctx);
		if (!ssl) {
			SSL_CTX_free(ctx);
			throw SSLError();
		}
		if (!SSL_set_fd(ssl, sck)) {
			SSL_free(ssl);
			SSL_CTX_free(ctx);
			throw SSLError();
		}
		setAsyncProvider(srcStream->getAsyncProvider());
	}

	~SSLTcpStream() {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
	}

	virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) override;
	virtual BinaryView implWrite(BinaryView buffer, bool nonblock) override;
	virtual void implCloseOutput() override;
	virtual void implReadAsync(const MutableBinaryView& buffer, Callback&& cb) override;
	virtual void implWriteAsync(const BinaryView& data, Callback&& cb) override;

	void connect();
	void accept();

	SSL *getSSL() const;

protected:
	SSL *ssl;
	SSL_CTX *ctx;
	std::mutex lock;
	RefCntPtr<TCPStream> orgStream;
	typedef std::unique_lock<std::mutex> Sync;

	typedef IAsyncProvider::CompletionFn CompFn;


	static const int errWantRead = -10;
	static const int errWantWrite = -11;
	static const int errNoData = -12;

	bool handleSSLError(int errCode);
	bool handleSSLErrorAsync(int errCode, CompFn &&fn);


	void implReadAsyncTicket(unsigned int ticket, const MutableBinaryView& buffer, Callback&& cb);
	void implWriteAsyncTicket(unsigned int ticket, const BinaryView& data, Callback&& cb);

	class RepeatAsyncWrite;
	class RepeatAsyncRead;

	unsigned int lockTicket = 0;
	std::atomic<unsigned int> nextTicket;
	ondra_shared::Dispatcher waitActions;
};

Stream convert_to_ssl(SSLMode mode, SSL_CTX *sslctx, Stream stream) {
	AbstractStream * as = stream;
	TCPStream &tcp = dynamic_cast<TCPStream &> (*as);
	RefCntPtr<SSLTcpStream> ssl_stream = new SSLTcpStream(sslctx, &tcp);
	if (mode == SSLMode::server) ssl_stream->accept();
	else  ssl_stream->connect();
	return (SSLTcpStream *)ssl_stream;
}



bool SSLTcpStream::handleSSLError(int errCode) {


	int ern = errno;
	int err = SSL_get_error(ssl, errCode);
	switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			return false;
		case SSL_ERROR_WANT_WRITE:
			if (!TCPStream::waitForOutput(iotimeout)) {
				throw TimeoutException();
			}
			return true;

		case SSL_ERROR_WANT_READ:
			if (!TCPStream::waitForInput(iotimeout)) {
				throw TimeoutException();
			}
			return true;
		default:
			if (errCode < 0) throw SystemException(ern);
			else throw SSLError();
	}
}




BinaryView SSLTcpStream::implRead(MutableBinaryView buffer, bool nonblock) {
	do {
		Sync _(lock);
		bool dataReady = SSL_pending(ssl) != 0 || TCPStream::implWaitForRead(0);
		if (!dataReady) {
			if (nonblock) return BinaryView();
			else {
				_.unlock();
				if (!TCPStream::waitForInput(iotimeout)) {
					throw TimeoutException();
				}
				continue;
			}
		}

		int r = SSL_read(ssl,buffer.data, buffer.length);
		if (r <= 0) {
			bool rt = handleSSLError(r);
			if (!rt) return eofConst;
		} else  {
			return BinaryView(buffer.data, r);
		}

	} while (true);
}



BinaryView SSLTcpStream::implWrite(BinaryView buffer, bool nonblock)  {
	do {
		Sync _(lock);
		bool dataReady = TCPStream::implWaitForWrite(0);
		if (!dataReady)  {
			if (nonblock) return buffer;
			else {
				_.unlock();
				if (!TCPStream::waitForOutput(iotimeout)) {
					throw TimeoutException();
				}
				continue;
			}
		}
		int r = SSL_write(ssl,buffer.data, buffer.length);
		if (r <= 0) {
			bool rt = handleSSLError(r);
			if (!rt) return eofConst;
		} else  {
			return buffer.substr(r);
		}
	} while (true);

}


void SSLTcpStream::implCloseOutput() {
	do {
		Sync _(lock);
		int r = SSL_shutdown(ssl);
		if (r <= 0) {
			bool rt = handleSSLError(r);
			if (!rt) return;
		} else {
			return;
		}
	} while (true);
}

class SSLTcpStream::RepeatAsyncRead {
public:
	RepeatAsyncRead(RefCntPtr<SSLTcpStream> owner,unsigned int ticket, MutableBinaryView buffer, Callback &&asyncFn)
			:owner(owner),ticket(ticket),buffer(buffer),asyncFn(std::move(asyncFn)) {}
	void operator()(AsyncState st) {
		if (st == asyncOK) {
			owner->implReadAsyncTicket(ticket,buffer, std::move(asyncFn));
		} else {
			asyncFn(st,BinaryView());
		}
	}
	void operator()() { operator()(asyncOK);}
protected:
	RefCntPtr<SSLTcpStream> owner;
	unsigned int ticket;
	MutableBinaryView buffer;
	Callback asyncFn;
};

class SSLTcpStream::RepeatAsyncWrite {
public:
	RepeatAsyncWrite(RefCntPtr<SSLTcpStream> owner, unsigned int ticket, BinaryView buffer, const Callback &asyncFn)
			:owner(owner),ticket(ticket),buffer(buffer),asyncFn(asyncFn) {}
	void operator()(AsyncState st) {
		if (st == asyncOK) {
			owner->implWriteAsyncTicket(ticket,buffer, std::move(asyncFn));
		} else {
			asyncFn(st,BinaryView());
		}
	}
	void operator()() { operator()(asyncOK);}
protected:
	RefCntPtr<SSLTcpStream> owner;
	unsigned int ticket;
	BinaryView buffer;
	Callback asyncFn;
};

void SSLTcpStream::implReadAsync(const MutableBinaryView& buffer, Callback&& cb) {
	if (asyncProvider == nullptr) throw NoAsyncProviderException();
	unsigned int ticket = ++nextTicket;
	implReadAsyncTicket(ticket, buffer, std::move(cb));
}

void SSLTcpStream::implWriteAsync(const BinaryView& data, Callback&& cb) {
	if (asyncProvider == nullptr) throw NoAsyncProviderException();
	unsigned int ticket = ++nextTicket;
	implWriteAsyncTicket(ticket, data, std::move(cb));
}

bool SSLTcpStream::handleSSLErrorAsync(int errCode, CompFn &&cb) {
	int err = SSL_get_error(ssl, errCode);
	switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			return false;
		case SSL_ERROR_WANT_WRITE:
			asyncProvider->runAsync(AsyncResource(sck,POLLOUT), iotimeout, std::move(cb));
			return true;
		case SSL_ERROR_WANT_READ:
			asyncProvider->runAsync(AsyncResource(sck,POLLIN|POLLRDHUP), iotimeout, std::move(cb));
			return true;
		default:
			throw SSLError();
			break;
	}
}



inline void SSLTcpStream::implReadAsyncTicket(unsigned int ticket,const MutableBinaryView& buffer, Callback&& cb) {
	Sync _(lock);
	if (lockTicket) {
		if (lockTicket != ticket) {
		waitActions << RepeatAsyncRead(this, ticket, buffer,std::move(cb));
		return;
		}
	} else {
		if (SSL_pending(ssl) == 0 && !TCPStream::implWaitForRead(0)) {
			asyncProvider->runAsync(AsyncResource(sck,POLLIN|POLLRDHUP),iotimeout,RepeatAsyncRead(this, ticket, buffer,std::move(cb)));
			return;
		}
	}
	int r = SSL_read(ssl,buffer.data, buffer.length);
	if (r <= 0) {
		try {
			bool rt = handleSSLErrorAsync(r,RepeatAsyncRead(this,ticket, buffer,std::move(cb)));
			if (!rt) {
				_.unlock();
				cb(asyncEOF,eofConst);
				_.lock();
			} else {
				lockTicket = ticket;
				return;
			}
		} catch (...) {
			_.unlock();
			cb(asyncError,BinaryView());
			_.lock();
		}
	} else  {
		_.unlock();
		cb(asyncOK,BinaryView(buffer.data, r));
		_.lock();
	}
	if (lockTicket) {
		lockTicket = 0;
		waitActions.pump();
	}

}

inline void SSLTcpStream::connect() {
	Sync _(lock);
	int r = SSL_connect(ssl);
	while (r < 1) {
		if (!handleSSLError(r)) return;
		r = SSL_connect(ssl);
	}
}

inline void SSLTcpStream::accept() {
	Sync _(lock);
	int r = SSL_accept(ssl);
	while (r < 1) {
		if (!handleSSLError(r)) return;
		r = SSL_accept(ssl);
	}
}

inline SSL* SSLTcpStream::getSSL() const {
	return ssl;
}

inline void SSLTcpStream::implWriteAsyncTicket(unsigned int ticket,	const BinaryView& data, Callback&& cb) {
	Sync _(lock);
	if (lockTicket) {
		if (lockTicket != ticket) {
		waitActions << RepeatAsyncWrite(this, ticket, data,cb);
		return;
		}
	} else {
		if (!TCPStream::implWaitForWrite(0)) {
			asyncProvider->runAsync(AsyncResource(sck,POLLOUT),iotimeout,RepeatAsyncWrite(this, ticket, data,cb));
			return;
		}
	}
	int r = SSL_write(ssl,data.data, data.length);
	if (r <= 0) {
		try {
			bool rt = handleSSLErrorAsync(r,RepeatAsyncWrite(this,ticket, data,cb));
			if (!rt) {
				cb(asyncEOF,eofConst);
			} else {
				lockTicket = ticket;
				return;
			}
		} catch (...) {
			cb(asyncError,BinaryView());
		}
	} else  {
		cb(asyncOK,data.substr(r));
	}
	if (lockTicket) {
		lockTicket = 0;
		waitActions.pump();
	}



}


SSLError::SSLError() {
	ERR_print_errors_cb([](const char *str, size_t len, void *u){
		std::string *s = reinterpret_cast<std::string *>(u);
		s->append(str,len);
		s->append("\n");
		return 1;
	}, &errors);
}

std::string SSLError::getMessage() const {
	return errors;
}

Stream SSLServerFactory::convert_to_ssl(Stream stream) {



	SSL_CTX *ctx;
	ctx = SSL_CTX_new(TLS_server_method());
	try {
		setup(ctx);

		AbstractStream * as = stream;
		TCPStream &tcp = dynamic_cast<TCPStream &> (*as);

		RefCntPtr<SSLTcpStream> ssl_stream = new SSLTcpStream(ctx, &tcp);

		SSL *ssl = ssl_stream->getSSL();
		precreateConnection(ctx,ssl);
		ssl_stream->accept();
		verifyConnection(ctx, ssl_stream->getSSL(), ssl_stream);
		return (SSLTcpStream *)ssl_stream;
	} catch (...) {
		SSL_CTX_free(ctx);
		throw;
	}
}

Stream SSLClientFactory::convert_to_ssl(Stream stream, const std::string &host) {
	SSL_CTX *ctx;
	ctx = SSL_CTX_new(TLS_client_method());
	try {
		setup(ctx);
	} catch (...) {
		SSL_CTX_free(ctx);
		throw;
	}

	AbstractStream * as = stream;
	TCPStream &tcp = dynamic_cast<TCPStream &> (*as);

	RefCntPtr<SSLTcpStream> ssl_stream = new SSLTcpStream(ctx, &tcp);

	SSL *ssl = ssl_stream->getSSL();
	precreateConnection(ctx,ssl);
	if(!SSL_set_tlsext_host_name(ssl, host.c_str())) throw SSLError();
	if(!X509_VERIFY_PARAM_set1_host(SSL_get0_param(ssl), host.c_str(), 0)) throw SSLError();
	ssl_stream->connect();
	verifyConnection(ctx, ssl_stream->getSSL(), ssl_stream);
	return (SSLTcpStream *)ssl_stream;


}

Stream SSLClientFactory::convert_to_ssl(Stream stream) {
	return convert_to_ssl(stream,host);
}

void SSLAbstractStreamFactory::setup(SSL_CTX* ctx) {
	SSL_CTX_set_default_verify_paths(ctx);
	if (!certfile.empty()) {
	    if (SSL_CTX_use_certificate_file(ctx, certfile.c_str(), SSL_FILETYPE_PEM) <= 0) {
		        throw SSLError();
	    }
	}
	if (!privkeyfile.empty()) {
		if (SSL_CTX_use_PrivateKey_file(ctx, privkeyfile.c_str(), SSL_FILETYPE_PEM) <= 0 ) {
			throw SSLError();
		}
	}
}



void SSLServerFactory::setup(SSL_CTX* ctx) {
	SSLAbstractStreamFactory::setup(ctx);


}

void SSLClientFactory::setup(SSL_CTX* ctx) {
	SSLAbstractStreamFactory::setup(ctx);

}

void SSLClientFactory::setHost(const std::string& host) {
	this->host = host;
}


void SSLAbstractStreamFactory::verifyConnection(SSL_CTX* , SSL*ssl , AbstractStream* ) {
	X509 *cert = SSL_get_peer_certificate(ssl);
	if(cert) {
	    const long cert_res = SSL_get_verify_result(ssl);
	    // in case of name/domain mismatch cert_res will
	    // be set as 62 --> X509_V_ERR_HOSTNAME_MISMATCH
	    if(cert_res != X509_V_OK) {
	    	throw SSLCertError(cert, cert_res);
	    } else {
	    	X509_free(cert);
	    }

	} else throw SSLError();

}


void SSLServerFactory::verifyConnection(SSL_CTX* ctx, SSL* ssl, AbstractStream* stream) {
	SSLAbstractStreamFactory::verifyConnection(ctx,ssl,stream);
}

void SSLAbstractStreamFactory::setCertFile(std::string certfile) {
	this->certfile = certfile;
}

void SSLAbstractStreamFactory::setPrivKeyFile(std::string privkeyfile) {
	this->privkeyfile = privkeyfile;
}

void SSLClientFactory::verifyConnection(SSL_CTX* ctx, SSL* ssl, AbstractStream* stream) {
	SSLAbstractStreamFactory::verifyConnection(ctx,ssl,stream);
}


void SSLAbstractStreamFactory::precreateConnection(SSL_CTX* , SSL* ) {
}

void SSLServerFactory::precreateConnection(SSL_CTX* ctx, SSL* ssl) {
	SSLAbstractStreamFactory::precreateConnection(ctx,ssl);
}

void SSLClientFactory::precreateConnection(SSL_CTX* ctx, SSL* ssl) {
	SSLAbstractStreamFactory::precreateConnection(ctx,ssl);
}


std::once_flag sslinit;

SSLAbstractStreamFactory::SSLAbstractStreamFactory() {
	std::call_once(sslinit,[]{
		SSL_library_init();
	});
}

class HttpsProvider: public IHttpsProvider {
public:
	HttpsProvider(SSLClientFactory *f):f(f?f:new SSLClientFactory) {}

	virtual Stream connect(Stream conn, StrViewA hostname) {
		return f->convert_to_ssl(conn,hostname);
	}
protected:
	std::unique_ptr<SSLClientFactory> f;
};

IHttpsProvider* newHttpsProvider(SSLClientFactory* sslfactory) {
	return new HttpsProvider(sslfactory);
}

SSLCertError::SSLCertError(X509* cert, long err)
:err(err),cert(cert, [](X509* c){
	X509_free(c);
})
{
}

std::string SSLCertError::getMessage() const {
	std::ostringstream b;
	char name[257];
	X509_NAME_oneline(X509_get_subject_name(cert.get()), name, 256);
	b << "Error in certificate '" << name << "': ";
	switch (err) {
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT  : b << "Unable to get issuer of cert.";break;
	case X509_V_ERR_UNABLE_TO_GET_CRL  : b << "Unable to get CRL";break;
	case X509_V_ERR_CERT_NOT_YET_VALID  : b << "Not yet valid";break;
	case X509_V_ERR_CERT_HAS_EXPIRED : b << "Expired";break;
	case X509_V_ERR_CRL_NOT_YET_VALID : b << "CLR not yet valid";break;
	case X509_V_ERR_CRL_HAS_EXPIRED : b << "CLR expired";break;
	case X509_V_ERR_OUT_OF_MEM : b << "Out of memory";break;
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT : b << "Self signed certificate";break;
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN : b << "Self signed certificate in chain";break;
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY : b << "Unknown issuer";break;
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE : b << "Unable to verify leaf signature";break;
	case X509_V_ERR_CERT_CHAIN_TOO_LONG : b << "Too long chain";break;
	case X509_V_ERR_CERT_REVOKED : b << "Revoked";break;
	case X509_V_ERR_INVALID_CA : b << "Invalid CA";break;
	case X509_V_ERR_PATH_LENGTH_EXCEEDED : b << "Path length exceeded";break;
	case X509_V_ERR_INVALID_PURPOSE : b << "Invalid purpose";break;
	case X509_V_ERR_CERT_UNTRUSTED : b << "Untrusted";break;
	case X509_V_ERR_CERT_REJECTED : b << "Rejected";break;
	case  X509_V_ERR_HOSTNAME_MISMATCH : b << "Host mismatch"; break;
	default: b << "Error: " << err;break;
	}

	b << " (code: " << err << ")";
	return b.str();
}


}

