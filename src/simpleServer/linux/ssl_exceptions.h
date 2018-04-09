/*
 * ssl_exceptions.h
 *
 *  Created on: Apr 8, 2018
 *      Author: ondra
 */

#ifndef SIMPLESERVER_LINUX_SSL_EXCEPTIONS_H_
#define SIMPLESERVER_LINUX_SSL_EXCEPTIONS_H_
#include <openssl/x509.h>


namespace simpleServer {

class SSLGenericError: public Exception {

};

class SSLError: public SSLGenericError{
public:
	SSLError();


protected:
	std::string errors;
	virtual  std::string getMessage() const;

};

class SSLCertError: public SSLGenericError {
public:
	SSLCertError(X509 *cert, long err);
protected:
	long err;
	std::shared_ptr<X509> cert;
	virtual  std::string getMessage() const;
};


}


#endif /* SIMPLESERVER_LINUX_SSL_EXCEPTIONS_H_ */
