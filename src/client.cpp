#include "client.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <limits>

#define SERVER_IP "127.0.0.1"
#define PACKET_SIZE sizeof(RawPacket)
#define SOCKET_TIMEOUT_SEC 2  // Timeout for socket operations (seconds)

// Function definition for creating a request for all stream packets
std::vector<uint8_t> Client::createStreamAllPacketsRequest() {
    return {1, 0};  // Assuming a basic request format, modify as needed
}

// Function definition for creating a request to resend a specific packet by its sequence number
std::vector<uint8_t> Client::createResendPacketRequest(int seq) {
    return {2, static_cast<uint8_t>(seq)};  // Assuming a basic request format, modify as needed
}

// Utility function to set socket timeout
bool setSocketTimeout(int sock, int timeoutSec) {
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    return true;
}

// Log error to a log file
void Client::logError(const std::string& errorMsg) {
    std::ofstream errorLog("ErrorLog.log", std::ios::out | std::ios::app);
    if (errorLog.is_open()) {
        errorLog << "ERROR: " << errorMsg << " | Time: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << '\n';
        errorLog.close();
    } else {
        std::cerr << "Failed to write to error log file\n";
    }
}

// Validate the packet fields
bool validatePacket(const RawPacket* raw) {
    // Validate symbol: must be a 4-byte string
    if (raw->symbol[0] == '\0') {
        return false;
    }

    // Validate sequence number (should be positive)
    if (ntohl(raw->sequence) <= 0) {
        return false;
    }

    // Validate price and quantity (should be positive)
    if (ntohl(raw->price) <= 0 || ntohl(raw->quantity) <= 0) {
        return false;
    }

    return true;
}

void Client::start(int port) {
    std::map<int, StreamData> orderedData;
    std::vector<int> missedSeq;
    int expectedSeq = 1;

    std::ofstream logFile("StreamData.log", std::ios::out | std::ios::trunc);
    if (!logFile.is_open()) {
        logError("Failed to open StreamData.log");
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Socket creation failed");
        return;
    }

    // Set socket timeout
    if (!setSocketTimeout(sock, SOCKET_TIMEOUT_SEC)) {
        logError("Socket timeout setup failed");
        close(sock);
        return;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        logError("Invalid address");
        close(sock);
        return;
    }

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logError("Connection failed");
        close(sock);
        return;
    }

    std::vector<uint8_t> request = createStreamAllPacketsRequest();
    if (send(sock, request.data(), request.size(), 0) < 0) {
        logError("Failed to send data to server");
        close(sock);
        return;
    }

    uint8_t buffer[1024];
    std::vector<uint8_t> leftover;

    while (true) {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);

        // Handle disconnections or errors
        if (bytes <= 0) {
            if (bytes == 0) {
                logError("Server disconnected");
            } else {
                logError("Error receiving data");
            }
            break;
        }

        leftover.insert(leftover.end(), buffer, buffer + bytes);

        while (leftover.size() >= PACKET_SIZE) {
            const RawPacket* raw = reinterpret_cast<const RawPacket*>(leftover.data());

            // Validate the packet
            if (!validatePacket(raw)) {
                logError("Invalid packet received, ignoring.");
                leftover.erase(leftover.begin(), leftover.begin() + PACKET_SIZE);
                continue;
            }

            StreamData data;
            data.symbol = std::string(raw->symbol, 4);
            data.side = raw->side;
            data.quantity = ntohl(raw->quantity);
            data.price = ntohl(raw->price);
            data.sequence = ntohl(raw->sequence);

            // Sequence integrity: Check if sequence numbers are in order
            while (expectedSeq < data.sequence) {
                missedSeq.push_back(expectedSeq++);
            }
            expectedSeq = data.sequence + 1;

            orderedData[data.sequence] = data;

            leftover.erase(leftover.begin(), leftover.begin() + PACKET_SIZE);
        }
    }

    close(sock);

    // Handle missed sequence numbers
    if (!missedSeq.empty()) {
        std::ofstream backFile("BackLogger.log", std::ios::out | std::ios::trunc);
        if (!backFile.is_open()) {
            logError("Failed to open BackLogger.log");
            return;
        }

        for (int seq : missedSeq) {
            int retrySock = socket(AF_INET, SOCK_STREAM, 0);
            if (retrySock < 0) continue;

            if (connect(retrySock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                logError("Reconnect failed for seq: " + std::to_string(seq));
                close(retrySock);
                continue;
            }

            std::vector<uint8_t> resendReq = createResendPacketRequest(seq);
            if (send(retrySock, resendReq.data(), resendReq.size(), 0) < 0) {
                logError("Failed to send resend request for seq: " + std::to_string(seq));
                close(retrySock);
                continue;
            }

            RawPacket raw;
            int bytes = recv(retrySock, &raw, PACKET_SIZE, MSG_WAITALL);
            close(retrySock);

            if (bytes == PACKET_SIZE) {
                // Validate recovered packet
                if (!validatePacket(&raw)) {
                    logError("Invalid recovered packet for seq: " + std::to_string(seq));
                    continue;
                }

                StreamData data;
                data.symbol = std::string(raw.symbol, 4);
                data.side = raw.side;
                data.quantity = ntohl(raw.quantity);
                data.price = ntohl(raw.price);
                data.sequence = ntohl(raw.sequence);

                backFile << "Recovered - Symbol: " << data.symbol
                         << ", Side: " << data.side
                         << ", Qty: " << data.quantity
                         << ", Price: " << data.price
                         << ", Seq: " << data.sequence << '\n';

                orderedData[data.sequence] = data;
            } else {
                logError("Failed to recover seq: " + std::to_string(seq));
            }
        }

        backFile.close();
    }

    // Write recovered data to log file
    for (const auto& [seq, data] : orderedData) {
        logFile << "Symbol: " << data.symbol
                << ", Side: " << data.side
                << ", Qty: " << data.quantity
                << ", Price: " << data.price
                << ", Seq: " << data.sequence << '\n';
    }

    logFile.close();
    // Print completion messages
    std::cout << "Processed the data\n";
    std::cout << "Created StreamData.log file of processed data\n";
    if (!missedSeq.empty()) {
        std::cout << "Created BackLogger.log file for missed sequences\n";
    }
}
