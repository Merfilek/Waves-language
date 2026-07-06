#include "WavesBridge.h"

#include <iostream>
#include <string>
#include <termios.h>
#include <unistd.h>

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
}