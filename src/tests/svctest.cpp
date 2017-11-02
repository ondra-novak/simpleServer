#include "../simpleServer/abstractStream.h"
#include "../simpleServer/abstractService.h"


using namespace simpleServer;


int main(int argc, char **argv) {

	return


	ServiceControl::create(argc, argv,"exampleService",[](ServiceControl control, StrViewA name, ArgList args) {

		std::cout << "Service running: " << name << std::endl;
		control.addCommand("stop",[=](ArgList args, Stream sx) {
			sx << "Stopping service\r\n";
			control.stop();
			return 0;
		});

		control.dispatch();
		return 0;
	});


}


