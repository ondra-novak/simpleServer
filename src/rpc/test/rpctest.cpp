#include "../../simpleServer/abstractService.h"
#include "../../simpleServer/http_pathmapper.h"
#include "../../simpleServer/html_escape.h"
#include "../../simpleServer/http_server.h"
#include "../../simpleServer/shared/stdLogFile.h"
#include "../rpcServer.h"

using namespace simpleServer;

using ondra_shared::StdLogFile;
using namespace json;

int main(int argc, char **argv) {
	return
	ServiceControl::create(argc, argv,"exampleWebServer",[](ServiceControl control, StrViewA name, ArgList args) {

		StdLogFile log("logfile");
		log.setCurrent();

		int port = 8787;
		RpcHttpServer server(NetAddr::create("",port),0,0);

		server.addRPCPath("/RPC", {true, 65536});
		server.add_listMethods("methods");
		server.add_ping("ping");
		server.add("sum",[](RpcRequest req){
			Value sum;
			if (!req.getArgs().empty()) {
				sum = req[0];
				for (std::size_t i = 1; i < req.getArgs().size();i++) {
					sum = sum.merge(req[i]);
				}
			}
			if (sum.defined()) {
				req.setResult(sum);
			} else {
				req.setError(1,"Result is not defined");
			}
		});

		server.start();




		std::cout << "Server running on port " <<port << std::endl;
		control.dispatch();

		return 0;
	});
}


