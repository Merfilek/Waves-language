#include "WavesBridge.h"

#include <iostream>
#include <string>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#ifndef _WIN32
static void setEcho(bool enabled)
{
    termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    if (enabled)
        tty.c_lflag |= ECHO;
    else
        tty.c_lflag &= ~ECHO;

    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}
#else
static void setEcho(bool enabled)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(h, &mode);
    if (enabled)
        mode |= ENABLE_ECHO_INPUT;
    else
        mode &= ~ENABLE_ECHO_INPUT;
    SetConsoleMode(h, mode);
}
#endif

extern "C" {
    WavesValue waves_Input(int argc, WavesValue* argv)
    {
        std::string input;
        std::getline(std::cin, input);
        return waves_makeStringOwned(input.c_str());
    }

    WavesValue waves_Hidden_Input(int argc, WavesValue* argv)
    {
        std::string input;

        setEcho(false);
        std::getline(std::cin, input);
        setEcho(true);

        std::cout << std::endl;

        return waves_makeStringOwned(input.c_str());
    }

#ifndef _WIN32
    WavesValue waves_Password_Input(int argc, WavesValue* argv)
    {
        std::string input;

        termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;

        newt.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        char ch;
        while (true)
        {
            ch = getchar();

            if (ch == '\n' || ch == '\r')
                break;

            if ((ch == 127 || ch == '\b') && !input.empty())
            {
                input.pop_back();
                std::cout << "\b \b" << std::flush;
                continue;
            }

            input += ch;
            std::cout << '*' << std::flush;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << std::endl;

        return waves_makeStringOwned(input.c_str());
    }
#else
    // _getch() (conio.h) is the direct Windows Console equivalent of the Linux
    // raw-mode getchar() loop above: reads one key immediately, no Enter needed,
    // no echo -- same reason this function needs it instead of plain std::cin,
    // same as the POSIX side needing termios instead of plain std::getline.
    WavesValue waves_Password_Input(int argc, WavesValue* argv)
    {
        std::string input;

        int ch;
        while (true)
        {
            ch = _getch();

            if (ch == '\n' || ch == '\r')
                break;

            if ((ch == 8 || ch == 127) && !input.empty())
            {
                input.pop_back();
                std::cout << "\b \b" << std::flush;
                continue;
            }

            input += (char)ch;
            std::cout << '*' << std::flush;
        }

        std::cout << std::endl;

        return waves_makeStringOwned(input.c_str());
    }
#endif
}