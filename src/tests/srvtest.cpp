#include "../simpleServer/abstractService.h"
#include "../simpleServer/http_pathmapper.h"
#include "../simpleServer/html_escape.h"
#include "../simpleServer/http_server.h"
#include "../simpleServer/shared/stdLogFile.h"

using namespace simpleServer;

using ondra_shared::StdLogFile;

static bool demoPage(const HTTPRequest &req, const StrViewA &vpath) {
	if (vpath.empty()) {
		req.redirectToFolderRoot();
		return true;
	}
	if (vpath[0] != '/') return false;
	Stream s = req.sendResponse("text/html");
	HtmlEscape<Stream> escape(s);

	s << "<html><body><table border=\"1\"><tr><th>Key</th><th>Value</th></tr>";
	s << "<tr><td>Path</td><td>";escape(req.getPath());s<<"</td></tr>";
	s << "<tr><td>VPath</td><td>";escape(vpath);s<<"</td></tr>";
	s << "<tr><td>Method</td><td>";escape(req.getMethod());s<<"</td></tr>";
	s << "</table></body></html>";

	return true;
}

int main(int argc, char **argv) {
	return
	ServiceControl::create(argc, argv,"exampleWebServer",[](ServiceControl control, StrViewA name, ArgList args) {

		StdLogFile log("logfile");
		log.setCurrent();

		typedef HttpStaticPathMapper::MapRecord M;
		M mappings[] = {
			{"",&demoPage},
			{"/aaa",&demoPage},
			{"/bbb",&demoPage},
			{"/bbb/aaa",&demoPage},
			{"/abd",&demoPage},
			{"/abd/",&demoPage},
			{"/abc",&demoPage},
			{"/ab",&demoPage},
			{"/ab/",&demoPage},
			{"/12345678",&demoPage},
			{"/123434",&demoPage},
			{"/12",&demoPage}
		};
		int port = 8787;
		HttpStaticPathMapperHandler pages(mappings);
		MiniHttpServer server(NetAddr::create("",port),0,0);

		server >> pages;

//		control.changeUser("ondra:sudo");

		std::cout << "Server running on port " <<port << std::endl;
		control.dispatch();

		return 0;
	});
}


