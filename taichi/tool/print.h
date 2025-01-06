#ifndef TOOL_PRINT_H
#define TOOL_PRINT_H

#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <ctime>

const bool G_ENABLE_OUTPUT = true;

enum class pType
{
    DEBUG = 1,
    MESSAGE = 2,
    WARNING = 3,
    ERROR = 4
};

class Out
{
public:
    Out() = default;
    ~Out() = default;

private:
    static void printTime()
    {
        time_t nowTime = time(nullptr);
        tm* ltm = localtime(&nowTime);

        printf("[%02d:%02d:%02d]", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    }

public:
    static pType logLevel;

public:
    static void Log(pType Type, const char* Format, ...)
    {
        if (!G_ENABLE_OUTPUT) return;
        if(Type < logLevel) return;

        printTime(); printf(" ");
        switch (Type)
        {
        case pType::DEBUG:
            printf("[ DEBUG ]");
            break;
        case pType::MESSAGE:
            printf("[MESSAGE]");
            break;
        case pType::WARNING:
            printf("[WARNING]");
            break;
        case pType::ERROR:
            printf("[ ERROR ]");
            break;
        }
        printf(" >> ");

        va_list args;

        va_start(args, Format);
        vprintf(Format, args);
        va_end(args);

        puts("");
    }
};

#ifdef TOOL_PRINT_H_DATA
pType Out::logLevel = pType::DEBUG;
#endif

#endif