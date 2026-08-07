#!/usr/bin/env python3
"""Generate runtime-switchable multi-language config header.

Auto-discovers all language directories under locales/ and generates
a header with inline dispatch functions that select strings and sounds
at runtime based on Lang::GetLanguage().
"""

import argparse
import json
import os

HEADER_TEMPLATE = """// Auto-generated runtime language config
// Supports: {supported_langs}
#pragma once

#include <string_view>

namespace Lang {{

    enum class Code : uint8_t {{ {enum_values} }};

    void Initialize();            // 从 NVS 读取语言设置
    void SetLanguage(Code lang);  // 运行时切换语言 + 写入 NVS
    Code GetLanguage();           // 获取当前语言
    const char* CodeStr();        // 返回如 "zh-CN" / "en-US"

    // 字符串资源 — 运行时按语言分发
    namespace Strings {{
{strings}
    }}

    // 音效资源 — 运行时按语言分发
    namespace Sounds {{
{sounds}
    }}

}}
"""


def get_sound_files(directory):
    if not os.path.exists(directory):
        return []
    return sorted(f for f in os.listdir(directory) if f.endswith('.ogg'))


def scan_languages(assets_dir):
    """Scan all language directories in locales/"""
    locales_dir = os.path.join(assets_dir, 'locales')
    if not os.path.exists(locales_dir):
        return []
    langs = []
    for d in sorted(os.listdir(locales_dir)):
        lang_path = os.path.join(locales_dir, d)
        json_path = os.path.join(lang_path, 'language.json')
        if os.path.isdir(lang_path) and os.path.exists(json_path):
            langs.append(d)
    return langs


def load_language(assets_dir, lang_code):
    json_path = os.path.join(assets_dir, 'locales', lang_code, 'language.json')
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def lang_prefix(lang):
    """Convert language code to safe prefix: 'zh-CN' -> 'zh_CN'"""
    return lang.replace('-', '_')


def embed_symbol(base, lang_prefix_str=None):
    """Generate ESP-IDF embed binary symbol name.
    
    For language-specific: base='welcome', prefix='zh_CN'
      -> file 'zh_CN_welcome.ogg' -> _binary_zh_CN_welcome_ogg_start
    For common/board: base='success', prefix=None
      -> file 'success.ogg' -> _binary_success_ogg_start
    """
    if lang_prefix_str:
        filename = f'{lang_prefix_str}_{base}.ogg'
    else:
        filename = f'{base}.ogg'
    # Replace non-alphanum with underscore (matches ESP-IDF behaviour)
    cleaned = ''
    for ch in filename:
        if ch.isalnum() or ch == '_':
            cleaned += ch
        else:
            cleaned += '_'
    return f'_binary_{cleaned}'


def generate_header(output_path):
    # Derive asset dir from output path
    main_dir = os.path.dirname(output_path)            # main/assets
    if os.path.basename(main_dir) == 'assets':
        main_dir = os.path.dirname(main_dir)           # main
    assets_dir = os.path.join(main_dir, 'assets')

    langs = scan_languages(assets_dir)
    if not langs:
        raise ValueError(f'No language directories found under {assets_dir}/locales/')

    print(f'Found languages: {", ".join(langs)}')

    # ---- Load all language JSON data ----
    lang_data = {}
    for lang in langs:
        data = load_language(assets_dir, lang)
        lang_data[lang] = data.get('strings', {})
        print(f'  {lang}: {len(lang_data[lang])} strings')

    # Collect all string keys across all languages
    all_keys = set()
    for strings in lang_data.values():
        all_keys.update(strings.keys())
    # Fallback: if a key is missing, use the en-US value first, then the first available
    primary_lang = 'en-US' if 'en-US' in langs else langs[0]

    # ---- Collect all sound files ----
    sound_files = {}  # base_name -> set of languages
    for lang in langs:
        lang_dir = os.path.join(assets_dir, 'locales', lang)
        for f in sorted(get_sound_files(lang_dir)):
            base = os.path.splitext(f)[0]
            if base not in sound_files:
                sound_files[base] = set()
            sound_files[base].add(lang)

    common_dir = os.path.join(assets_dir, 'common')
    common_sounds = {os.path.splitext(f)[0] for f in get_sound_files(common_dir)}

    boards_dir = os.path.join(main_dir, 'boards')
    board_sounds = set()
    if os.path.exists(boards_dir):
        for root, dirs, files in os.walk(boards_dir):
            for f in files:
                if f.endswith('.ogg'):
                    board_sounds.add(os.path.splitext(f)[0])

    print(f'Sound statistics:')
    for lang in langs:
        count = len([b for b, ls in sound_files.items() if lang in ls])
        print(f'  {lang}: {count} sounds')
    print(f'  common: {len(common_sounds)} sounds')
    print(f'  board-specific: {len(board_sounds)} sounds')

    # ---- Generate code ----
    enum_values = ', '.join(lang_prefix(l) for l in langs)

    # String dispatch functions
    strings_code = []
    for key in sorted(all_keys):
        key_upper = key.upper()

        # Collect values per language, with fallback
        values = {}
        for lang in langs:
            val = lang_data[lang].get(key)
            if val is None:
                val = lang_data.get(primary_lang, {}).get(key, key)
            values[lang] = val.replace('"', '\\"').replace('\n', '\\n')

        strings_code.append(f'        inline const char* {key_upper}() {{')
        strings_code.append(f'            switch (GetLanguage()) {{')
        for lang in langs:
            enum_val = lang_prefix(lang)
            strings_code.append(f'                case Code::{enum_val}: return "{values[lang]}";')
        strings_code.append(f'            }}')
        strings_code.append(f'            return "{values[langs[0]]}";')
        strings_code.append(f'        }}')

    # Sound dispatch functions
    sounds_code = []
    processed_bases = set()

    # 1) Language-specific sounds
    for base in sorted(sound_files.keys()):
        langs_with = sound_files[base]
        processed_bases.add(base)
        base_upper = base.upper()

        # Fallback language for languages that don't have this file
        fallback_lang = list(langs_with)[0]

        sounds_code.append(f'')
        sounds_code.append(f'        // Sound: {base}')

        # Declare extern symbols for each language
        for lang in langs_with:
            pfx = lang_prefix(lang)
            sym = embed_symbol(base, pfx)
            var = f'ogg_{pfx.lower()}_{base}'
            sounds_code.append(f'        extern const char {var}_start[] asm("{sym}_start");')
            sounds_code.append(f'        extern const char {var}_end[]   asm("{sym}_end");')

        # Inline dispatch
        sounds_code.append(f'        inline std::string_view OGG_{base_upper}() {{')
        sounds_code.append(f'            switch (GetLanguage()) {{')
        for lang in langs:
            enum_val = lang_prefix(lang)
            if lang in langs_with:
                pfx = lang_prefix(lang)
                var = f'ogg_{pfx.lower()}_{base}'
            else:
                pfx = lang_prefix(fallback_lang)
                var = f'ogg_{pfx.lower()}_{base}'
            sounds_code.append(f'                case Code::{enum_val}:')
            sounds_code.append(f'                    return {{static_cast<const char*>({var}_start), static_cast<size_t>({var}_end - {var}_start)}};')
        sounds_code.append(f'            }}')
        sounds_code.append(f'            return {{}};')
        sounds_code.append(f'        }}')

    # 2) Common sounds (language-agnostic)
    for base in sorted(common_sounds):
        if base in processed_bases:
            continue
        processed_bases.add(base)
        base_upper = base.upper()
        sym = embed_symbol(base)
        sounds_code.append(f'')
        sounds_code.append(f'        // Common sound: {base}')
        sounds_code.append(f'        extern const char ogg_{base}_start[] asm("{sym}_start");')
        sounds_code.append(f'        extern const char ogg_{base}_end[]   asm("{sym}_end");')
        sounds_code.append(f'        inline std::string_view OGG_{base_upper}() {{')
        sounds_code.append(f'            return {{static_cast<const char*>(ogg_{base}_start), static_cast<size_t>(ogg_{base}_end - ogg_{base}_start)}};')
        sounds_code.append(f'        }}')

    # 3) Board-specific sounds (language-agnostic)
    for base in sorted(board_sounds):
        if base in processed_bases:
            continue
        processed_bases.add(base)
        base_upper = base.upper()
        sym = embed_symbol(base)
        sounds_code.append(f'')
        sounds_code.append(f'        // Board-specific sound: {base}')
        sounds_code.append(f'        extern const char ogg_{base}_start[] asm("{sym}_start");')
        sounds_code.append(f'        extern const char ogg_{base}_end[]   asm("{sym}_end");')
        sounds_code.append(f'        inline std::string_view OGG_{base_upper}() {{')
        sounds_code.append(f'            return {{static_cast<const char*>(ogg_{base}_start), static_cast<size_t>(ogg_{base}_end - ogg_{base}_start)}};')
        sounds_code.append(f'        }}')

    # Fill template
    content = HEADER_TEMPLATE.format(
        supported_langs=', '.join(langs),
        enum_values=enum_values,
        strings='\n'.join(strings_code),
        sounds='\n'.join(sounds_code),
    )

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f'\nGenerated: {output_path}')
    print(f'  Strings: {len(all_keys)} keys across {len(langs)} languages')
    print(f'  Sounds: {len(processed_bases)} total (lang={len(sound_files)} common={len(common_sounds)} board={len(board_sounds)})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate runtime multi-language config header')
    parser.add_argument('--output', required=True, help='Output header file path')
    args = parser.parse_args()

    try:
        generate_header(args.output)
        print('Success!')
    except Exception as e:
        print(f'Error: {e}')
        exit(1)
