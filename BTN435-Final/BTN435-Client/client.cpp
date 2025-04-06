#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080
char buffer[1024];

struct Device {
    std::string type; // "light", "camera", "thermostat"
    int id; // Unique for each device
    std::string status; // "on", "off", or temperature for thermostats

    void print() const {
        std::cout << "[" << type << " #" << id << "] - Status: " << status << "\n";
    }
};

std::vector<Device> devices;

void printDevices() {
    std::cout << "\n--- Device List ---\n";
    for (const auto& device : devices) {
        device.print();
    }
    std::cout << "-------------------\n";
}

void updateDevice(SOCKET& sock) {
    int choiceID;
    std::string choiceType, status;

    std::cout << "\nEnter device type (light, camera, thermostat): ";
    std::cin >> choiceType;
    std::cout << "Enter device ID: ";
    std::cin >> choiceID;

    bool found = false;
    for (auto& device : devices) {
        if (device.type == choiceType && device.id == choiceID) {
            std::cout << "Selected Device:\n";
            device.print();
            if (device.type == "thermostat") {
                std::cout << "Enter Temperator (20, 25): ";
                std::cin >> status;
            }
            else {
                std::cout << "Enter Power Status (on, off): ";
                std::cin >> status;
            }
            
            std::string updateRequest = "GET/" + device.type + "/" + std::to_string(device.id) + "/" + status;
            send(sock, updateRequest.c_str(), updateRequest.length(), 0);

            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::string response(buffer);
                std::cout << buffer << std::endl;
                if (strncmp(buffer, "200", 3) == 0) {
                    device.status = status;
                }
            }
            found = true;
            break;
        }
    }

    if (!found) {
        std::cout << "Device not found.\n";
    }
}

void showMenu(SOCKET& sock) {
    int choice;

    do {
        std::cout << "\n===== Smart Home CLI =====\n";
        std::cout << "1. View Devices\n";
        std::cout << "2. Update a Device\n";
        std::cout << "3. Exit\n";
        std::cout << "Choose an option: ";
        std::cin >> choice;

        switch (choice) {
        case 1:
            printDevices();
            break;
        case 2:
            updateDevice(sock);
            break;
        case 3:
            std::cout << "Exiting...\n";
            break;
        default:
            std::cout << "Invalid option. Try again.\n";
        }

    } while (choice != 3);
}

Device deserialize(const std::string& str) {
    std::stringstream ss(str);
    std::string type, idStr, status;
    std::getline(ss, type, ',');
    std::getline(ss, idStr, ',');
    std::getline(ss, status, ',');

    return Device{ type, std::stoi(idStr), status };
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    sockaddr_in serverAddr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Server address setup
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect to server
    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Get Devices from Server
    std::string request = "GET/devices";
    send(sock, request.c_str(), request.length(), 0);

    int exitCode = 1;
    while (exitCode) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string response(buffer);

            if (response == "end") {
                exitCode = 0; // exit - all devices received
                break;
            }

            //std::cout << "[DEBUG] Device Added: " << response << std::endl;

            devices.push_back(deserialize(response));
        }
    }

    showMenu(sock);
    
    std::string exitRequest = "GET/exit";
    send(sock, exitRequest.c_str(), exitRequest.length(), 0);

    closesocket(sock);
    WSACleanup();
    return 0;
}
