#include <unistd.h>

#include "../simpleServer/abstractStream.h"
#include "../simpleServer/abstractService.h"
#include "../simpleServer/http_parser.h"
#include "../simpleServer/http_pathmapper.h"
#include "../simpleServer/html_escape.h"
#include "../simpleServer/http_server.h"


using namespace simpleServer;



static bool demoPage(HTTPRequest req, StrViewA vpath) {
	Stream s = req.sendResponse("text/html");
	HtmlEscape<Stream> escape(s);

	s << "<html><body><table border=\"1\"><tr><th>Key</th><th>Value</th></tr>";
	s << "<tr><td>Path</td><td>";escape(req.getPath());s<<"</td></tr>";
	s << "<tr><td>VPath</td><td>";escape(vpath);s<<"</td></tr>";
	s << "<tr><td>Method</td><td>";escape(req.getMethod());s<<"</td></tr>";
	s << "</table></body></html>";

	return true;
}

static bool rejectPage(HTTPRequest req, StrViewA vpath) {
	return false;
}

int main(int argc, char **argv) {

	return


	ServiceControl::create(argc, argv,"exampleService",[](ServiceControl control, StrViewA name, ArgList args) {

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
			{"/12345678",&rejectPage},
			{"/123434",&demoPage},
			{"/12",&demoPage}
		};

		HttpStaticPathMapperHandler pages(mappings);

		MiniHttpServer server(NetAddr::create("",8787),0,0);

		server >> pages;


		std::cout << "Server running on port 8787" << std::endl;

		control.dispatch();

		return 0;
	});


}


