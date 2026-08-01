#!/usr/bin/env python3
"""
Pollock Translation Utility for Ruflux
Uses local Ollama LLM inference (e.g. Gemma 4) to translate and inject UI strings
across all 37 Pollock PO translation files in res/loc/po AND res/loc/rufus.loc.

Usage Examples:
  python res/loc/translate_pollock.py --msg-id MSG_174 --new-string "Ruflux - Another USB Formatting Utility"
  python res/loc/translate_pollock.py --old-string "Old String" --new-string "New String"
  python res/loc/translate_pollock.py --msg-id MSG_001 --new-string "Operation completed." --model batiai/gemma4-e4b:q4
"""

import os
import sys
import argparse
import glob
import json
import urllib.request
import re

# Force stdout to UTF-8 encoding for multi-language console output
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# Supported Pollock languages and human-readable names
LANG_MAP = {
    "en-US": "English",
    "ar-SA": "Arabic",
    "bg-BG": "Bulgarian",
    "cs-CZ": "Czech",
    "da-DK": "Danish",
    "de-DE": "German",
    "el-GR": "Greek",
    "es-ES": "Spanish",
    "fa-IR": "Persian",
    "fi-FI": "Finnish",
    "fr-FR": "French",
    "he-IL": "Hebrew",
    "hr-HR": "Croatian",
    "hu-HU": "Hungarian",
    "id-ID": "Indonesian",
    "it-IT": "Italian",
    "ja-JP": "Japanese",
    "ko-KR": "Korean",
    "lt-LT": "Lithuanian",
    "lv-LV": "Latvian",
    "ms-MY": "Malay",
    "nb-NO": "Norwegian",
    "nl-NL": "Dutch",
    "pl-PL": "Polish",
    "pt-BR": "Brazilian Portuguese",
    "pt-PT": "European Portuguese",
    "ro-RO": "Romanian",
    "ru-RU": "Russian",
    "sk-SK": "Slovak",
    "sl-SI": "Slovenian",
    "sr-RS": "Serbian",
    "sv-SE": "Swedish",
    "th-TH": "Thai",
    "tr-TR": "Turkish",
    "uk-UA": "Ukrainian",
    "vi-VN": "Vietnamese",
    "zh-CN": "Simplified Chinese",
    "zh-TW": "Traditional Chinese",
}

def translate_chunk(new_text, chunk_langs, model_name, ollama_url, timeout=180):
    """Query Ollama API to translate new_text into a chunk of target languages simultaneously."""
    langs_str = "\n".join([f"- {code}: {name}" for code, name in chunk_langs.items() if code != "en-US"])
    prompt = (
        f"Translate the software UI text '{new_text}' into the following languages.\n"
        f"Keep product names like 'Ruflux' untranslated in Latin characters if appropriate.\n"
        f"Return ONLY a valid JSON object mapping each language code to its translated string with no markdown formatting.\n\n"
        f"Target Languages:\n{langs_str}\n\n"
        f"JSON Format Example:\n{{\"{list(chunk_langs.keys())[0]}\": \"...\"}}"
    )
    payload = json.dumps({
        "model": model_name,
        "prompt": prompt,
        "format": "json",
        "stream": False,
        "options": {"temperature": 0.1}
    }).encode("utf-8")

    req = urllib.request.Request(ollama_url, data=payload, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            res_str = data.get("response", "").strip()
            res_dict = json.loads(res_str)
            res_dict["en-US"] = new_text
            return res_dict
    except Exception as e:
        print(f"Error during Ollama batch generation: {e}")
        return {"en-US": new_text}

def update_po_file(po_path, msg_id, old_string, new_string, translation):
    """Update a single PO file with the new msgid and translated msgstr."""
    if not os.path.exists(po_path):
        return False

    with open(po_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # Strategy 1: Search by MSG_xxx identifier if provided
    if msg_id:
        pattern = re.compile(
            r'(#\.\s*(?:•\s*)?' + re.escape(msg_id) + r'\s*\r?\nmsgid\s+")[^"]+("\s*\r?\nmsgstr\s+")[^"]*(")',
            re.MULTILINE
        )
        if pattern.search(content):
            new_content = pattern.sub(f'#. • {msg_id}\nmsgid "{new_string}"\nmsgstr "{translation}"', content)
            with open(po_path, "w", encoding="utf-8", newline="\n") as f:
                f.write(new_content)
            return True

    # Strategy 2: Search by exact old_string
    if old_string:
        pattern = re.compile(
            r'(msgid\s+"' + re.escape(old_string) + r'"\s*\r?\nmsgstr\s+")[^"]*(")',
            re.MULTILINE
        )
        if pattern.search(content):
            new_content = pattern.sub(f'msgid "{new_string}"\nmsgstr "{translation}"', content)
            with open(po_path, "w", encoding="utf-8", newline="\n") as f:
                f.write(new_content)
            return True

    # Strategy 3: Match existing new_string msgid to update msgstr
    pattern_new = re.compile(
        r'(msgid\s+"' + re.escape(new_string) + r'"\s*\r?\nmsgstr\s+")[^"]*(")',
        re.MULTILINE
    )
    if pattern_new.search(content):
        new_content = pattern_new.sub(f'msgid "{new_string}"\nmsgstr "{translation}"', content)
        with open(po_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(new_content)
        return True

    return False

def update_rufus_loc(rufus_loc_path, msg_id, new_string, translations):
    """Update master rufus.loc across all language blocks while preserving strict DOS CRLF line endings."""
    if not os.path.exists(rufus_loc_path):
        return 0

    with open(rufus_loc_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    lines = content.splitlines()
    new_lines = []
    current_lang = "en-US"
    updated_count = 0

    for line in lines:
        m_lang = re.match(r'^l\s+"([^"]+)"', line)
        if m_lang:
            current_lang = m_lang.group(1)

        if msg_id:
            m_msg = re.match(r'^(t\s+' + re.escape(msg_id) + r'\s+")[^"\r\n]*(".*)$', line)
            if m_msg and current_lang in translations:
                trans = translations[current_lang]
                line = f'{m_msg.group(1)}{trans}{m_msg.group(2)}'
                updated_count += 1

        new_lines.append(line)

    # Note: Rufus parser.c explicitly checks that rufus.loc MUST be saved with DOS (CRLF \r\n) line endings!
    with open(rufus_loc_path, "wb") as f:
        full_text = "\r\n".join(new_lines) + "\r\n"
        f.write(full_text.encode("utf-8"))

    return updated_count

def main():
    parser = argparse.ArgumentParser(description="Translate and update Pollock PO files & rufus.loc via Ollama LLM.")
    parser.add_argument("--new-string", required=True, help="New English string to translate and set as msgid.")
    parser.add_argument("--old-string", default=None, help="Existing msgid string to search and replace.")
    parser.add_argument("--msg-id", default=None, help="Target Pollock message ID (e.g. MSG_174, MSG_001).")
    parser.add_argument("--model", default="batiai/gemma4-e4b:q4", help="Ollama model name (default: batiai/gemma4-e4b:q4).")
    parser.add_argument("--ollama-url", default="http://localhost:11434/api/generate", help="Ollama API endpoint.")
    parser.add_argument("--batch-size", type=int, default=10, help="Number of languages to process per LLM call.")
    parser.add_argument("--po-dir", default=None, help="Path to PO files directory (default: res/loc/po).")
    parser.add_argument("--rufus-loc", default=None, help="Path to rufus.loc master file.")

    args = parser.parse_args()

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    po_dir = os.path.abspath(args.po_dir) if args.po_dir else os.path.join(repo_root, "res", "loc", "po")
    rufus_loc = os.path.abspath(args.rufus_loc) if args.rufus_loc else os.path.join(repo_root, "res", "loc", "rufus.loc")

    if not os.path.exists(po_dir):
        print(f"Error: PO directory '{po_dir}' does not exist.")
        sys.exit(1)

    print(f"=== Ruflux Pollock Translation Tool ===")
    print(f"Model       : {args.model}")
    print(f"New String  : '{args.new_string}'")
    if args.msg_id:
        print(f"Message ID  : {args.msg_id}")
    if args.old_string:
        print(f"Old String  : '{args.old_string}'")
    print(f"PO Dir      : {po_dir}")
    print(f"Master Loc  : {rufus_loc}\n")

    items = list(LANG_MAP.items())
    all_translations = {"en-US": args.new_string}

    for i in range(0, len(items), args.batch_size):
        chunk = dict(items[i:i + args.batch_size])
        codes_range = f"{list(chunk.keys())[0]}..{list(chunk.keys())[-1]}"
        print(f"Translating batch of {len(chunk)} languages ({codes_range})...")
        res = translate_chunk(args.new_string, chunk, args.model, args.ollama_url)
        for code, trans in res.items():
            if code != "en-US":
                print(f"  [{code}] {trans}")
            all_translations[code] = trans

    print(f"\nReceived {len(all_translations)} translations from {args.model}.")
    print("Updating PO files...")

    updated_count = 0
    for code, trans in all_translations.items():
        if code == "en-US":
            continue
        po_path = os.path.join(po_dir, f"{code}.po")
        if update_po_file(po_path, args.msg_id, args.old_string, args.new_string, trans):
            updated_count += 1
            print(f"  Updated {code}.po")
        else:
            print(f"  Warning: Could not match block in {code}.po")

    print(f"\nUpdating master {os.path.basename(rufus_loc)}...")
    loc_count = update_rufus_loc(rufus_loc, args.msg_id, args.new_string, all_translations)
    print(f"Updated {loc_count} language blocks in rufus.loc.")

    print(f"\nComplete! Successfully updated PO files & master rufus.loc.")

if __name__ == "__main__":
    main()
