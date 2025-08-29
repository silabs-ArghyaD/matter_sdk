#!/usr/bin/env python3
"""
Unit Test Validator using LiteLLM (GPT-4.1)
Validates test files against ValidationTemplate.md
"""

import os
import sys
import argparse
import requests
from pathlib import Path

LITELLM_API = "https://litellm.silabs.net/chat/completions"

def read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except Exception as e:
        print(f"❌ Error reading {path}: {e}")
        return None

def load_template():
    template_path = Path(__file__).parent / "ValidationTemplate.md"
    template = read_file(template_path)
    if not template:
        print("❌ Could not load ValidationTemplate.md")
        sys.exit(1)
    return template

def validate_file(file_path, template, token):
    content = read_file(file_path)
    if not content:
        return {"file": file_path, "status": "ERROR", "error": "File could not be read"}

    prompt = f"""
You are a unit test validator. Validate the following C++ unit test file
using the provided validation template.

VALIDATION TEMPLATE:
{template}

TEST FILE TO VALIDATE: {file_path}

```cpp
{content}
```

Follow the template exactly: show passed checks, failed checks, recommendations,
and a final 🎯 Status: PASS or FAIL.
"""

    headers = {
        "accept": "application/json",
        "authorization": f"Bearer {token}",
        "content-type": "application/json"
    }

    payload = {
        "model": "gpt-4.1",
        "messages": [
            {"role": "system", "content": "You are a strict validator. Follow the template exactly."},
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.1,
        "max_tokens": 1500
    }

    try:
        resp = requests.post(LITELLM_API, headers=headers, json=payload)
        resp.raise_for_status()
        data = resp.json()
        result_text = data["choices"][0]["message"]["content"]

        status = "UNKNOWN"
        if "Status: PASS" in result_text:
            status = "PASS"
        elif "Status: FAIL" in result_text:
            status = "FAIL"

        return {"file": file_path, "status": status, "validation_result": result_text}

    except Exception as e:
        return {"file": file_path, "status": "ERROR", "error": str(e)}

def main():
    parser = argparse.ArgumentParser(description="Validate test files with LiteLLM GPT-4.1")
    parser.add_argument("files", nargs="+", help="List of test files to validate")
    args = parser.parse_args()

    token = os.getenv("LITELLM_TOKEN")
    if not token:
        print("❌ LITELLM_TOKEN environment variable required")
        sys.exit(1)

    template = load_template()

    for f in args.files:
        print(f"🔎 Validating {f} ...")
        result = validate_file(f, template, token)
        if result["status"] == "ERROR":
            print(f"   ⚠️ ERROR: {result['error']}")
        else:
            print(result["validation_result"])
            print("="*60)

if __name__ == "__main__":
    main()
