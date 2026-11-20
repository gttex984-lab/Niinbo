// Cliente WebSocket minimo (sem TLS, sem fragmentacao), so IPv4 por IP.
#include "ws.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<sys/socket.h>)
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <lwip/sockets.h>
#include <lwip/inet.h>
#define close lwip_close
#endif

#define CAP (1 << 20)
static int fd = -1;
static uint8_t *buf;

static int rd(void *p, size_t n) {
  uint8_t *q = p;
  while (n) { int r = recv(fd, q, n, 0); if (r <= 0) return -1; q += r; n -= r; }
  return 0;
}
void ws_close(void) { if (fd >= 0) close(fd); fd = -1; }

int ws_open(const char *ip, int port) {
  struct sockaddr_in a; memset(&a, 0, sizeof a);
  fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
  a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = inet_addr(ip);
  if (connect(fd, (struct sockaddr *)&a, sizeof a) < 0) { ws_close(); return -1; }
  char rq[256];
  int n = snprintf(rq, sizeof rq, "GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n"
    "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n", ip, port);
  if (send(fd, rq, n, 0) < 0) { ws_close(); return -1; }
  char h[512]; int l = 0;
  while (l < (int)sizeof h - 1) {
    if (recv(fd, h + l, 1, 0) <= 0) { ws_close(); return -1; }
    h[++l] = 0; if (l >= 4 && !strcmp(h + l - 4, "\r\n\r\n")) break;
  }
  if (!strstr(h, " 101 ")) { ws_close(); return -1; }
  if (!buf) buf = malloc(CAP);
  return buf ? 0 : -1;
}

static int frame(int op, const void *d, size_t n) {   // mascara zero e valida e evita XOR
  uint8_t h[8]; int k = 0;
  h[k++] = 0x80 | op;
  if (n < 126) h[k++] = 0x80 | n; else { h[k++] = 0x80 | 126; h[k++] = n >> 8; h[k++] = n; }
  memset(h + k, 0, 4); k += 4;
  if (send(fd, h, k, 0) < 0) return -1;
  return n && send(fd, d, n, 0) < 0 ? -1 : 0;
}
int ws_send(const char *t) { return fd < 0 ? -1 : frame(1, t, strlen(t) > 60000 ? 60000 : strlen(t)); }

int ws_poll(int ms, ws_msg *m) {
  if (fd < 0) return -1;
  fd_set s; FD_ZERO(&s); FD_SET(fd, &s);
  struct timeval t = { ms / 1000, (ms % 1000) * 1000 };
  int r = select(fd + 1, &s, 0, 0, &t);
  if (r < 0) return -1;
  if (!r) return 0;
  uint8_t h[2]; if (rd(h, 2)) return -1;
  int op = h[0] & 15; uint64_t n = h[1] & 127;
  if (n == 126) { uint8_t e[2]; if (rd(e, 2)) return -1; n = e[0] << 8 | e[1]; }
  else if (n == 127) { uint8_t e[8]; if (rd(e, 8)) return -1; n = 0; for (int i = 0; i < 8; i++) n = n << 8 | e[i]; }
  if (n >= CAP) return -1;
  if (rd(buf, n)) return -1;
  buf[n] = 0;
  if (op == 9) { frame(10, buf, n); return 0; }
  if (op == 8) return -1;
  m->op = op; m->data = buf; m->len = n; return 1;
}
