#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

class Client {
public:
    void start(int port);
    std::vector<int> missedSeq;
    int expectedSeq = 1;

private:
    std::vector<uint8_t> createStreamAllPacketsRequest();
    std::vector<uint8_t> createResendPacketRequest(int missingSeq);
    void logError(const std::string& errorMsg); // Declaration of logError
};

// Final parsed packet
struct StreamData {
    std::string symbol;
    char side;
    int32_t quantity;
    int32_t price;
    int32_t sequence;
};

// Raw packet exactly as received (packed, fixed size)
#pragma pack(push, 1)
struct RawPacket {
    char symbol[4];
    char side;
    uint32_t quantity;
    uint32_t price;
    uint32_t sequence;
};
#pragma pack(pop)
