from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
header = root / "tests" / "test_require.h"
header.write_text(
r'''#pragma once
#include <cstdlib>
#include <iostream>
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << "REQUIRE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << '\\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)
''',
    encoding="utf-8",
)

for path in sorted((root / "tests").glob("test_cpp_*.cpp")):
    text = path.read_text(encoding="utf-8")
    if not re.search(r"\bassert\(", text):
        continue
    if '#include "test_require.h"' not in text:
        text = '#include "test_require.h"\n' + text
    text = re.sub(r"\bassert\(", "REQUIRE(", text)
    path.write_text(text, encoding="utf-8")
