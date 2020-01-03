#include "resources.h"

namespace simpleServer {
Resource client_rpc_js = {
		"text/javascript",
		R"javascript(  //"

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

		function retryCallback(status, statusText, decision) {			
			if (decision) {
				return this.call2(method,args, retryCnt+1);
			} else {
				throw {code:status,message:statusText};
			}
		}
		

		var id = this.nextId++;
		var req = {
				jsonrpc: "2.0",
				id:this.nextId,
				method:method,
				params: args
		};
		if (Object.keys(this.context).length) {
			req.context = this.context;
		}
		return fetch(this.url, {
			method: "POST",
			headers: {
				"Content-Type":"application/json",
				"Accept":"application/json"
			},
			body: JSON.stringify(req)
		}).then(function(resp) {
			if (resp.status != 200) {
					return this.onConnectionError(
							[resp.status,resp.statusText],retryCnt,
							retryCallback.bind(this,xhr.status,xhr.statusText));
			} else {
				return resp.json().then(function(jres) {
					if (jres.error) {
						throw jres.error;
					} else {
						if (jres.context) {
							this.updateContext(jres.context);
						}
						return jres.result;
					}
				}.bind(this),function(err){
					var msg = "Response parse error";
					return this.onConnectionError(
							[1,err],retryCnt,
							retryCallback.bind(this,1,err));
				}.bind(this));
			}			
		}.bind(this), function(err) {
			return this.onConnectionError(
					[0,err],retryCnt,
					retryCallback.bind(this,0,err));
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
		if (retryCnt > 3) 
			return callback(false);
		else {
			return new Promise(function(ok){
				setTimeout(function(){
					ok(callback(true));
				},retryCnt*500);
			});
		}
	};
	
	RpcClient.prototype.createMethod = function(methodName) {
		return this.call.bind(this, methodName);
	};
	
	RpcClient.prototype.methods = function(retryCnt) {
		if (!retryCnt) retryCnt = 0;
		function retryCallback(status, statusText, decision) {			
			if (decision) {
				return this.methods(retryCnt+1);
			} else {
				fail({code:status,message:statusText});
			}
		}
		return fetch(this.url,{		
			method: "POST",
			headers: {
				"Content-Type":"text/plain",
				"Accept":"application/json"
			},
			body:""
		}).then(function(resp){
			if (resp.status == 200) {
					return resp.json();
			} else {
				return this.onConnectionError(
						[resp.status,resp.statusText],retryCnt,
						retryCallback.bind(this,xhr.status,xhr.statusText));
			}				
		}.bind(this)).catch(function(err) {
			return this.onConnectionError(
					[0,err], retryCnt,
					retryCallback.bind(this,0,err));

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

var WsRpcClient  = (function(){
	"use strict";

	function fixUrl(url) {
		var wsurl;
		var httpurl;
		if (url.substr(0,5) == "ws://") {
			wsurl = url;
			httpurl = "http://"+url.substr(5);
		}
		else if (url.substr(0,6) == "wss://") {
			wsurl = url;
			httpurl = "https://"+url.substr(6);
		}
		else if (url.substr(0,7) == "http://") {
			wsurl = "ws://"+url.substr(7);
			httpurl = url;
		}
		else if (url.substr(0,8) == "https://") {
			wsurl = "wss://"+url.substr(8);
			httpurl = url;
		} else if (url.substr(0,1) == "/") {
			return fixUrl(location.origin+url);
		} else {
			var x = location.pathname.lastIndexOf("/");
			var path = location.pathname.substr(0,x+1);
			return fixUrl(location.origin+path+url);			
		}
		
		return {
			wsurl:wsurl,
			httpurl:httpurl
		};

	}
	

	function WsRpcClient(url) {
		var u = fixUrl(url);
		this.wsurl = u.wsurl;
		RpcClient.call(this,u.httpurl);
		this.socket = null;
		this.waiting = {};
		this.notify = {};
		this.retryCnt = 0;
	}
	WsRpcClient.prototype = Object.create(RpcClient.prototype);
	WsRpcClient.prototype.connect = function() {
		if (this.socket == null) {
			return (this.socket = new Promise(function(ok,error) {
				var init = function() {
					s.removeEventListener("open", init);
					s.removeEventListener("error", e);	
					s.addEventListener("close", this.onclose.bind(this));
					s.addEventListener("message", this.onmessage.bind(this));
					s.addEventListener("error", this.onerror.bind(this));
					ok(s);
				}.bind(this);
				var e = function(x) {
					s.removeEventListener("open", init);
					s.removeEventListener("error", e);	
					error(x);
				}.bind(this);
				var s = new WebSocket(this.wsurl);
				s.addEventListener("open",init);
				s.addEventListener("error",e);
			}.bind(this))).then(function(){});	
		}
	} 

	WsRpcClient.prototype.onclose = function() {	
		this.socket = null;
		if (Object.keys(this.waiting).length != 0) {
			this.onConnectionError([-1,"WebSocket closed"],this.retryCnt++,
				function(resp) {
					var w = this.waiting;
					this.waiting = {};
					for (var x in w) {
						var c = w[x];
						if (resp) {
							c.ok = this.call2(c.method, c.args);
						} else {
							c.error({"code":-1,"message":"connection lost"});
						}
					}					
				}.bind(this));
		}
	}

	WsRpcClient.prototype.onmessage = function(event) {


		this.retryCnt = 0;
		var data = event.data;
		var jdata = JSON.parse(data);
		if (jdata.result) {
			var id = jdata.id;
			var reg = this.waiting[id];
			if (jdata.context) {
				this.updateContext(jdata.context);
			}
			if (reg) reg.ok(jdata.result);
			delete this.waiting[id];
		} else if (jdata.error) {
			var id = jdata.id;
			var reg = this.waiting[id];
			if (reg) reg.error(jdata.error);
			delete this.waiting[id];
		} else if (jdata.method) {
			this.onnotify(jdata.method, jdata.params, jdata.jsonrpc, jdata.id)
		}
	}

	WsRpcClient.prototype.onerror = function() {
	
	} 

	WsRpcClient.prototype.onnotify = function(method,params,version,id) {

		function sendSuccess(id, version, result) {
			if (typeof result =="undefined") {
				sendError(id,version,"Method didn't produce a result");
			} else {
				this.socket.send(JSON.stringify({"id":id, "jsonrpc":version, "result": result}));
			}
		}

		function sendError(id, version, error) {
			if (!(typeof error == "object" && "code" in error && "message" in error)) {
				error = {"code":-32603,"message":"Internal error", "data":error.toString()};
			}
			this.socket.send(JSON.stringify({"id":id, "jsonrpc":version, "error": error}));	
		}	


		var n = this.notify[method];
		if (typeof id != "undefined" && id != null) {
			if (n) {
				var resp = n(params);
				if (typeof resp == "object" && resp instanceof Promise) {
					resp.then(sendSuccess.bind(this,id, version),
						sendError.bind(this,id, version));
				} else {
					sendSiccess.call(this,id, version, resp);
				}
			} else {
				sendError.call(this, id,version,{
					"code":-32601,
					"message":"Method not found",
					"data": method
				})
			}
		} else if (n) {
			n(params);
		}					
	} 

	WsRpcClient.prototype.call2 = function(method,args) {
		return new Promise(function(ok,error) {
			var id = "ws"+(++this.nextId);
			this.waiting[id] = {
				method:method,
				args:args,
				ok:ok,
				error,error};
			var req = {
					"jsonrpc":"2.0",
					"method":method,
					"params":args,
					"id":id};

			if (Object.keys(this.context).length) {
				req.context = this.context;
			}
			
			var m = JSON.stringify(req);

			
			function send() {
				if (this.socket == null) {
					this.connect()
						.then(send.bind(this),this.onclose.bind(this));
				} else {
					this.socket.then(function(s) {
						s.send(m);
					});
				}
			}
			send.call(this);
		}.bind(this));
	}
	
	return WsRpcClient;

})();
	 

//)javascript"
};
}
