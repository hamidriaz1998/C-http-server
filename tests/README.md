# HTTP Parser Testing

This directory contains test drivers for testing individual components of the HTTP server.

## Usage

The makefile has been extended with a generic test target that allows you to test any component easily:

```bash
make test-<component>
```

### Available Tests

#### HTTP Parser Test
```bash
make test-http
```

Tests the HTTP request parsing functionality including:
- ✅ Simple GET requests
- ✅ POST requests with JSON body
- ✅ Multiple headers parsing
- ✅ Malformed request handling
- ✅ Empty request handling

#### Debug HTTP Parser
```bash
make test-debug
```

Provides detailed step-by-step debugging output showing how the HTTP parser processes requests. Useful for understanding the parsing flow and troubleshooting issues.

## Test Structure

Each test file follows the pattern `test_<component>.c` and includes:
- Multiple test cases covering different scenarios
- Detailed output showing parsed results
- Assertions to verify correct behavior
- Memory cleanup using appropriate free functions

## Adding New Component Tests

To test a new component (e.g., `hashtable`):

1. Create `tests/test_hashtable.c`
2. Include necessary headers and implement test functions
3. Run with `make test-hashtable`

The makefile automatically:
- Compiles the test with all library objects (excluding main.c)
- Links against the component source files
- Runs the test executable
- Cleans up the test binary after execution

## Test Results

### HTTP Parser Test Results

**✅ All tests passing!**

- **Simple GET**: Correctly parses method, path, version, and headers
- **POST with body**: Properly handles JSON body content and Content-Length header
- **Multiple headers**: Successfully parses all header key-value pairs
- **Error handling**: Gracefully handles malformed and empty requests

### Key Features Verified

1. **Request Line Parsing**: Method, path, and HTTP version extraction
2. **Header Parsing**: Multiple headers with proper key-value separation
3. **Body Parsing**: Correctly identifies and extracts request body when Content-Length is present
4. **Memory Management**: Proper allocation and cleanup of all parsed components
5. **Error Handling**: Robust handling of malformed inputs

## Notes

- The parser correctly handles the `\r\n\r\n` separator between headers and body
- Headers are stored in a hashtable for efficient lookup
- Memory is properly managed with `free_http_request()` function
- All string fields are duplicated to avoid dangling pointer issues
