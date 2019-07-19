/*
 * ssl_socket.h
 *
 *  Created on: Mar 26, 2018
 *      Author: ondra
 */

#ifndef SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_SSL_SOCKET_H_
#define SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_SSL_SOCKET_H_
#include <openssl/ssl.h>
#include "../exceptions.h"
#include "../abstractStream.h"


namespace simpleServer {


enum class SSLMode {
	client,
	server
};


class SSLAbstractStreamFactory {
public:
	SSLAbstractStreamFactory();
	virtual ~SSLAbstractStreamFactory() {};
	virtual Stream convert_to_ssl(Stream stream) = 0;
	virtual void setup(SSL_CTX *ctx);
	virtual void verifyConnection(SSL_CTX *ctx, SSL *ssl, AbstractStream *stream);
	virtual void precreateConnection(SSL_CTX *ctx, SSL *ssl);


	void setCertFile(std::string certfile);
	void setPrivKeyFile(std::string privkeyfile);


protected:

	std::string certfile;
	std::string privkeyfile;

};

class SSLServerFactory: public SSLAbstractStreamFactory {
public:

	virtual Stream convert_to_ssl(Stream stream) override;
	virtual void setup(SSL_CTX *ctx) override;
	virtual void verifyConnection(SSL_CTX *ctx, SSL *ssl, AbstractStream *stream) override;
	virtual void precreateConnection(SSL_CTX *ctx, SSL *ssl)  override;

};

class SSLClientFactory: public SSLAbstractStreamFactory {
public:
	virtual Stream convert_to_ssl(Stream stream) override;
	virtual Stream convert_to_ssl(Stream stream, const std::string &host);
	virtual void setup(SSL_CTX *ctx) override;
	virtual void verifyConnection(SSL_CTX *ctx, SSL *ssl, AbstractStream *stream)  override;
	virtual void precreateConnection(SSL_CTX *ctx, SSL *ssl)  override;


	void setHost(const std::string &host);
	std::string host;

};

class IHttpsProvider;
IHttpsProvider *newHttpsProvider(SSLClientFactory *sslfactory);

}




#endif /* SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_SSL_SOCKET_H_ */
