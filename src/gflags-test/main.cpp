#include <iostream>
#include <gflags/gflags.h>   


using namespace std;

DEFINE_string(confPath, "../conf/setup.ini", "program configure file.");
DEFINE_int32(port, 9090, "program listen port");
DEFINE_bool(daemon, true, "run daemon mode");

int main(int argc, char** argv)
{
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	cout << "confPath =" << FLAGS_confPath << endl;
	cout << "port =" << FLAGS_port << endl;

	if (FLAGS_daemon) {
		cout << "run background ..." << endl;
	}
	else {
		cout << "run foreground ..." << endl;
	}

	cout << "good luck and good bye!" << endl;

	gflags::ShutDownCommandLineFlags();
	return 0;
}