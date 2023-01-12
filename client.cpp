#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "10000"
#define DEFAULT_BUFLEN 512

uint32_t CalculateCRC(const std::string &message)
{
    uint32_t crc = 0xFFFFFFFF;
    for (char c : message)
    {
        crc = crc ^ c;
        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x01)
            {
                crc = (crc >> 1) ^ 0x04C11DB7;
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

bool CheckCRC(const std::string &message, uint32_t received_crc)
{
    uint32_t calculated_crc = CalculateCRC(message);
    return received_crc == calculated_crc;
}

void SendMessages(SOCKET socket)
{
    char sendbuf[DEFAULT_BUFLEN];
    int sendbuflen = DEFAULT_BUFLEN;

    while (true)
    {
        std::string recipient;
        std::string message;
        std::cout << "Enter recipient name: ";
        std::getline(std::cin, recipient);
        std::cout << "Enter message: ";
        std::getline(std::cin, message);

        std::string send_message = "MESG|" + recipient + "|" + message;
        send(socket, send_message.c_str(), send_message.length(), 0);
    }
}

void SendPrivateMessage()
{
    std::string recipient;
    std::string message;
    std::cout << "Enter recipient name: ";
    std::getline(std::cin, recipient);
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    std::string send_message = "MESG|" + recipient + "|" + message;
    send(socket, send_message.c_str(), send_message.length(), 0);
}

void ReceiveMessages(SOCKET socket)
{
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    while (true)
    {
        int bytes_received = recv(socket, recvbuf, recvbuflen, 0);
        if (bytes_received <= 0)
        {
            std::cout << "Connection closed" << std::endl;
            break;
        }

        std::string message(recvbuf, bytes_received);

        if (CheckCRC == true)
        {
            // message is valid
            cout << message << endl;
        }
        else
        {
            // message is corrupted
            cout << "Received corrupted message" << endl;
        }
    }
}

int main()
{
    // Registration for client part 1
    std::string name;
    std::cout << "Enter your name: ";
    std::getline(std::cin, name);

    std::string set_name_message = "NAME|" + name;

    WSADATA wsa_data;
    int wsa_startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_startup_result != 0)
    {
        std::cerr << "WSAStartup failed: " << wsa_startup_result << std::endl;
        return 1;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addrinfo_result = nullptr;
    int getaddrinfo_result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &addrinfo_result);
    if (getaddrinfo_result != 0)
    {
        std::cerr << "getaddrinfo failed: " << gai_strerror(getaddrinfo_result) << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET sock = INVALID_SOCKET;
    sock = socket(addrinfo_result->ai_family, addrinfo_result->ai_socktype, addrinfo_result->ai_protocol);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        WSACleanup();
        return 1;
    }

    // Registration of client part 2
    send(sock, set_name_message.c_str(), set_name_message.length(), 0);

    int connect_result = connect(sock, addrinfo_result->ai_addr, (int)addrinfo_result->ai_addrlen);
    if (connect_result == SOCKET_ERROR)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(addrinfo_result);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return 1;
    }

    std::thread receive_thread(ReceiveMessages, sock);
    std::thread send_thread(SendMessages, sock);
    std::thread private_message_thread(SendPrivateMessage, sock);

    receive_thread.join();
    send_thread.join();
    private_message_thread.join();

    closesocket(sock);
    WSACleanup();

    return 0;
}
