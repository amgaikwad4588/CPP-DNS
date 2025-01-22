#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>

// Define the DNS Message Header
struct DNS_Header {
    uint16_t ID;     // Identification
    uint16_t FLAGS;  // Flags
    uint16_t QDCOUNT; // Number of questions
    uint16_t ANCOUNT; // Number of answers
    uint16_t NSCOUNT; // Number of authority records
    uint16_t ARCOUNT; // Number of additional records
};

// Define the DNS Question Section
struct DNS_Question {
    std::vector<uint8_t> NAME; // Domain name encoded as labels
    uint16_t TYPE;            // Query type
    uint16_t CLASS;           // Query class
};

// Define the DNS Message Structure
struct DNS_Message {
    DNS_Header header;
    DNS_Question question;

    void to_network_order() {
        header.ID = htons(header.ID);
        header.FLAGS = htons(header.FLAGS);
        header.QDCOUNT = htons(header.QDCOUNT);
        header.ANCOUNT = htons(header.ANCOUNT);
        header.NSCOUNT = htons(header.NSCOUNT);
        header.ARCOUNT = htons(header.ARCOUNT);
        question.TYPE = htons(question.TYPE);
        question.CLASS = htons(question.CLASS);
    }
};

// Create the DNS response with the question section
void create_response(DNS_Message &dns_message) {
    // Fill the header section
    dns_message.header.ID = 1234;
    dns_message.header.FLAGS = (1 << 15); // Set response flag
    dns_message.header.QDCOUNT = 1;      // One question
    dns_message.header.ANCOUNT = 0;      // No answer records
    dns_message.header.NSCOUNT = 0;      // No authority records
    dns_message.header.ARCOUNT = 0;      // No additional records

    // Encode the domain name (codecrafters.io)
    std::string first_label = "codecrafters";
    std::string second_label = "io";

    dns_message.question.NAME.push_back((uint8_t)first_label.size());
    for (char c : first_label)
        dns_message.question.NAME.push_back(c);

    dns_message.question.NAME.push_back((uint8_t)second_label.size());
    for (char c : second_label)
        dns_message.question.NAME.push_back(c);

    dns_message.question.NAME.push_back(0); // Null byte terminator

    // Fill the TYPE and CLASS fields
    dns_message.question.TYPE = 1;  // A record
    dns_message.question.CLASS = 1; // IN class

    // Convert fields to network byte order
    dns_message.to_network_order();
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    setbuf(stdout, NULL);

    int udpSocket;
    struct sockaddr_in clientAddress;

    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Socket creation failed: " << strerror(errno) << "..." << std::endl;
        return 1;
    }

    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "SO_REUSEPORT failed: " << strerror(errno) << std::endl;
        return 1;
    }

    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(udpSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true) {
        int bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }

        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;

        // Create the response
        DNS_Message response;
        create_response(response);

        // Serialize the response
        std::vector<uint8_t> responseBuffer;

        // Header
        responseBuffer.insert(responseBuffer.end(), (uint8_t *)&response.header, (uint8_t *)&response.header + sizeof(response.header));

        // Question NAME
        responseBuffer.insert(responseBuffer.end(), response.question.NAME.begin(), response.question.NAME.end());

        // Question TYPE
        responseBuffer.push_back((response.question.TYPE >> 8) & 0xFF);
        responseBuffer.push_back(response.question.TYPE & 0xFF);

        // Question CLASS
        responseBuffer.push_back((response.question.CLASS >> 8) & 0xFF);
        responseBuffer.push_back(response.question.CLASS & 0xFF);

        // Send the response
        if (sendto(udpSocket, responseBuffer.data(), responseBuffer.size(), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1) {
            perror("Failed to send response");
        }
    }

    close(udpSocket);
    return 0;
}
