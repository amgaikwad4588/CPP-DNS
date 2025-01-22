#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

// DNS Header structure
struct DNS_Header {
    uint16_t ID;
    uint16_t FLAGS;
    uint16_t QDCOUNT;
    uint16_t ANCOUNT;
    uint16_t NSCOUNT;
    uint16_t ARCOUNT;
};

// Helper to convert 16-bit and 32-bit integers to network byte order
inline uint16_t to_network_order_16(uint16_t value) {
    return htons(value);
}

inline uint32_t to_network_order_32(uint32_t value) {
    return htonl(value);
}

// Helper to parse domain names
std::string parse_domain_name(const uint8_t* data, size_t& offset) {
    std::string domain_name;
    while (data[offset] != 0) {
        uint8_t label_length = data[offset++];
        for (int i = 0; i < label_length; ++i) {
            domain_name += static_cast<char>(data[offset++]);
        }
        domain_name += '.';
    }
    ++offset; // Skip the null byte
    if (!domain_name.empty()) {
        domain_name.pop_back(); // Remove trailing dot
    }
    return domain_name;
}

// Helper to encode domain names as label sequences
std::vector<uint8_t> encode_domain_name(const std::string& domain) {
    std::vector<uint8_t> encoded_name;
    size_t start = 0, end = 0;

    while ((end = domain.find('.', start)) != std::string::npos) {
        encoded_name.push_back(end - start); // Length of the label
        for (size_t i = start; i < end; ++i) {
            encoded_name.push_back(domain[i]);
        }
        start = end + 1;
    }
    // Add the last label
    encoded_name.push_back(domain.size() - start);
    for (size_t i = start; i < domain.size(); ++i) {
        encoded_name.push_back(domain[i]);
    }
    encoded_name.push_back(0); // Null byte to terminate the domain
    return encoded_name;
}

// DNS Server
int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int udpSocket;
    struct sockaddr_in clientAddress;
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    sockaddr_in serverAddress = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(udpSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) != 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true) {
        int bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            break;
        }

        size_t offset = 0;

        // Parse the header
        DNS_Header* header = reinterpret_cast<DNS_Header*>(buffer);
        header->ID = ntohs(header->ID);
        header->QDCOUNT = ntohs(header->QDCOUNT);

        // Parse the question
        offset = sizeof(DNS_Header);
        std::string domain_name = parse_domain_name(reinterpret_cast<uint8_t*>(buffer), offset);

        uint16_t qtype = ntohs(*reinterpret_cast<uint16_t*>(&buffer[offset]));
        offset += 2;
        uint16_t qclass = ntohs(*reinterpret_cast<uint16_t*>(&buffer[offset]));
        offset += 2;

        std::cout << "Received query for: " << domain_name << ", Type: " << qtype << ", Class: " << qclass << std::endl;

        // Prepare the response
        std::vector<uint8_t> response;

        // Header
        DNS_Header response_header = *header;
        response_header.FLAGS = to_network_order_16(0x8180); // Standard query response, no error
        response_header.QDCOUNT = to_network_order_16(1); // 1 question
        response_header.ANCOUNT = to_network_order_16(1); // 1 answer
        response_header.NSCOUNT = 0;
        response_header.ARCOUNT = 0;

        response.insert(response.end(), reinterpret_cast<uint8_t*>(&response_header), reinterpret_cast<uint8_t*>(&response_header) + sizeof(response_header));

        // Question
        auto encoded_name = encode_domain_name(domain_name);
        response.insert(response.end(), encoded_name.begin(), encoded_name.end());
        response.push_back(0x00); // Type: A
        response.push_back(0x01);
        response.push_back(0x00); // Class: IN
        response.push_back(0x01);

        // Answer
        response.insert(response.end(), encoded_name.begin(), encoded_name.end());
        response.push_back(0x00); // Type: A
        response.push_back(0x01);
        response.push_back(0x00); // Class: IN
        response.push_back(0x01);
        response.push_back(0x00); // TTL: 60
        response.push_back(0x00);
        response.push_back(0x00);
        response.push_back(0x3C);
        response.push_back(0x00); // Data length: 4
        response.push_back(0x04);
        response.push_back(0x08); // Data: 8.8.8.8
        response.push_back(0x08);
        response.push_back(0x08);
        response.push_back(0x08);

        // Send the response
        if (sendto(udpSocket, response.data(), response.size(), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), clientAddrLen) == -1) {
            perror("Failed to send response");
        }
    }

    close(udpSocket);
    return 0;
}
