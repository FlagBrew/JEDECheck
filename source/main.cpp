/*
*   This file is part of JEDECheck
*   Copyright (C) 2017-2018 Bernardo Giordano
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <3ds.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include "spi.hpp"

static const std::string INFO  = "[ INFO] ";
static const std::string ERROR = "[ERROR] ";
static const std::string YUM   = "[  YUM] ";

static const std::vector<u32> knownJEDECs = {
    0x204012, 0x621600, 0x204013, 0x621100,
    0x204014, 0x202017, 0x204017, 0x208013
};

static std::string format(const std::string fmt_str, ...)
{
    va_list ap;
    char *fp = NULL;
    va_start(ap, fmt_str);
    vasprintf(&fp, fmt_str.c_str(), ap);
    va_end(ap);
    std::unique_ptr<char[]> formatted(fp);
    return std::string(formatted.get());
}

static std::string dateTime(void)
{
    time_t unixTime = time(NULL);
    struct tm* timeStruct = gmtime((const time_t*)&unixTime);
    return format("%04i%02i%02i_%02i%02i%02i", 
        timeStruct->tm_year + 1900, timeStruct->tm_mon + 1, timeStruct->tm_mday,
        timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec);
}

int main() {
    bool store = false;
    std::ostringstream o;

    pxiDevInit();
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    o << "JEDECheck version " 
      << VERSION_MAJOR << "."
      << VERSION_MINOR << "."
      << VERSION_MICRO << "\n\n";

    FS_CardType cardType;
    Result res = FSUSER_GetCardType(&cardType);
    if (R_SUCCEEDED(res) && cardType != CARD_CTR)
    {
        u8* headerData = new u8[0x3B4];
        res = FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0LL, headerData);
        if (R_SUCCEEDED(res))
        {
            CardType type;
            char cardTitle[14] = {0};
            char gameCode[6] = {0};

            std::copy(headerData, headerData + 12, cardTitle);
            std::copy(headerData + 12, headerData + 16, gameCode);
            cardTitle[13] = '\0';
            gameCode[5] = '\0';
            o << INFO << "Card title: " << cardTitle << "\n";
            o << INFO << "Game code:  " << gameCode << "\n\n";

            u8 sr = 0;
            u32 jedec = 0;
            int infrared = gameCode[0] == 'I' ? 1 : 0;
            CardType t = (infrared == 1) ? FLASH_INFRARED_DUMMY : FLASH_STD_DUMMY;
            res = SPIReadJEDECIDAndStatusReg(t, &jedec, &sr);
            if (R_SUCCEEDED(res))
            {
                if (std::find(knownJEDECs.begin(), knownJEDECs.end(), jedec) != knownJEDECs.end())
                {
                    o << INFO << "JEDEC ID already documented :(\n";
                }
                else
                {
                    // yum
                    store = true;
                    o << YUM << "JEDEC ID not documented yet! :)\n";
                }
                o << INFO << "Infrared: " << infrared << "\n";
                o << INFO << "JEDEC ID: 0x" << std::hex << jedec << "\n";
            }
            else
            {
                o << ERROR << "Unable to retrieve JEDEC ID.\n";
            }

            res = SPIGetCardType(&type, infrared);
            if (R_SUCCEEDED(res))
            {
                o << INFO << "CardType: " << getCardTypeString(type) << "\n";
            }
            else
            {
                o << ERROR << "Unable to retrieve Card Type.\n";
            }
        }
        else
        {
            o << ERROR << "Failed to get header data.\n\n";
        }
        delete[] headerData;
    }
    else
    {
        o << ERROR << "Couldn't load a DS cartridge correctly.";
    }

    std::cout << o.str() << "\n\n";

    if (store)
    {
        std::string name = "/JEDECheck_" + dateTime() + ".txt";
        std::ofstream f;
        f.open(name);
        f << o.str();
        f.close();

        std::cout << "Report saved in " << name 
            << "\n\nPlease report it as an issue on\nhttps://www.github.com/FlagBrew/JEDECheck";
    }

    std::cout << "\n\nPress START to exit.";
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();

    while (aptMainLoop() && !(hidKeysDown() & KEY_START))
    {
        hidScanInput();
    }

    gfxExit();
    pxiDevExit();
    return 0;
}