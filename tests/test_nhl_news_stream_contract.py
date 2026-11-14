"""Guard against silent HTTP-200 ESPN/Forge JSON failures on ESP32.

These source contracts complement PlatformIO compilation and the hardware tests.
The controller must never assume a 200 response is already valid JSON.
"""
from pathlib import Path

source=Path("include/NhlNewsService.h").read_text()
config=Path("platformio.ini").read_text()
main=Path("src/main.cpp").read_text()

assert config.count("bblanchon/StreamUtils@^1.9.0")==2, "Both boards need streaming decoder"
for required in (
    "#include <StreamUtils.h>",
    'http.collectHeaders(headers,3)',
    'ChunkDecodingStream decoded(wire);',
    'transfer.indexOf("chunked")>=0',
    'ReadBufferingStream buffered(body,kStreamBufferBytes);',
    'wire.setTimeout(8000);',
    'DeserializationOption::Filter(filter)',
    'DeserializationOption::NestingLimit(16)',
    'result.error=parsed.c_str()',
    'result.error=parse.c_str()',
    'if(filter.overflowed())',
    'sample.createNestedArray("images").createNestedObject()',
    'logParseFailure("ESPN",http,parsed,doc);',
    'logParseFailure("Forge",http,parse,doc);',
    'ESP.getFreeHeap()',
    'doc.memoryUsage()',
    'http.header("Transfer-Encoding")',
    'http.header("Content-Encoding")',
    'https://site.api.espn.com/apis/site/v2/sports/hockey/nhl/news?limit=5',
):
    assert required in source,required
assert 'const auto parse=parseHttpJson(http,doc,filter);' in source
assert 'const auto parsed=parseHttpJson(http,doc,filter);' in source
assert 'http.getString()' not in source, "No whole-response copying on CYD"
# The source only updates cache after successful decode; error paths preserve
# any previously displayed stories and report a retry in the dashboard.
error_handler=main.split('void tickForgeNews(uint32_t now) {',1)[1].split(
    '// News typography is independent',1)[0]
assert 'newsCache.lastSuccessAt=millis();' in error_handler
assert 'newsCache.failures' in error_handler
assert 'newsLastError=String("ESPN ")' in error_handler
assert 'newsTryEspnNext=false;' in error_handler
print("PASS: HTTP-200 stream parser handles chunked, buffering, memory, and diagnostics")
