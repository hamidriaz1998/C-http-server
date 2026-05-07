#include "../include/utils.h"
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int hexval(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int strip_query_fragment(const char *url, char *out, size_t out_size) {
  size_t i = 0;
  while (url[i] && url[i] != '?' && url[i] != '#') {
    if (i + 1 >= out_size)
      return -1;
    out[i] = url[i];
    i++;
  }
  out[i] = '\0';
  return 0;
}

static int url_decode(const char *src, char *dst, size_t dst_size) {
  size_t di = 0;
  for (size_t i = 0; src[i] != '\0'; i++) {
    unsigned char ch = (unsigned char)src[i];

    if (ch == '%') {
      if (!src[i + 1] || !src[i + 2])
        return -1;
      int hi = hexval(src[i + 1]);
      int lo = hexval(src[i + 2]);
      if (hi < 0 || lo < 0)
        return -1;
      ch = (unsigned char)((hi << 4) | lo);
      if (ch == '\0')
        return -1;
      i += 2;
    }

    if (ch == '\\')
      return -1;
    if (ch < ASCII_CTRL_START || ch == ASCII_DEL)
      return -1;

    if (di + 1 >= dst_size)
      return -1;
    dst[di++] = (char)ch;
  }
  dst[di] = '\0';
  return 0;
}

static int normalize_and_join(const char *path, const char *doc_root, char *out,
                              size_t out_size) {
  const char *segments[MAX_PATH_SEGMENTS];
  size_t seg_len[MAX_PATH_SEGMENTS];
  size_t seg_count = 0;

  const char *p = path;
  while (*p == '/')
    p++;

  while (*p) {
    const char *start = p;
    while (*p && *p != '/')
      p++;
    size_t len = (size_t)(p - start);
    while (*p == '/')
      p++;

    if (len == 0)
      continue;
    if (len == 1 && start[0] == '.')
      continue;

    if (len == 2 && start[0] == '.' && start[1] == '.') {
      if (seg_count == 0)
        return -1;
      seg_count--;
      continue;
    }

    if (seg_count >= MAX_PATH_SEGMENTS)
      return -1;
    segments[seg_count] = start;
    seg_len[seg_count] = len;
    seg_count++;
  }

  size_t root_len = strlen(doc_root);
  while (root_len > 0 && doc_root[root_len - 1] == '/')
    root_len--;
  if (root_len == 0)
    return -1;

  size_t needed = root_len + 1;
  if (seg_count == 0) {
    needed += strlen("index.html");
  } else {
    for (size_t i = 0; i < seg_count; i++) {
      needed += seg_len[i] + 1;
    }
    needed -= 1;
  }

  if (needed + 1 > out_size)
    return -1;

  size_t pos = 0;
  memcpy(out + pos, doc_root, root_len);
  pos += root_len;
  out[pos++] = '/';

  if (seg_count == 0) {
    memcpy(out + pos, "index.html", strlen("index.html"));
    pos += strlen("index.html");
  } else {
    for (size_t i = 0; i < seg_count; i++) {
      memcpy(out + pos, segments[i], seg_len[i]);
      pos += seg_len[i];
      if (i + 1 < seg_count)
        out[pos++] = '/';
    }
  }

  out[pos] = '\0';
  return 0;
}

const char *get_mime_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";
  ext++;

  if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
    return "text/html";
  if (strcmp(ext, "css") == 0)
    return "text/css";
  if (strcmp(ext, "js") == 0)
    return "application/javascript";
  if (strcmp(ext, "json") == 0)
    return "application/json";
  if (strcmp(ext, "png") == 0)
    return "image/png";
  if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
    return "image/jpeg";
  if (strcmp(ext, "gif") == 0)
    return "image/gif";
  if (strcmp(ext, "svg") == 0)
    return "image/svg+xml";
  if (strcmp(ext, "ico") == 0)
    return "image/x-icon";
  if (strcmp(ext, "txt") == 0)
    return "text/plain";
  if (strcmp(ext, "pdf") == 0)
    return "application/pdf";
  if (strcmp(ext, "xml") == 0)
    return "application/xml";
  if (strcmp(ext, "wav") == 0)
    return "audio/wav";
  if (strcmp(ext, "mp3") == 0)
    return "audio/mpeg";
  if (strcmp(ext, "mp4") == 0)
    return "video/mp4";
  if (strcmp(ext, "webm") == 0)
    return "video/webm";
  if (strcmp(ext, "woff") == 0)
    return "font/woff";
  if (strcmp(ext, "woff2") == 0)
    return "font/woff2";
  if (strcmp(ext, "ttf") == 0)
    return "font/ttf";

  return "application/octet-stream";
}

int resolve_request_path(const char *url, const char *doc_root, char *out,
                         size_t out_size) {
  char path_only[PATH_MAX];
  char decoded[PATH_MAX];

  if (strip_query_fragment(url, path_only, sizeof(path_only)) != 0)
    return -1;

  if (path_only[0] == '\0') {
    if (snprintf(path_only, sizeof(path_only), "/") >= (int)sizeof(path_only))
      return -1;
  }

  if (path_only[0] != '/') {
    char tmp[PATH_MAX];
    int n = snprintf(tmp, sizeof(tmp), "/%s", path_only);
    if (n < 0 || (size_t)n >= sizeof(tmp))
      return -1;
    memcpy(path_only, tmp, (size_t)n + 1);
  }

  if (url_decode(path_only, decoded, sizeof(decoded)) != 0)
    return -1;

  if (normalize_and_join(decoded, doc_root, out, out_size) != 0)
    return -1;

  struct stat sb;
  if (stat(out, &sb) != 0)
    return -1;

  if (S_ISDIR(sb.st_mode)) {
    const char *index_suffix = "index.html";
    size_t len = strlen(out);
    size_t suffix_len = strlen(index_suffix);

    if (len + 1 + suffix_len + 1 > out_size)
      return -1;

    if (len == 0 || out[len - 1] != '/') {
      out[len] = '/';
      out[len + 1] = '\0';
      len += 1;
    }

    memcpy(out + len, index_suffix, suffix_len);
    out[len + suffix_len] = '\0';

    if (stat(out, &sb) != 0 || !S_ISREG(sb.st_mode))
      return -1;

    return 0;
  }

  if (!S_ISREG(sb.st_mode))
    return -1;

  return 0;
}
