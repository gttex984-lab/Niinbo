// nimbo (cliente Xbox 360): conecta, vincula conta por codigo, lista apps,
// mostra o video, toca o audio e envia o controle. Reconecta e retoma sozinho.
#include "platform.h"
#include "ws.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

enum { S_CONNECT, S_PAIR, S_MENU, S_STREAM, S_OSK };
static char host[64] = "192.168.0.10", token[80], lastapp[24], status[80], code[16], path[200];
static int port = 8080, state = S_CONNECT, nreq, resume;
static char aid[16][24], aname[16][32];
static int napps, sel;

static void tx(const char *f, ...) {
  char b[256]; va_list v; va_start(v, f); vsnprintf(b, sizeof b, f, v); va_end(v);
  ws_send(b);
}
static void load_cfg(void) {
  snprintf(path, sizeof path, "%snimbo.cfg", plat_data_dir());
  FILE *f = fopen(path, "r");
  if (f) { fscanf(f, "%63s %d", host, &port); fclose(f); }
  snprintf(path, sizeof path, "%stoken.txt", plat_data_dir());
  if ((f = fopen(path, "r"))) { fscanf(f, "%79s", token); fclose(f); }
}
static void save_token(void) {
  snprintf(path, sizeof path, "%stoken.txt", plat_data_dir());
  FILE *f = fopen(path, "w"); if (f) { fputs(token, f); fclose(f); }
}

static void on_msg(ws_msg *m) {
  char *s = (char *)m->data;
  if (m->op == 2) {
    if (m->len < 2) return;
    if (m->data[0] == 1) { plat_draw_jpeg(m->data + 1, m->len - 1); nreq = 1; }
    else if (m->data[0] == 2) plat_audio(m->data + 1, m->len - 1);
    return;
  }
  if (!strncmp(s, "CODE ", 5)) { snprintf(code, sizeof code, "%s", s + 5); state = S_PAIR; }
  else if (!strncmp(s, "OK ", 3)) { snprintf(token, sizeof token, "%s", s + 3); save_token(); tx("LIST"); }
  else if (!strcmp(s, "READY")) tx("LIST");
  else if (!strcmp(s, "ERR auth")) { token[0] = 0; tx("PAIR"); }
  else if (!strncmp(s, "ERR ", 4)) snprintf(status, sizeof status, "%s", s + 4);
  else if (!strncmp(s, "APPS\n", 5)) {
    napps = 0; char *ln = strtok(s + 5, "\n");
    while (ln && napps < 16) {
      char *bar = strchr(ln, '|');
      if (bar) { *bar = 0; snprintf(aid[napps], 24, "%s", ln); snprintf(aname[napps], 32, "%s", bar + 1); napps++; }
      ln = strtok(NULL, "\n");
    }
    if (resume && lastapp[0]) { tx("OPEN %s", lastapp); state = S_STREAM; } else state = S_MENU;
    resume = 0;
  }
}

static const char *KEYS[] = { "up", "down", "left", "right", "a", "b", "x", "y", "lb", "rb", "start", "back" };
static const uint32_t KB[] = { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_LB, B_RB, B_START, B_BACK };

// teclado na tela: LT abre. A = letra, X = apagar, Y = espaco, RB = digitar, START = digitar + Enter, B = fechar
static const char *ROWS[] = { "abcdefghij", "klmnopqrst", "uvwxyz.-_@", "0123456789", "/:?&=+!#%~" };
static char osk[100]; static int ox, oy;
static void draw_osk(void) {
  char l[64]; plat_line("Digitar (A letra, X apagar, Y espaco, RB digita, START envia, B fecha)", 0);
  snprintf(l, sizeof l, "> %s_", osk); plat_line(l, 0);
  for (int r = 0; r < 5; r++) {
    char b[64]; int k = 0;
    for (int c = 0; c < 10; c++) { b[k++] = (r == oy && c == ox) ? '[' : ' '; b[k++] = ROWS[r][c]; b[k++] = (r == oy && c == ox) ? ']' : ' '; }
    b[k] = 0; plat_line(b, 0);
  }
}

static void draw_menu(void) {
  plat_clear(); plat_line("nimbo - apps na nuvem", 0); plat_line("", 0);
  if (state == S_CONNECT) { plat_line("Conectando ao servidor...", 0); plat_line(host, 0); }
  if (state == S_PAIR) {
    char l[80]; plat_line("Vincule sua conta:", 0);
    snprintf(l, sizeof l, "No celular abra http://%s:%d/link", host, port); plat_line(l, 0);
    snprintf(l, sizeof l, "Digite o codigo: %s", code); plat_line(l, 1);
  }
  if (state == S_MENU) { for (int i = 0; i < napps; i++) plat_line(aname[i], i == sel); plat_line("", 0); plat_line("A abre - Back+R3 volta para este menu", 0); }
  if (status[0]) plat_line(status, 0);
  plat_present();
}

int main(void) {
  plat_init(); load_cfg();
  uint32_t prev = 0, rep = 0, lastmouse = 0; int prevrt = 0, prevlt = 0;
  for (;;) {
    if (state == S_CONNECT) {
      draw_menu();
      if (ws_open(host, port) == 0) { status[0] = 0; if (token[0]) tx("HELLO %s", token); else tx("PAIR"); state = S_PAIR; code[0] = 0; }
      else { snprintf(status, sizeof status, "Sem conexao. Tentando de novo..."); plat_sleep_ms(2000); continue; }
    }
    ws_msg m; int r, guard = 0, redraw = 0;
    while (guard++ < 8 && (r = ws_poll(guard == 1 ? 5 : 0, &m)) != 0) {
      if (r < 0) { ws_close(); resume = (state == S_STREAM || state == S_OSK); state = S_CONNECT; snprintf(status, sizeof status, "Rede caiu. Reconectando..."); break; }
      on_msg(&m); redraw = 1;
    }
    if (state == S_CONNECT) continue;

    pad_t p; uint32_t now = plat_ms();
    if (plat_pad(&p)) {
      uint32_t pressed = p.btn & ~prev;
      if (p.btn & (B_UP | B_DOWN | B_LEFT | B_RIGHT)) {       // repeticao ao segurar
        if (!(prev & p.btn & 15)) rep = now + 400;
        else if (now > rep) { pressed |= p.btn & 15; rep = now + 100; }
      }
      int rtp = p.rt > 128 && !prevrt, ltp = p.lt > 128 && !prevlt;
      prevrt = p.rt > 128; prevlt = p.lt > 128;

      if (state == S_MENU) {
        if ((pressed & B_UP) && sel > 0) sel--;
        if ((pressed & B_DOWN) && sel < napps - 1) sel++;
        if ((pressed & B_A) && napps) { snprintf(lastapp, sizeof lastapp, "%s", aid[sel]); tx("OPEN %s", lastapp); state = S_STREAM; }
        draw_menu();
      } else if (state == S_STREAM) {
        if ((p.btn & B_BACK) && (pressed & B_R3)) { tx("CLOSE"); lastapp[0] = 0; state = S_MENU; }
        else {
          for (int i = 0; i < 12; i++) if ((pressed & KB[i]) && !(KB[i] == B_BACK && (p.btn & B_R3))) tx("KEY %s", KEYS[i]);
          int dx = abs(p.rx) > 6000 ? p.rx / 2500 : 0, dy = abs(p.ry) > 6000 ? -p.ry / 2500 : 0;
          if ((dx || dy) && now - lastmouse > 40) { tx("MOUSE %d %d", dx, dy); lastmouse = now; }
          if (rtp) tx("CLICK");
          if (ltp) { state = S_OSK; osk[0] = 0; ox = oy = 0; }
        }
      } else if (state == S_OSK) {
        int n = strlen(osk);
        if ((pressed & B_LEFT) && ox > 0) ox--;
        if ((pressed & B_RIGHT) && ox < 9) ox++;
        if ((pressed & B_UP) && oy > 0) oy--;
        if ((pressed & B_DOWN) && oy < 4) oy++;
        if ((pressed & B_A) && n < 98) { osk[n] = ROWS[oy][ox]; osk[n + 1] = 0; }
        if ((pressed & B_Y) && n < 98) { osk[n] = ' '; osk[n + 1] = 0; }
        if ((pressed & B_X) && n) osk[n - 1] = 0;
        if (pressed & B_RB) { tx("TEXT %s", osk); state = S_STREAM; }
        if (pressed & B_START) { tx("TEXTE %s", osk); state = S_STREAM; }
        if (pressed & B_B) state = S_STREAM;
        redraw = 1;
      }
      prev = p.btn;
    }
    if (state == S_PAIR || state == S_CONNECT) draw_menu();
    else if (state == S_STREAM && nreq) { plat_present(); nreq = 0; }
    else if (state == S_OSK && redraw) { draw_osk(); plat_present(); }
  }
}
