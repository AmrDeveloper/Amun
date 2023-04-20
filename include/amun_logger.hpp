#pragma once

#include "amun_color.hpp"

#include <unordered_map>

#define ENABLE_RA_LOGGER

namespace amun {

enum LogKind {
    Info,
    Debug,
    Error,
};

static std::unordered_map<LogKind, int> colors_map = {
    {LogKind::Info, COLOR_YELLOW},
    {LogKind::Debug, COLOR_BLUE},
    {LogKind::Error, COLOR_RED},
};

class Logger {

  public:
    auto type(LogKind kind) -> Logger&
    {
        text_color = colors_map[kind];
        return *this;
    }

    template <class T>
    auto operator<<([[maybe_unused]] T content) -> Logger&
    {
#ifdef ENABLE_RA_LOGGER
        material_print(content, text_color, background_color);
#endif
        return *this;
    }

  private:
    int text_color = 0;
    int background_color = 0;
};

static Logger logger;

#define logi logger.type(amun::LogKind::Info) << __FILE__ << ":" << __LINE__ << " i: "

#define logd logger.type(amun::LogKind::Debug) << __FILE__ << ":" << __LINE__ << " d: "

#define loge logger.type(amun::LogKind::Error) << __FILE__ << ":" << __LINE__ << " e: "
}