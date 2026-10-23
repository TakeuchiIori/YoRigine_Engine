#include "Localization.h"

#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>

#include <json.hpp>
#include "Debugger/Logger.h"

namespace YoRigine::loc {
namespace {

Language    g_lang = Language::Japanese;
std::string g_langDir;
bool        g_enLoaded = false;

// 元の文字列 ("##" より前) → 訳。
// unordered_map の値はリハッシュでも動かないので、返した c_str() は表を読み直すまで生きる。
std::unordered_map<std::string, std::string> g_en;
// "##" 付きの元の文字列 → «訳 + ##…» をつないだもの (毎フレームつながないよう覚えておく)
std::unordered_map<std::string, std::string> g_joined;
// 元の名前 → «訳###名前» (言語ごと)
std::unordered_map<std::string, std::string> g_windowNames[2];

// 先頭に並ぶアイコン文字 (U+E000〜U+F8FF の私用領域) と空白の長さ (バイト数)。
// UTF-8 では 3 バイト列 EE 80 80 〜 EF A3 BF にあたる。
size_t IconPrefixLength(const char* s) {
  const auto* p = reinterpret_cast<const unsigned char*>(s);
  size_t n = 0;
  bool hasIcon = false;
  for (;;) {
    const unsigned char c0 = p[n];
    if ((c0 == 0xEE || (c0 == 0xEF && p[n + 1] <= 0xA3)) && p[n + 1] && p[n + 2]) {
      n += 3;
      hasIcon = true;
    } else if (hasIcon && c0 == ' ') {
      ++n;
    } else {
      break;
    }
  }
  return n;
}

void LoadEnglish() {
  if (g_enLoaded) {
    return;
  }
  g_enLoaded = true;
  g_en.clear();
  g_joined.clear();
  g_windowNames[static_cast<int>(Language::English)].clear();

  const std::string path = g_langDir + "en.json";
  std::ifstream f(path);
  if (!f) {
    Logger("[Lang] " + path + " が見つかりません (English は日本語のまま表示されます)\n");
    return;
  }
  try {
    nlohmann::json j;
    f >> j;
    for (auto it = j.begin(); it != j.end(); ++it) {
      if (it.value().is_string()) {
        g_en[it.key()] = it.value().get<std::string>();
      }
    }
    Logger("[Lang] English: " + std::to_string(g_en.size()) + " strings\n");
  } catch (const std::exception &e) {
    Logger(std::string("[Lang] en.json の読み込みに失敗: ") + e.what() + "\n");
  }
}

} // namespace

void Initialize(const std::string& langDir) {
  g_langDir = langDir;
  if (g_lang == Language::English) {
    LoadEnglish();
  }
}

void SetLanguage(Language lang) {
  g_lang = lang;
  if (lang == Language::English) {
    LoadEnglish();
  }
}

Language Current() { return g_lang; }

const char* LanguageName(Language lang) {
  return lang == Language::English ? "English" : "日本語";
}

const char* LanguageCode(Language lang) {
  return lang == Language::English ? "en" : "ja";
}

Language LanguageFromCode(const std::string& code) {
  return code == "en" ? Language::English : Language::Japanese;
}

const char* Tr(const char* src) {
  if (!src || !*src || g_lang == Language::Japanese) {
    return src;
  }

  // 素の文字列 (大半はこれ)
  auto it = g_en.find(src);
  if (it != g_en.end()) {
    return it->second.c_str();
  }

  // «アイコン + 表示 + ##ID» : 表示の部分だけ訳し、前後はそのまま残す。
  //  ・先頭のアイコン (Font Awesome は私用領域の文字) と空白は表のキーに含めない。
  //    キーにアイコンが混ざると en.json が目で読めなくなるため。
  //  ・"##" 以降は ImGui の ID。訳すと配置や開閉状態が言語ごとに別物になる。
  const size_t head = IconPrefixLength(src);
  const char* hash = std::strstr(src + head, "##");
  if (head == 0 && !hash) {
    return src;
  }

  auto jt = g_joined.find(src);
  if (jt != g_joined.end()) {
    return jt->second.c_str();
  }
  const char* tail = hash ? hash : src + std::strlen(src);
  auto bt = g_en.find(std::string(src + head, tail));
  // 訳が無いものも覚えておく (毎フレーム文字列を組み立て直さないように)
  std::string joined = bt != g_en.end()
      ? std::string(src, head) + bt->second + tail
      : std::string(src);
  return g_joined.emplace(src, std::move(joined)).first->second.c_str();
}

// 表に無ければ src.c_str() を返すので、一時オブジェクトを渡さないこと
const char* Tr(const std::string& src) { return Tr(src.c_str()); }

const char* TrId(const std::string& name) {
  // 言語ごとに別の表へ覚える。切り替えても前の言語の文字列は消えない。
  auto &cache = g_windowNames[static_cast<int>(g_lang)];
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it->second.c_str();
  }
  return cache.emplace(name, std::string(Tr(name.c_str())) + StableId(name))
      .first->second.c_str();
}

std::string StableId(const std::string& name) { return "###" + name; }

const char* const* TrList(const char* const* items, int count) {
  static std::vector<const char*> buf;
  buf.resize(static_cast<size_t>(count > 0 ? count : 0));
  for (int i = 0; i < count; ++i) {
    buf[static_cast<size_t>(i)] = Tr(items[i]);
  }
  return buf.data();
}

// 表を作り直すので、それまでに返したポインタは無効になる。
// フレームの途中 (ImGui がラベルを握っている間) には呼ばないこと。
void Reload() {
  g_enLoaded = false;
  if (g_lang == Language::English) {
    LoadEnglish();
  }
}

} // namespace YoRigine::loc
