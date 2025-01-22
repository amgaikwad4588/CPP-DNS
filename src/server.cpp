#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "server_init.hpp"
#include "dns_message.hpp"
#include "request_handling.hpp"

void configure_output() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    setbuf(stdout, NULL);
}

Resolver_Info parse_resolver_arg(int argc, char** argv) {
    Resolver_Info resolver_info;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--resolver") == 0) {
            std::string resolver = argv[i + 1];
            size_t colon_pos = resolver.find(':');
            if (colon_pos != std::string::npos) {
                resolver_info.ip = resolver.substr(0, colon_pos);
                resolver_info.port = resolver.substr(colon_pos + 1);
            }
            break;
        }
    }
    return resolver_info;
}

void handle_client_request(int udpSocket, Resolver_Info& resolver_info) {
    struct sockaddr_in clientAddress;
    socklen_t clientAddrLen = sizeof(clientAddress);
    uint8_t buffer[512];

    int bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, 
                             reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddrLen);
    if (bytesRead == -1) {
        perror("Error receiving data");
        return;
    }

    buffer[bytesRead] = '\0';
    std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;

    DNS_Message response = create_response(buffer);
    int question_number = response.header.QDCOUNT;
    response.to_network_order();
    response.create_response_labels(question_number, buffer);

    for (int question_index = 0; !resolver_info.ip.empty() && question_index < question_number; ++question_index) {
        query_resolver_server(resolver_info.socket, resolver_info.addr, response, question_index);
    }

    auto [responseBuffer, responseSize] = create_response_buffer(question_number, response);

    if (sendto(udpSocket, responseBuffer.get(), responseSize, 0, 
               reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress)) == -1) {
        perror("Failed to send response");
    }
}

int main(int argc, char** argv) {
    configure_output();

    Resolver_Info resolver_info = parse_resolver_arg(argc, argv);
    setup_resolver_socket(resolver_info);

    auto [server_code, udpSocket] = create_server(2053);
    if (server_code != 0) {
        std::cerr << "Server creation failed!\n";
        return server_code;
    }

    while (true) {
        handle_client_request(udpSocket, resolver_info);
    }

    close(udpSocket);
    close(resolver_info.socket);

    return 0;
}