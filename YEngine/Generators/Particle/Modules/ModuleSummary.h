#pragma once
#include <cstdio>
#include <string>

/// モジュール一覧の一行要約（IParticleModule::GetSummary）を作るための書式ヘルパー
template <class... Args>
inline std::string FormatSummary(const char* fmt, Args... args) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), fmt, args...);
    return buf;
}
