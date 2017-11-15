#pragma once

#include <cstdint>
#include "stringview.h"

namespace simpleServer {



struct SHA1_CTX
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} ;

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
    );

void SHA1(
    char *hash_out,
    const char *str,
    int len);


class Sha1 {
public:

	Sha1() {
		SHA1Init(&ctx);
	}

	Sha1& reset() {
		SHA1Init(&ctx);return *this;
		return *this;
	}

	Sha1& update(const BinaryView &data) {
		SHA1Update(&ctx, data.data,data.length);
		return *this;
	}

	BinaryView final() {
		SHA1Final( digest,&ctx);
		return BinaryView(digest,20);
	}



protected:
	SHA1_CTX ctx;
	unsigned char digest[20];

};



}


