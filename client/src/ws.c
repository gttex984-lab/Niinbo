// Cliente WebSocket minimo sobre a API "raw" TCP do lwIP (sem sockets).
#include "ws.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <network/network.h>
#include <lwip/tcp.h>

#define CAP  (1 << 20)      // maior mensagem aceita
#define RING (1 << 18)      // fila de recepcao (potencia de 2)
static struct tcp_pcb *pcb;
static volatile int state;  // 0 fechado, 1 conectando, 2 conectado
static uint8_t *buf, *ring;
static size_t rh, rt, rn;   // cabeca, cauda, quantidade na fila

static err_t on_recv(void *arg, struct tcp_pcb *p, struct pbuf *pb, err_t err) {
  if (!pb) { state = 0; return ERR_OK; }            // servidor fechou
  if (err != ERR_OK) { pbuf_free(pb); return err; }
  for (struct pbuf *q = pb; q; q = q->next) {
    if (rn + q->len > RING) { state = 0; break; }
    for (size_t i = 0; i < q->len; i++) { ring[rt] = ((uint8_t *)q->payload)[i]; rt = (rt + 1) & (RING - 1); }
    rn += q->len;
  }
  pbuf_free(pb);
  return ERR_OK;
}
static void on_err(void *arg, err_t e) { pcb = NULL; state = 0; }
static err_t on_conn(void *arg, struct tcp_pcb *p, err_t err) { state = (err == ERR_OK) ? 2 : 0; return ERR_OK; }

void ws_close(void) {
  if (pcb) {
    tcp_arg(pcb, NULL); tcp_recv(pcb, NULL); tcp_err(pcb, NULL);
    if (tcp_close(pcb) != ERR_OK) tcp_abort(pcb);
    pcb = NULL;
  }
  state = 0; rh = rt = rn = 0;
}

static int raw_send(const void *d, size_t n) {
  const uint8_t *p = d; uint32_t t0 = plat_ms();
  while (n) {
    if (!pcb || state != 2) return -1;
    size_t room = tcp_sndbuf(pcb), k = n;
    if (k > room) k = room;
    if (k > 60000) k = 60000;
    if (!k || tcp_write(pcb, p, (u16_t)k, TCP_WRITE_FLAG_COPY) != ERR_OK) {
      network_poll();
      if ((int)(plat_ms() - t0) > 5000) return -1;
      continue;
    }
    tcp_output(pcb); p += k; n -= k; t0 = plat_ms();
  }
  return 0;
}

static int rd(void *p, size_t n) {
  uint8_t *q = p; uint32_t t0 = plat_ms();
  while (n) {
    if (!rn) {
      network_poll();
      if (state != 2 || (int)(plat_ms() - t0) > 5000) return -1;
      continue;
    }
    size_t k = rn < n ? rn : n;
    for (size_t i = 0; i < k; i++) { q[i] = ring[rh]; rh = (rh + 1) & (RING - 1); }
    rn -= k; q += k; n -= k; t0 = plat_ms();
    for (size_t left = k; left && pcb;) {          // libera a janela de recepcao
      u16_t c = left > 60000 ? 60000 : (u16_t)left;
      tcp_recved(pcb, c); left -= c;
    }
  }
  return 0;
}

int ws_open(const char *ip, int port) {
  unsigned a, b, c, d;
  if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return -1;
  if (!buf) buf = malloc(CAP);
  if (!ring) ring = malloc(RING);
  if (!buf || !ring) return -1;
  ws_close();
  ip_addr_t addr;
#ifdef IP_ADDR4
  IP_ADDR4(&addr, a, b, c, d);
#else
  IP4_ADDR(&addr, a, b, c, d);
#endif
  pcb = tcp_new(); if (!pcb) return -1;
  tcp_err(pcb, on_err); tcp_recv(pcb, on_recv);
  state = 1;
  if (tcp_connect(pcb, &addr, port, on_conn) != ERR_OK) { ws_close(); return -1; }
  uint32_t t0 = plat_ms();
  while (state == 1 && (int)(plat_ms() - t0) < 5000) network_poll();
  if (state != 2) { ws_close(); return -1; }
  char rq[256];
  int n = snprintf(rq, sizeof rq, "GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n"
    "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n", ip, port);
  if (raw_send(rq, n)) { ws_close(); return -1; }
  char h[512]; int l = 0;
  while (l < (int)sizeof h - 1) {
    if (rd(h + l, 1)) { ws_close(); return -1; }
    h[++l] = 0; if (l >= 4 && !strcmp(h + l - 4, "\r\n\r\n")) break;
  }
  if (!strstr(h, " 101 ")) { ws_close(); return -1; }
  return 0;
}

static int frame(int op, const void *d, size_t n) {   // mascara zero e valida e evita XOR
  uint8_t h[8]; int k = 0;
  h[k++] = 0x80 | op;
  if (n < 126) h[k++] = 0x80 | n; else { h[k++] = 0x80 | 126; h[k++] = n >> 8; h[k++] = n; }
  memset(h + k, 0, 4); k += 4;
  if (raw_send(h, k)) return -1;
  return n ? raw_send(d, n) : 0;
}
int ws_send(const char *t) {
  size_t n = strlen(t);
  return (!pcb || state != 2) ? -1 : frame(1, t, n > 60000 ? 60000 : n);
}

int ws_poll(int ms, ws_msg *m) {
  if (!pcb && !rn) return -1;
  uint32_t t0 = plat_ms();
  do { network_poll(); } while (!rn && state && (int)(plat_ms() - t0) < ms);
  if (!rn) return state == 2 ? 0 : -1;
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
