#include "resources.h"

namespace simpleServer {
Resource client_rpc_js = {
		"text/javascript",
		R"javascript(

var RpcClient = (function(){
	"use strict";
	
	
	function RpcClient(url) {
		this.url = url;
		this.nextId = 1;
		this.context={};
	};
	
	function isObjEmpty(obj) {
		Object.keys(obj).length === 0; 
	}
	
	RpcClient.prototype.call2 = function(method,args,retryCnt) {
		if (!retryCnt) retryCnt = 0;
		
		
		return new Promise(function(ok, fail) {

			var id = this.nextId++;
			function retryCallback(status, statusText, decision) {			
				if (decision) {
					this.call2(method,args, retryCnt+1).then(ok,fail);
				} else {
					fail({code:status,message:statusText});
				}
			}
			
			var req = {
					jsonrpc: "2.0",
					id:this.nextId,
					method:method,
					params: args
			};
			if (Object.keys(this.context).length) {
				req.context = this.context;
			}
			
			var xhr = new XMLHttpRequest;
			xhr.open("POST", this.url);
			xhr.setRequestHeader("Content-Type","application/json");
			xhr.setRequestHeader("Accept","application/json");
			xhr.onreadystatechange = function() {
				if (xhr.readyState == 4) {
					if (xhr.status != 200) {
						this.onConnectionError(
								[xhr.status,xhr.statusText],
								retryCnt,
								retryCallback.bind(this,xhr.status,xhr.statusText));
					} else {
						var jres;
						try {
							 jres= JSON.parse(xhr.responseText);
						} catch (e) {
							var msg = "Response parse error";
							this.onConnectionError(
									[1,msg],
									retryCnt,
									retryCallback.bind(this,1,msg));
							return;
						} 
						if (jres.error) {
							fail(jres.error);
						} else {
							if (jres.context) {
								this.updateContext(jres.context);
							}
							ok(jres.result);
						}
					}
				}
			}.bind(this);			
			xhr.send(JSON.stringify(req));
			
		}.bind(this));
	};
	
	RpcClient.prototype.updateContext = function(newCtx) {
		for (var i in newCtx) {
			if (newCtx[i] === null) delete this.context[i];
			else this.context[i] = newCtx[i];
		}
	}
	
	
	RpcClient.prototype.call = function(method) {
		var args = Array.prototype.slice.call(arguments,1);
		return this.call2(method,args,0);
	};
	
	RpcClient.prototype.onConnectionError = function(details, retryCnt, callback) {
		if (retryCnt > 3) callback(false);
		else {
			setTimeout(callback.bind(null,true),retryCnt*500);
		}
	};
	
	RpcClient.prototype.createMethod = function(methodName) {
		return this.call.bind(this, methodName);
	};
	
	RpcClient.prototype.methods = function(retryCnt) {
		if (!retryCnt) retryCnt = 0;
		return new Promise(function(ok, fail) {

			function retryCallback(status, statusText, decision) {			
				if (decision) {
					this.methods(retryCnt+1).then(ok,fail);
				} else {
					fail({code:status,message:statusText});
				}
			}

			var xhr = new XMLHttpRequest;
			xhr.open("POST", this.url);
			xhr.setRequestHeader("Accept","application/json");
			xhr.onreadystatechange = function() {
				
				if (xhr.readyState == 4) {
					if (xhr.status != 200) {
						this.onConnectionError(
								[xhr.status,xhr.statusText],
								retryCnt,
								retryCallback.bind(this,xhr.status,xhr.statusText));
					} else {
						var jres;
						try {
							 jres= JSON.parse(xhr.responseText);
						} catch (e) {
							var msg = "Response parse error";
							this.onConnectionError(
									[1,msg],
									retryCnt,
									retryCallback.bind(this,1,msg));
							return;
						}
						ok(jres);
					}
				}
				
			}.bind(this);
			xhr.send("");
		}.bind(this));		
	};
	
	RpcClient.prototype.createObject = function(prefix) {
		return this.methods().then(function(m) {
		
			var obj = {};
			m.forEach(function(itm) {
				
				if (!prefix || itm.substr(0,prefix.length) == prefix) {
					
					if (prefix) {
						itm = itm.substr(prefix.length);
						if (itm.substr(0,1) == ".") itm = itm.substr(1);
					}
					if (itm) {
						var parts = itm.split(".");
						var x = obj;
						var last = null;
						parts.forEach(function(p){
							if (last !== null) {
								if (typeof x[last] != "object") {
									x[last] = {};
								}
								x = x[last];
							}
							last = p;
						});
						if (last !== null) {
							x[last] = this.createMethod(itm);
						}
					}

					
				}			
			}.bind(this));
			return obj;
		}.bind(this));		
	};
	
	return RpcClient;

})();

)javascript"};}
