/*
 * base64.cpp
 *
 *  Created on: Apr 5, 2018
 *      Author: ondra
 */




namespace simpleServer {


const char *base64_standard =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				  "abcdefghijklmnopqrstuvwxyz"
				  "0123456789"
	              "+/";
const char *base64_url = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				  "abcdefghijklmnopqrstuvwxyz"
				  "0123456789"
	              "-_";

}
