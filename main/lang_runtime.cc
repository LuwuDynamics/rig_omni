#include "assets/lang_config.h"
#include "settings.h"

namespace Lang {

#if CONFIG_FIRMWARE_REGION_OVERSEAS
static Code g_lang = Code::en_US;  // 海外版默认英文
#else
static Code g_lang = Code::zh_CN;  // 国内版默认中文
#endif
static bool g_initialized = false;

void Initialize() {
    if (g_initialized) return;
    Settings settings("language", false);
    int lang = settings.GetInt("code", static_cast<int>(g_lang));  // 默认值跟随区域设定
    if (lang >= 0 && lang <= static_cast<int>(Code::en_US)) {
        g_lang = static_cast<Code>(lang);
    }
    g_initialized = true;
}

void SetLanguage(Code lang) {
    g_lang = lang;
    Settings settings("language", true);
    settings.SetInt("code", static_cast<int>(lang));
}

Code GetLanguage() {
    return g_lang;
}

const char* CodeStr() {
    switch (g_lang) {
        case Code::en_US: return "en-US";
        case Code::zh_CN: return "zh-CN";
        default: return "zh-CN";
    }
}

}  // namespace Lang
