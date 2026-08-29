#pragma once

#include <stdint.h>

#include <string>
#include <vector>

enum class ProtocolMessageType
{
    Configuration,
    Problem,
};

struct ProtocolMessage
{
    ProtocolMessageType type;
    std::string payload;
};

struct MoonboardHold
{
    char type;
    uint16_t position;
};

class MoonboardProtocolParser
{
public:
    bool feed(char value, ProtocolMessage &message);

private:
    bool configurationStarted_ = false;
    bool problemStarted_ = false;
    std::string configuration_;
    std::string problem_;
};

bool parseProblem(
    const std::string &message,
    std::vector<MoonboardHold> &holds);

int aboveHoldPosition(uint16_t position, uint8_t rows);
std::string positionToCoordinates(uint16_t position, uint8_t rows);
