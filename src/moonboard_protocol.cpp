#include "moonboard_protocol.h"

#include <cstdlib>

namespace
{
bool isHoldType(char value)
{
    switch (value)
    {
    case 'E':
    case 'F':
    case 'L':
    case 'M':
    case 'P':
    case 'R':
    case 'S':
        return true;
    default:
        return false;
    }
}

std::string columnName(uint16_t column)
{
    std::string result;
    uint16_t value = column + 1;
    while (value > 0)
    {
        const uint8_t remainder = (value - 1) % 26;
        result.insert(result.begin(), static_cast<char>('A' + remainder));
        value = (value - 1) / 26;
    }
    return result;
}
} // namespace

bool MoonboardProtocolParser::feed(char value, ProtocolMessage &message)
{
    if (value == '~')
    {
        configurationStarted_ = true;
        configuration_ = "~";
        return false;
    }

    if (configurationStarted_)
    {
        configuration_ += value;
        if (value == '*')
        {
            message.type = ProtocolMessageType::Configuration;
            message.payload = configuration_;
            configuration_.clear();
            configurationStarted_ = false;
            return true;
        }
        return false;
    }

    if (value == '#')
    {
        if (!problemStarted_)
        {
            problemStarted_ = true;
            problem_.clear();
            return false;
        }

        message.type = ProtocolMessageType::Problem;
        message.payload = problem_;
        problem_.clear();
        problemStarted_ = false;
        return true;
    }

    if (problemStarted_)
        problem_ += value;

    return false;
}

bool parseProblem(
    const std::string &message,
    std::vector<MoonboardHold> &holds)
{
    holds.clear();
    size_t start = 0;

    while (start <= message.size())
    {
        const size_t comma = message.find(',', start);
        const size_t end = comma == std::string::npos ? message.size() : comma;
        const std::string token = message.substr(start, end - start);

        if (token.size() < 2 || !isHoldType(token[0]))
            return false;

        char *numberEnd = nullptr;
        const long position = std::strtol(token.c_str() + 1, &numberEnd, 10);
        if (
            numberEnd == token.c_str() + 1 ||
            *numberEnd != '\0' ||
            position < 0 ||
            position > 65535)
        {
            return false;
        }

        holds.push_back(
            {token[0], static_cast<uint16_t>(position)});

        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }

    return !holds.empty();
}

int aboveHoldPosition(uint16_t position, uint8_t rows)
{
    if (rows == 0)
        return -1;

    const uint16_t column = position / rows;
    const uint8_t positionInColumn = position % rows;

    if (column % 2 == 0)
    {
        if (positionInColumn == rows - 1)
            return -1;
        return position + 1;
    }

    if (positionInColumn == 0)
        return -1;
    return position - 1;
}

std::string positionToCoordinates(uint16_t position, uint8_t rows)
{
    if (rows == 0)
        return "?";

    const uint16_t column = position / rows;
    const uint8_t positionInColumn = position % rows;
    const uint8_t row = column % 2 == 0
        ? positionInColumn + 1
        : rows - positionInColumn;
    return columnName(column) + std::to_string(row);
}
