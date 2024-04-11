#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "neco.h"
#include "picohttpparser.h"

typedef struct {
  char* ext;
  char* type;
} mime_type;

const mime_type mime_types[] = {
  { .ext = ".txt",  .type = "text/plain; charset=utf-8" },
  { .ext = ".html", .type = "text/html; charset=utf-8" },
  { .ext = ".js",   .type = "text/javascript" },
  { .ext = ".json", .type = "text/json" },
  { .ext = ".png",  .type = "image/png" },
  { .ext = ".jpg",  .type = "image/jpeg" },
  { .ext = ".jpeg", .type = "image/jpeg" },
  { .ext = ".gif",  .type = "image/gif" },
  { .ext = ".css",  .type = "text/css" },
};

void client(int argc, void *argv[]) {
  int conn = *(int*)argv[0];

  char buf[4096];
  const char *method, *path;
  int pret, minor_version;
  struct phr_header headers[100];
  size_t buflen = 0, prevbuflen = 0, method_len, path_len, num_headers;
  ssize_t rret;

retry:
  num_headers = 0;
  prevbuflen = 0;
  buflen = 0;
  while (1) {
    ssize_t rret = neco_read(conn, buf + buflen, sizeof(buf) - buflen);
    if (rret <= 0) {
      return;
    }
    prevbuflen = buflen;
    buflen += rret;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(
        buf, buflen, &method, &method_len, &path, &path_len,
        &minor_version, headers, &num_headers, prevbuflen);
    if (pret > 0) {
      break;
    }
    if (pret == -1) {
      // ParseError
      return;
    }
    if (buflen == sizeof(buf)) {
      // RequestIsTooLongError
      continue;
    }
  }

  int keep_alive = 0;
  int i;
  for (i = 0; i < num_headers; i++) {
    if (strncasecmp(headers[i].name, "connection", headers[i].name_len) == 0 &&
        strncasecmp(headers[i].value, "keep-alive", headers[i].value_len) == 0)
      keep_alive = 1;
  }

  char zpath[2049], *s = zpath, *z = s + 1;
  memset(zpath, 0, sizeof(zpath));
  memcpy(zpath + 1, path, path_len);
  while (*z) {
    char *p = strchr(z, '/');
    if (p == NULL) {
      if (strcmp(z + 1, "..") == 0) {
        p = z + strlen(z) - 1;
        while (p > s) {
          if (*p == '/') {
            *p = 0;
            break;
          }
          p--;
        }
      }
      break;
    } else {
      if (strncmp(z, "..", p-z-1) == 0) {
        char *t = z - 2;
        while (t > s) {
          if (*t == '/') {
            strcpy(t, p);
            break;
          }
          t--;
        }
      }
    }
    z = p + 1;
  }

#if 0
  const char *s = keep_alive ? 
    "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nhello world\n" :
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nhello world\n";
  neco_write(conn, s, strlen(s));
#else
  zpath[0] = '.';
  if (zpath[strlen(zpath)-1] == '/')
    strcat(zpath, "index.html");
  size_t zlen = strlen(zpath);

  struct stat st;
  if (stat(zpath, &st) != 0) {
    const char *s = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nNot Found\n";
    neco_write(conn, s, strlen(s));
    close(conn);
    return;
  }

  char *ct = "application/octet-stream";
  const int num_types = sizeof(mime_types) / sizeof(mime_types[0]);
  char *p = zpath + zlen;
  for (i = 0; i < num_types; i++) {
    if (strcmp(p - strlen(mime_types[i].ext), mime_types[i].ext) == 0) {
      ct = mime_types[i].type;
      break;
    }
  }

  int f = open(zpath, O_RDONLY);
  if (f == -1) {  
    const char *s = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nNot Found\n";
    neco_write(conn, s, strlen(s));
    close(conn);
    return;
  }
  snprintf(buf, sizeof(buf),
      keep_alive ?
      "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n" :
      "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
      ct,
      st.st_size);
  neco_write(conn, buf, strlen(buf));
  off_t total = st.st_size;
  while (total > 0) {
    ssize_t n = neco_read(f, buf, sizeof(buf));
    if (n < 0) break;
    total -= n;
    if (neco_write(conn, buf, n) < 0) break;
  }
  close(f);
#endif

  if (keep_alive)
    goto retry;
  close(conn);
}

int neco_main(int argc, char *argv[]) {
  int ln = neco_serve("tcp", "0.0.0.0:8888");
  if (ln == -1) {
    perror("neco_serve");
    exit(1);
  }
  printf("listening at :8888\n");
  while (1) {
    int conn = neco_accept(ln, NULL, NULL);
    if (conn > 0) {
      neco_start(client, 1, &conn);
    }
  }
  close(ln);
  return 0;
}
