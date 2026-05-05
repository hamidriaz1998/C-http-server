# C HTTP Server

A small, educational HTTP/1.1 server written in C. It focuses on the basics: a
simple TCP listener, a thread pool for request handling, a minimal HTTP parser,
and static file serving from a public directory. The code is intentionally
compact and easy to follow so you can experiment with networking, concurrency,
and request parsing.

## Current Architecture

- Entry point: `main.c` sets up signal handling, initializes the server, and
  starts the request loop.
- Networking: `src/network.c` owns socket setup, bind/listen/accept, and hands
  accepted connections to the thread pool.
- Thread pool: `src/thread_pool.c` + `src/queue.c` implement a basic task queue
  with worker threads waiting on a condition variable.
- HTTP parsing: `src/http.c` parses request lines, headers, and optional body
  into `http_request` and builds `http_response` structures.
- Request handling: `src/handler.c` currently supports GET only. It resolves
  the requested path, builds headers, and streams files using `sendfile`.
- Utilities: `src/utils.c` provides path normalization, URL decoding, and MIME
  type detection.
- Data structures: `src/hashtable.c` implements a small hash table used for
  headers.

Static files are served from the `public/` directory. Paths are normalized to
prevent directory traversal and the default file is `index.html`.

## Usage

### Build

```bash
make
```

The binary is created at `bin/server`.

### Run

```bash
./bin/server
```

By default the server listens on port `9000` and serves files from `./public`.
Open `http://localhost:9000/` to verify.

### Tests

The makefile supports per-component tests. For example:

```bash
make test-http
```

See `tests/README.md` for details.

## Project Layout

- `main.c` entry point
- `include/` public headers
- `src/` implementation files
- `public/` static assets served by the server
- `tests/` component tests
- `makefile` build and test targets
- `roadmap.md` planned improvements

## Future Plans

Based on `roadmap.md`:

- Remove magic numbers and hardcoded values
- Make the document root configurable (currently `./public`)
- Make the port configurable (currently `9000`)
- Improve concurrency with an acceptor thread and epoll-based workers
- Add logging
- Add CLI arguments

If you want to contribute, open an issue or pick a roadmap item and send a PR.
