#pragma once

#include "jot_color.hpp"

#include <unordered_map>

#define JOT_LOGGER

namespace jot {

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

class JotLogger {

  public:
    JotLogger &type(LogKind kind) {
        text_color = colors_map[kind];
        return *this;
    }

    template <class T> JotLogger &operator<<([[maybe_unused]] T content) {
#ifdef JOT_LOGGER
        material_print(content, text_color, background_color);
#endif
        return *this;
    }

  private:
    int text_color = 0;
    int background_color = 0;
};

static JotLogger logger;

#define logi logger.type(jot::LogKind::Info) << __FILE__ << ":" << __LINE__ << " i: "

#define logd logger.type(jot::LogKind::Debug) << __FILE__ << ":" << __LINE__ << " d: "

#define loge logger.type(jot::LogKind::Error) << __FILE__ << ":" << __LINE__ << " e: "
}