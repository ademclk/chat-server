#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <ctime>
#include <fstream>
#include "crc.h"

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "10000"
#define DEFAULT_BUFLEN 512

struct ClientInfo
{
    SOCKET socket;
    std::string name;
};

std::unordered_map<std::string, ClientInfo> clients;
std::mutex clients_mutex;

void log_with_timestamp(std::string message)
{

    // get current time and date and apply formatting
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", ltm);

    // open log file
    std::ofstream log_file;
    log_file.open("log.txt", std::ios_base::app);

    // write to log file
    log_file << timestamp << " " << message << std::endl;

    // close log file
    log_file.close();
}

std::string GenerateErrorCheckingBits(const std::string &message)
{
    // Calculate the checksum of the message
    unsigned long checksum = 0;
    for (char c : message)
    {
        checksum += c;
    }

    // Convert the checksum to a hexadecimal string
    std::stringstream stream;
    stream << std::hex << checksum;
    std::string checksum_str = stream.str();

    // Return the last 8 characters of the hexadecimal string as the error checking bits
    return checksum_str.substr(checksum_str.length() - 8);
}

void ForwardPrivateMessage(std::string message)
{
    size_t separator = message.find("|");
    std::string recipient = message.substr(0, separator);
    message = message.substr(separator + 1);

    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].name == recipient)
        {
            // send the private message to the intended recipient
            send(clients[i].sock, message.c_str(), message.length(), 0);
            break;
        }
    }
}

void ServiceClient(SOCKET client_socket, std::string client_name)
{
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    while (true)
    {
        int bytes_received = recv(client_socket, recvbuf, recvbuflen, 0);
        if (bytes_received <= 0)
        {
            // Client disconnected or error occurred, remove client and exit thread
            std::lock_guard<std::mutex> guard(clients_mutex);
            clients.erase(client_name);
            break;
        }

        std::string message(recvbuf, bytes_received);

        // Parse message and handle command
        size_t separator_pos = message.find('|');
        if (separator_pos == std::string::npos)
        {
            std::cerr << "Invalid message received from client " << client_name << std::endl;
            continue;
        }

        std::string command = message.substr(0, separator_pos);
        if (command == "CONN")
        {
            // Add client to list
            std::lock_guard<std::mutex> guard(clients_mutex);
            clients[client_name] = {client_socket, client_name};

            // Send list of current clients to new client
            std::string client_list;
            for (const auto &client_info : clients)
            {
                const auto &name = client_info.first;
                client_list += name + '|';
            }
            send(client_socket, client_list.c_str(), client_list.length(), 0);

            // Send notification to all other clients about new client
            for (const auto &client_info : clients)
            {
                const auto &name = client_info.first;
                const auto &info = client_info.second;
                if (name == client_name)
                    continue;
                std::string notification = "NEW|" + client_name;
                send(info.socket, notification.c_str(), notification.length(), 0);
            }

            log_with_timestamp(client_name);
        }
        else if (command == "MESG")
        {
            std::string recipient = message.substr(separator_pos + 1);
            separator_pos = recipient.find('|');
            if (separator_pos == std::string::npos)
            {
                std::cerr << "Invalid MESG command received from client " << client_name << std::endl;
                continue;
            }
            std::string message_text = recipient.substr(separator_pos + 1);

            separator_index = message.find("|");
            receiver = message.substr(0, separator_index);
            message = message.substr(separator_index + 1);

            // service_client() function
            unsigned int crc = CalculateCRC(message);

            // attach the CRC value to the message
            string complete_message = message + "|" + to_string(crc);

            // forward the message to the appropriate recipients
            for (const auto &client : clients)
            {
                if (client.name == receiver)
                {
                    send(client.sock, complete_message.c_str(), complete_message.length(), 0);
                    break;
                }
            }

            if (receiver != "")
            {
                std::lock_guard<std::mutex> guard(clients_mutex);
                auto recipient_iter = clients.find(recipient_name);
                if (recipient_iter == clients.end())
                {
                    std::cerr << "Recipient " << recipient_name << " not found for client " << client_name << std::endl;
                    continue;
                }

                ForwardPrivateMessage(receiver + "|" + message);
            }
            else
            {
                // Send message to recipient
                std::string send_message = "MESG|" + client_name + "|" + message_text;
                send(recipient_iter->second.socket, send_message.c_str(), send_message.length(), 0);

                log_with_timestamp(recipient);
                log_with_timestamp(message_text);

                // Send message to sender (echo)
                send(client_socket, send_message.c_str(), send_message.length(), 0);

                // Add error checking bits to message and send it
                message_text = "MESG|" + message_text + "|" + GenerateErrorCheckingBits(message_text);
                send(recipient_iter->second.socket, message_text.c_str(), message_text.length(), 0);
            }
        }

        else if (command == "MERR")
        {
            std::string recipient = message.substr(separator_pos + 1);
            separator_pos = recipient.find('|');
            if (separator_pos == std::string::npos)
            {
                std::cerr << "Invalid MERR command received from client " << client_name << std::endl;
                continue;
            }

            std::string recipient_name = recipient.substr(0, separator_pos);
            std::string message_text = recipient.substr(separator_pos + 1);

            std::lock_guard<std::mutex> guard(clients_mutex);
            auto recipient_iter = clients.find(recipient_name);
            if (recipient_iter == clients.end())
            {
                std::cerr << "Recipient " << recipient_name << " not found for client " << client_name << std::endl;
                continue;
            }

            // Resend last message without errors
            message_text = "MESG|" + message_text;

            log_with_timestamp(message_text);

            send(recipient_iter->second.socket, message_text.c_str(), message_text.length(), 0);
        }
        else if (command == "GONE")
        {
            // Remove client from list and notify other clients
            std::lock_guard<std::mutex> guard(clients_mutex);
            clients.erase(client_name);

            log_with_timestamp(client_name + " disconnected");

            for (const auto &client_info : clients)
            {
                const auto &name = client_info.first;
                const auto &info = client_info.second;
                std::string notification = "GONE|" + client_name;
                send(info.socket, notification.c_str(), notification.length(), 0);
            }
            break;
        }
        else
        {
            std::cerr << "Invalid command received from client " << client_name << std::endl;
        }
    }
}

int main()
{
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *addrinfo_result = nullptr;
    int getaddrinfo_result = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &addrinfo_result);
    if (getaddrinfo_result != 0)
    {
        std::cerr << "getaddrinfo failed: " << gai_strerror(getaddrinfo_result) << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listen_socket = INVALID_SOCKET;
    listen_socket = socket(addrinfo_result->ai_family, addrinfo_result->ai_socktype, addrinfo_result->ai_protocol);
    if (listen_socket == INVALID_SOCKET)
    {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        WSACleanup();
        return 1;
    }

    int bind_result = bind(listen_socket, addrinfo_result->ai_addr, (int)addrinfo_result->ai_addrlen);
    if (bind_result == SOCKET_ERROR)
    {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrinfo_result);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for client connections..." << std::endl;

    while (true)
    {
        SOCKET client_socket = INVALID_SOCKET;
        client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }

        std::cout << "Client connected" << std::endl;

        // Declare a buffer to hold the client's name
        char name_buffer[DEFAULT_BUFLEN];

        // Receive the client's name from the socket
        int bytes_received = recv(client_socket, name_buffer, DEFAULT_BUFLEN, 0);

        // Convert the received bytes to a string
        std::string client_name(name_buffer, bytes_received);

        // Print the received name to the console
        std::cout << "Received name from client: " << client_name << std::endl;

        std::thread client_thread(ServiceClient, client_socket, client_name);
        client_thread.detach();
    }

    WSACleanup();
    return 0;
}
