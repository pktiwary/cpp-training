#include <fcgiapp.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <cstdio>
#include <cstring>
#include <list>
#include <chrono>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

static const char* resp = "Content-type: text/html\r\n"
    "\r\nFastCGI Hello! PID: [%d] thread ID: [%d] "
    "listening on [%s] request received at [%s] responded at [%s]";

std::string getCurrentTime()
{
    time_t rawtime;
    struct tm *info;
    char buffer[80];

    time( &rawtime );
    info = localtime( &rawtime );
    strftime(buffer, sizeof(buffer),"%x %X", info);
    return std::string(buffer);
}

int openSocket(const char* conn, int queueDepth) {
    auto unixDomainSocket = true;
    mode_t saved_mask;
    // if listening on a unix domain socket, we need to make sure it has the
    // correct permissions. In case of TCP, it doesn't matter
    if(conn && conn[0] == ':') unixDomainSocket = false;
    if(unixDomainSocket) saved_mask = umask(0);
    auto listenFd = FCGX_OpenSocket(conn, queueDepth);
    if(unixDomainSocket) umask(saved_mask);
    return listenFd;
}

int testSingleThreaded(const char* conn) {
    std::cout << "This is a demo fcgi program (single threaded)" << std::endl;
     if(FCGX_Init() != 0) {
        std::cerr << "Error in calling FCGX_Init()" << std::endl;
        return -1;
    }

    FCGX_Request request;
    auto listenFd = openSocket(conn, 5);
    if(listenFd < 0) {
    	std::cerr << "Error in calling openSocket(" << conn << ", 5)" << std::endl;
        return -2;
    }

    FCGX_InitRequest(&request, listenFd, 0);
    std::cout << "Init request successful" << std::endl;

    while (FCGX_Accept_r(&request) >= 0) {
        char msg[4096];
        auto acceptTime = getCurrentTime();
        usleep(5000);
        std::snprintf(msg, sizeof(msg) - 1, resp, getpid(),
            std::this_thread::get_id(),
            conn, acceptTime.c_str(), getCurrentTime().c_str());
        msg[sizeof(msg) - 1] = '\0';
        FCGX_PutStr(msg, std::strlen(msg), request.out);
    }
    return 0;
}

void processRequest(int sock, const char* conn)
{
	 if(FCGX_Init() != 0) {
		std::cerr << "Error in calling FCGX_Init()" << std::endl;
		return;
	}

	FCGX_Request request;
	FCGX_InitRequest(&request, sock, 0);
	std::cout << "Init request successful" << std::endl;

	while (FCGX_Accept_r(&request) >= 0) {
		std::string acceptTime = getCurrentTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
		std::string respondTime = getCurrentTime();
		char msg[4096];
		snprintf(msg, sizeof(msg) - 1, resp, getpid(),
				std::this_thread::get_id(),
				conn, acceptTime.c_str(),
				respondTime.c_str());
		msg[sizeof(msg) - 1] = '\0';
		FCGX_PutStr(msg, std::strlen(msg), request.out);
	}
}

int testMultiThreaded(const char* conn, int numThreads) {
    std::cout << "This is a demo fcgi program (" << numThreads << " threads)" << std::endl;
    int listenFd = openSocket(conn, 10);
    if(listenFd < 0) {
    	std::cerr << "Error in calling openSocket(" << conn << ", 5)" << std::endl;
        return -2;
    }

    std::list<std::thread> tids;
    for(int i = 0; i < numThreads; ++i) {
    	tids.push_back(std::thread(processRequest, listenFd, conn));
    }
    for(auto itr = tids.begin(); itr != tids.end(); ++itr) {
    	itr->join();
    }
    return 0;
}


} // end unnamed namespace

int main(int argc, char* argv[])
{
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [:<TCP Port>|<Unix socket path>] [<num threads>]" << std::endl;
        return -1;
    }
    if(argc >= 3) { // number of threads specified
        testMultiThreaded(argv[1], std::atoi(argv[2]));
    } else {
        testSingleThreaded(argv[1]);
    }
}
