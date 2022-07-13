#pragma once

#include <iostream>

#define JOT_LOGGER

namespace jot {

    struct JotLogger {
       
        template<class T>
        JotLogger& operator<< ([[maybe_unused]] T content) {
#ifdef JOT_LOGGER
            std::cout << content;
#endif
            return *this;
        }
    };

    static JotLogger logger;
    #define logger logger << __FILE__ << ":" << __LINE__ << ':'
}

