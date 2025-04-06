#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080

struct Device {
    std::string type; // "light", "camera", "thermostat"
    int id; // Unique for each device
    std::string status; // "on", "off", or temperature for thermostats
};

// Storing all devices globally
std::vector<Device> devices;

void initDevices() {
    // 8 lights (default "off")
    for (int i = 1; i <= 8; ++i) {
        devices.push_back(Device{ "light", i, "off" });
    }
    // 8 cameras (default "off")
    for (int i = 1; i <= 8; ++i) {
        devices.push_back(Device{ "camera", i, "off" });
    }
    // 2 thermostats (default 20°C)
    for (int i = 1; i <= 2; ++i) {
        devices.push_back(Device{ "thermostat", i, "20" });
    }
}

int processRequest(const std::string& req, SOCKET clientSocket) {
    std::string method, type, deviceID, status;

    std::cout << "Client Req: " << req << std::endl;

    std::istringstream stream(req);
    std::string token;
    std::vector<std::string> reqTokens;

    while (std::getline(stream, token, '/')) {
        if (!token.empty()) {
            reqTokens.push_back(token);
        }
    }

    if (reqTokens.size() == 4) {
        method = reqTokens[0];
        type = reqTokens[1];
        deviceID = reqTokens[2];
        status = reqTokens[3];
    }
    else if (reqTokens.size() == 2) {
        method = reqTokens[0];
        type = reqTokens[1];
    }
    else {
        std::string reply = "400 Bad Request - Invalid parameters in request\n";
        std::cout << reply;
        send(clientSocket, reply.c_str(), reply.size(), 0);
        return 0;
    }

    if (type == "devices") {
        for (const auto& d : devices) {
            std::ostringstream oss;
            oss << d.type << "," << d.id << "," << d.status;
            std::string serialized = oss.str();
            send(clientSocket, serialized.c_str(), serialized.size(), 0);
            Sleep(50); // prevent data clumping
        }
        send(clientSocket, "end", 3, 0);
        return 1;
    }

    if (type == "exit") {
       
        return 3;
    }

    for (auto& device : devices) {
        if (device.type == type && device.id == std::stoi(deviceID)) {
            device.status = status;

            std::ostringstream oss;
            oss << "200 OK - " << type << " #" << deviceID << " is now " << status << "\n";
            std::string reply = oss.str();
            std::cout << reply;
            send(clientSocket, reply.c_str(), reply.size(), 0);
            return 1;
        }
    }

    std::string notFound = "404 Not Found - Device does not exist\n";
    std::cout << notFound;
    send(clientSocket, notFound.c_str(), notFound.size(), 0);
    return 0;
}



void handleClient(SOCKET clientSocket) {
    int exitCode = 0;
    while (exitCode != 3) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';

            exitCode = processRequest(buffer, clientSocket);
        }
    }
    std::cout << "Closing Connection" << std::endl;
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket;
    sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);



    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Bind socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Create Devices
    initDevices();

    // Printing Devices
    //for (auto& device : devices) {
    //    std::cout << "[DEBUG] Device: " << device.type << " #" << device.id << " status: " << device.status << "\n";
    //}

    std::cout << "Server listening on port " << PORT << "...\n";

    // Accept multiple clients in a loop
    std::vector<std::thread> clientThreads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed.\n";
            continue;
        }

        std::cout << "New client connected.\n";
        clientThreads.emplace_back(handleClient, clientSocket);
    }

    // Clean up (won't be reached unless loop is broken)
    for (auto& threads : clientThreads) {
        if (threads.joinable()) threads.join();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
