#pragma once

#include <iostream>

#ifdef _WIN32

#include <Windows.h>

#endif

#define COLOR_BLACK 0
#define COLOR_DARK_BLUE 1
#define COLOR_DARK_GREEN 2
#define COLOR_LIGHT_BLUE 3
#define COLOR_DARK_RED 4
#define COLOR_MAGENTA 5
#define COLOR_ORANGE 6
#define COLOR_LIGHT_GRAY 7
#define COLOR_GRAY 8
#define COLOR_BLUE 9
#define COLOR_GREEN 10
#define COLOR_CYAN 11
#define COLOR_RED 12
#define COLOR_PINK 13
#define COLOR_YELLOW 14
#define COLOR_WHITE 15

template <class T> void material_print(T text, const int text_color, const int background_color) {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBuffInfo;
    WORD default_colors = 0;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleScreenBuffInfo))
        default_colors = consoleScreenBuffInfo.wAttributes;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), background_color << 4 | text_color);
#elif defined(__linux__)
    std::string t, b;
    switch (text_color) {
    case 0: t = "30"; break;  // color_black      0
    case 1: t = "34"; break;  // color_dark_blue  1
    case 2: t = "32"; break;  // color_dark_green 2
    case 3: t = "36"; break;  // color_light_blue 3
    case 4: t = "31"; break;  // color_dark_red   4
    case 5: t = "35"; break;  // color_magenta    5
    case 6: t = "33"; break;  // color_orange     6
    case 7: t = "37"; break;  // color_light_gray 7
    case 8: t = "90"; break;  // color_gray       8
    case 9: t = "94"; break;  // color_blue       9
    case 10: t = "92"; break; // color_green     10
    case 11: t = "96"; break; // color_cyan      11
    case 12: t = "91"; break; // color_red       12
    case 13: t = "95"; break; // color_pink      13
    case 14: t = "93"; break; // color_yellow    14
    case 15: t = "97"; break; // color_white     15
    default: t = "97";
    }
    switch (background_color) {
    case 0: b = "40"; break;   // color_black      0
    case 1: b = "44"; break;   // color_dark_blue  1
    case 2: b = "42"; break;   // color_dark_green 2
    case 3: b = "46"; break;   // color_light_blue 3
    case 4: b = "41"; break;   // color_dark_red   4
    case 5: b = "45"; break;   // color_magenta    5
    case 6: b = "43"; break;   // color_orange     6
    case 7: b = "47"; break;   // color_light_gray 7
    case 8: b = "100"; break;  // color_gray       8
    case 9: b = "104"; break;  // color_blue       9
    case 10: b = "102"; break; // color_green     10
    case 11: b = "106"; break; // color_cyan      11
    case 12: b = "101"; break; // color_red       12
    case 13: b = "105"; break; // color_pink      13
    case 14: b = "103"; break; // color_yellow    14
    case 15: b = "107"; break; // color_white     15
    default: b = "40";
    }
    std::cout << "\033[" + t + ";" + b + "m";
#endif
    std::cout << text;
#if defined(_WIN32)
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), default_colors);
#elif defined(__linux__)
    std::cout << "\033[0m"; // reset color
#endif
}

template <class T> void material_println(T text, const int text_color, const int background_color) {
    material_print(text, text_color, background_color);
    std::cout << '\n';
}
