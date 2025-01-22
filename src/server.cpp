#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bitset>
#include <vector>
struct dns_header
{
    int16_t ID;
    int16_t FLAGS;
    int16_t QDCOUNT;
    int16_t ANCOUNT;
    int16_t NSCOUNT;
    int16_t ARCOUNT;
    ssize_t Parse(const char *buffer, ssize_t length)
    {
        if (length < 12)
        {
            return 0;
        }
        ID = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[0]));
        FLAGS = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[2]));
        QDCOUNT = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[4]));
        ANCOUNT = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[6]));
        NSCOUNT = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[8]));
        ARCOUNT = ntohs(*reinterpret_cast<const uint16_t *>(&buffer[10]));
        return 12;
    }
    std::string ToBuffer() const
    {
        std::string buffer;
        buffer.reserve(12);
        int16_t hostnumber = htons(ID);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(FLAGS);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(QDCOUNT);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(ANCOUNT);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(NSCOUNT);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(ARCOUNT);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        return buffer;
    }
};
struct dns_query
{
    std::vector<std::string> names;
    int16_t qtype;
    int16_t qclass;
    ssize_t Parse(const char *buffer, ssize_t length)
    {
        if (length < 12)
        {
            return 0;
        }
        int i = 12;
        for (; i < length; ++i)
        {
            if (buffer[i] == 0)
            {
                break;
            }
            int name_length = buffer[i];
            std::string name(&buffer[i + 1], name_length);
            names.push_back(name);
            i += name_length;
        }
        qtype = ntohs(*reinterpret_cast<const int16_t *>(&buffer[i + 1]));
        qclass = ntohs(*reinterpret_cast<const int16_t *>(&buffer[i + 3]));
        return 12;
    }
    std::string ToBuffer() const
    {
        std::string buffer;
        for (auto name : names)
        {
            buffer.append(std::string(1, name.size()));
            buffer.append(name);
        }
        buffer.append(std::string(1, 0));
        int16_t hostnumber = htons(qtype);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(qclass);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        return buffer;
    }
};
struct dns_answer
{
    std::vector<std::string> names;
    int16_t atype;
    int16_t aclass;
    int32_t TTL;
    int16_t length;
    int32_t data;
    std::string ToBuffer() const
    {
        std::string buffer;
        for (auto name : names)
        {
            buffer.append(std::string(1, name.size()));
            buffer.append(name);
        }
        buffer.append(std::string(1, 0));
        int16_t hostnumber = htons(atype);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber = htons(aclass);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        int32_t hostnumber2 = htonl(TTL);
        buffer.append(reinterpret_cast<const char *>(&hostnumber2), 4);
        hostnumber = htons(length);
        buffer.append(reinterpret_cast<const char *>(&hostnumber), 2);
        hostnumber2 = htonl(data);
        buffer.append(reinterpret_cast<const char *>(&hostnumber2), 4);
        return buffer;
    }
};
class DNSMessage
{
public:
    DNSMessage(const char *buffer, ssize_t length)
    {
        auto result = header.Parse(buffer, length);
        if (result == 0)
        {
            error = true;
            return;
        }
        result = query.Parse(buffer, length);
        if (result == 0)
        {
            error = true;
            return;
        }
    }
    std::string ToBuffer()
    {
        header.FLAGS |= (1 << 15);
        header.QDCOUNT = 1;
        header.ANCOUNT = 1;
        query.qtype = 1;
        query.qclass = 1;
        answer.names = query.names;
        answer.atype = 1;
        answer.aclass = 1;
        answer.TTL = 60;
        answer.length = 4;
        answer.data = 0x08080808;
        std::string buffer;
        buffer.append(header.ToBuffer());
        buffer.append(query.ToBuffer());
        buffer.append(answer.ToBuffer());
        return buffer;
    }
private:
    dns_header header;
    dns_query query;
    dns_answer answer;
    bool error;
};
void system_info()
{
    unsigned int i = 1;
    char *c = (char *)&i;
    if (*c)
        printf("Little endian\n");
    else
        printf("Big endian\n");
}
int main()
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    // Disable output buffering
    setbuf(stdout, NULL);
    system_info();
    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!" << std::endl;
    // Uncomment this block to pass the first stage
    int udpSocket;
    struct sockaddr_in clientAddress;
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1)
    {
        std::cerr << "Socket creation failed: " << strerror(errno) << "..." << std::endl;
        return 1;
    }
    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "SO_REUSEPORT failed: " << strerror(errno) << std::endl;
        return 1;
    }
    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };
    if (bind(udpSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0)
    {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }
    int bytesRead;
    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);
    while (true)
    {
        // Receive data
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1)
        {
            perror("Error receiving data");
            break;
        }
        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;
        DNSMessage message(buffer, bytesRead);
        auto response = message.ToBuffer();
        std::cout << "Response size: " << response.size() << std::endl;
        std::cout << "Response size: " << response.size() << std::endl;
        if (sendto(udpSocket, response.c_str(), response.size(), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
        {
            perror("Failed to send response");
        }
    }
    close(udpSocket);
    return 0;
}