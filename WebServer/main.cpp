#include <iostream>
#include "http_server.h"


int main() {
    WebServer::http_server server;
    std::cout << "Running..." << std::endl;
    server.listen("18080");
}
