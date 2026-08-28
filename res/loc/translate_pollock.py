#!/usr/bin/env python3
"""
Pollock Translation Utility for Ruflux
Uses local Ollama LLM inference (Gemma 4, TranslateGemma 12B, etc.) to translate and inject UI strings
across all 37 Pollock PO translation files in res/loc/po AND res/loc/rufus.loc.

Usage Examples:
  python res/loc/translate_pollock.py --msg-id MSG_174 --new-string "Ruflux - Another USB Formatting Utility"
  python res/loc/translate_pollock.py --msg-id MSG_174 --new-string "Ruflux - Another USB Formatting Utility" --model translategemma:12b --strict
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

def log(msg):
    """Print message and flush immediately to ensure real-time log updates."""
    print(msg, flush=True)

def translate_chunk(new_text, chunk_langs, model_name, ollama_url, strict=False, timeout=600):
    """Query Ollama API to translate new_text into a chunk of target languages simultaneously."""
    langs_str = "\n".join([f"- {code}: {name}" for code, name in chunk_langs.items() if code != "en-US"])
    
    if strict:
        prompt = (
            f"You are a strict software UI localization translation system.\n"
            f"Translate the following string into the target languages.\n"
            f"Source English String: \"{new_text}\"\n\n"
            f"Target Languages:\n{langs_str}\n\n"
            f"Rules:\n"
            f"1. Preserve 'Ruflux' untranslated.\n"
            f"2. Use standard computer software UI terminology.\n"
            f"3. Return ONLY a valid JSON object mapping each language code to its translated string."
        )
        temp = 0.0
    else:
        prompt = (
            f"Translate the software UI text '{new_text}' into the following languages.\n"
            f"Keep product names like 'Ruflux' untranslated in Latin characters.\n"
            f"Use modern software terminology (e.g. 'utilita' in Czech, 'nástroj' in Slovak, 'utility' in English).\n"
            f"Return ONLY a valid JSON object mapping each language code to its translated string with no markdown formatting.\n\n"
            f"Target Languages:\n{langs_str}\n\n"
            f"JSON Format Example:\n{{\"{list(chunk_langs.keys())[0]}\": \"...\"}}"
        )
        temp = 0.1

    payload = json.dumps({
        "model": model_name,
        "prompt": prompt,
        "format": "json",
        "stream": False,
        "options": {"temperature": temp}
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
        log(f"Error during Ollama batch generation: {e}")
        return {"en-US": new_text}

def update_po_file(po_path, msg_id, old_string, new_string, translation):
    """Update a single PO file with the new msgid and translated msgstr.
    
    Tries three strategies in order:
      1. Match by MSG_xxx identifier comment.
      2. Match by exact old msgid string.
      3. Match by new msgid string (idempotent re-run).
    If none match (e.g. a brand-new string), appends a new PO entry at the end.
    """
    if not os.path.exists(po_path):
        return False

    with open(po_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # Strategy 1: Search by MSG_xxx identifier if provided
    if msg_id:
        pattern = re.compile(
            r'(#\.\s*(?:\u2022\s*)?' + re.escape(msg_id) + r'\s*\r?\nmsgid\s+")[^"]+("\s*\r?\nmsgstr\s+")[^"]*(")',
            re.MULTILINE
        )
        if pattern.search(content):
            new_content = pattern.sub(f'#. \u2022 {msg_id}\nmsgid "{new_string}"\nmsgstr "{translation}"', content)
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

    # Strategy 3: Match existing new_string msgid to update msgstr (idempotent)
    pattern_new = re.compile(
        r'(msgid\s+"' + re.escape(new_string) + r'"\s*\r?\nmsgstr\s+")[^"]*(")',
        re.MULTILINE
    )
    if pattern_new.search(content):
        new_content = pattern_new.sub(f'msgid "{new_string}"\nmsgstr "{translation}"', content)
        with open(po_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(new_content)
        return True

    # Strategy 4: Brand-new string — append a new PO entry at the end of the file.
    id_comment = f"#. \u2022 {msg_id}\n" if msg_id else ""
    new_entry = f"\n{id_comment}msgid \"{new_string}\"\nmsgstr \"{translation}\"\n"
    with open(po_path, "a", encoding="utf-8", newline="\n") as f:
        f.write(new_entry)
    return True

def update_rufus_loc(rufus_loc_path, msg_id, new_string, translations):
    """Update master rufus.loc across all language blocks.
    
    For each language block:
      - If 't MSG_xxx ...' already exists: replace it in-place.
      - If it is missing (e.g. a brand-new string added via --add-string):
        insert it at the correct alphabetically-sorted position among the
        existing MSG lines so the block stays well-ordered.
    Preserves strict DOS CRLF line endings required by the Rufus parser.
    """
    if not os.path.exists(rufus_loc_path):
        return 0

    with open(rufus_loc_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    lines = content.splitlines()

    # Split into language blocks. Each block is delimited by a run of '#' characters.
    # The separator line starts a new block header, so we group:
    #   [header-comment lines] [sep line] [lang block lines] [sep line] [lang block] ...
    SEP = re.compile(r'^#{6,}')
    blocks = []   # list of lists-of-lines
    current = []
    for line in lines:
        if SEP.match(line):
            if current:
                blocks.append(current)
            current = [line]   # separator starts a new block
        else:
            current.append(line)
    if current:
        blocks.append(current)

    if not msg_id:
        # No msg_id means nothing to update; just rewrite unchanged.
        with open(rufus_loc_path, "wb") as f:
            f.write(("\r\n".join(lines) + "\r\n").encode("utf-8"))
        return 0

    msg_pattern     = re.compile(r'^t\s+' + re.escape(msg_id) + r'\s+')
    any_msg_pattern = re.compile(r'^t\s+(MSG_\S+)')
    updated_count   = 0

    result_blocks = []
    for block in blocks:
        # Identify the language this block belongs to.
        lang_id = None
        for line in block:
            m = re.match(r'^l\s+"([^"]+)"', line)
            if m:
                lang_id = m.group(1)
                break

        if lang_id is None or lang_id not in translations:
            result_blocks.append(block)
            continue

        # Check whether msg_id already exists in this block.
        if any(msg_pattern.match(l) for l in block):
            # Replace in-place.
            new_block = []
            for line in block:
                if msg_pattern.match(line):
                    line = f't {msg_id} "{translations[lang_id]}"'
                    updated_count += 1
                new_block.append(line)
            result_blocks.append(new_block)
        elif lang_id != "en-US":
            # Brand-new string: insert at the correct sorted position.
            new_block = list(block)
            insert_line = f't {msg_id} "{translations[lang_id]}"'

            # Collect (line-index, MSG_id) for every existing MSG line.
            msg_positions = [
                (i, any_msg_pattern.match(l).group(1))
                for i, l in enumerate(new_block)
                if any_msg_pattern.match(l)
            ]

            if msg_positions:
                # Binary-search for the right alphabetical slot.
                insert_at = msg_positions[-1][0] + 1   # default: after last MSG line
                for pos, mid in msg_positions:
                    if msg_id < mid:
                        insert_at = pos
                        break
                new_block.insert(insert_at, insert_line)
            else:
                # No MSG lines yet — append before any trailing blank lines.
                insert_at = len(new_block)
                while insert_at > 0 and not new_block[insert_at - 1].strip():
                    insert_at -= 1
                new_block.insert(insert_at, insert_line)

            updated_count += 1
            result_blocks.append(new_block)
        else:
            result_blocks.append(block)

    # Flatten and write with mandatory CRLF endings.
    all_lines = [line for block in result_blocks for line in block]
    with open(rufus_loc_path, "wb") as f:
        f.write(("\r\n".join(all_lines) + "\r\n").encode("utf-8"))

    return updated_count

def main():
    parser = argparse.ArgumentParser(description="Translate and update Pollock PO files & rufus.loc via Ollama LLM.")
    parser.add_argument("--new-string", required=True, help="New English string to translate and set as msgid.")
    parser.add_argument("--old-string", default=None, help="Existing msgid string to search and replace.")
    parser.add_argument("--msg-id", default=None, help="Target Pollock message ID (e.g. MSG_174, MSG_001).")
    parser.add_argument("--model", default="batiai/gemma4-e4b:q4", help="Ollama model name (default: batiai/gemma4-e4b:q4).")
    parser.add_argument("--strict", action="store_true", help="Enable strict translation mode for specialized MT models (TranslateGemma).")
    parser.add_argument("--ollama-url", default="http://localhost:11434/api/generate", help="Ollama API endpoint.")
    parser.add_argument("--batch-size", type=int, default=10, help="Number of languages to process per LLM call.")
    parser.add_argument("--po-dir", default=None, help="Path to PO files directory (default: res/loc/po).")
    parser.add_argument("--rufus-loc", default=None, help="Path to rufus.loc master file.")

    args = parser.parse_args()

    # Auto-enable --strict if model name contains 'translate'
    if "translate" in args.model.lower():
        args.strict = True

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    po_dir = os.path.abspath(args.po_dir) if args.po_dir else os.path.join(repo_root, "res", "loc", "po")
    rufus_loc = os.path.abspath(args.rufus_loc) if args.rufus_loc else os.path.join(repo_root, "res", "loc", "rufus.loc")

    if not os.path.exists(po_dir):
        log(f"Error: PO directory '{po_dir}' does not exist.")
        sys.exit(1)

    log(f"=== Ruflux Pollock Translation Tool ===")
    log(f"Model       : {args.model}")
    log(f"Strict Mode : {args.strict}")
    log(f"New String  : '{args.new_string}'")
    if args.msg_id:
        log(f"Message ID  : {args.msg_id}")
    if args.old_string:
        log(f"Old String  : '{args.old_string}'")
    log(f"PO Dir      : {po_dir}")
    log(f"Master Loc  : {rufus_loc}\n")

    items = list(LANG_MAP.items())
    all_translations = {"en-US": args.new_string}

    for i in range(0, len(items), args.batch_size):
        chunk = dict(items[i:i + args.batch_size])
        codes_range = f"{list(chunk.keys())[0]}..{list(chunk.keys())[-1]}"
        log(f"Translating batch of {len(chunk)} languages ({codes_range})...")
        res = translate_chunk(args.new_string, chunk, args.model, args.ollama_url, strict=args.strict)
        for code, trans in res.items():
            if code != "en-US":
                log(f"  [{code}] {trans}")
            all_translations[code] = trans

    log(f"\nReceived {len(all_translations)} translations from {args.model}.")
    log("Updating PO files...")

    updated_count = 0
    for code, trans in all_translations.items():
        if code == "en-US":
            continue
        po_path = os.path.join(po_dir, f"{code}.po")
        if update_po_file(po_path, args.msg_id, args.old_string, args.new_string, trans):
            updated_count += 1
            log(f"  Updated {code}.po")
        else:
            log(f"  Warning: Could not match block in {code}.po")

    log(f"\nUpdating master {os.path.basename(rufus_loc)}...")
    loc_count = update_rufus_loc(rufus_loc, args.msg_id, args.new_string, all_translations)
    log(f"Updated {loc_count} language blocks in rufus.loc.")

    log(f"\nComplete! Successfully updated PO files & master rufus.loc.")

if __name__ == "__main__":
    main()
