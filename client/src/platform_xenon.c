// Plataforma libxenon (Xbox 360 RGH/JTAG). ATENCAO: escrito de memoria, nao
// foi testado. Pontos marcados VERIFICAR dependem da versao da libxenon.
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xenos/xenos.h>
#include <console/console.h>
#include <usb/usbmain.h>
#include <input/input.h>
#include <network/network.h>
#include <xenon_sound/sound.h>
#include <time/time.h>
#include <ppc/timebase.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#include "stb_image.h"
#define VW 1280
#define VH 720
static uint32_t *rgba;        // quadro decodificado (ARGB)

void plat_init(void) {
  xenos_init(VIDEO_MODE_AUTO); console_init();
  usb_init(); usb_do_poll(); network_init(); xenon_sound_init();   // VERIFICAR nomes
  rgba = malloc(VW * VH * 4);
}
uint32_t plat_ms(void) { return mftb() / (PPC_TIMEBASE_FREQ / 1000); }   // VERIFICAR constante
void plat_sleep_ms(int ms) { mdelay(ms); network_poll(); }
const char *plat_data_dir(void) { return "uda:/XBOX 360/nimbo/"; }       // VERIFICAR: nome do volume (hdd1/uda)

int plat_pad(pad_t *p) {
  struct controller_data_s c; usb_do_poll(); network_poll();
  if (get_controller_data(&c, 0) == 0) return 0;                         // VERIFICAR retorno
  p->btn = (c.up ? B_UP : 0) | (c.down ? B_DOWN : 0) | (c.left ? B_LEFT : 0) | (c.right ? B_RIGHT : 0)
    | (c.a ? B_A : 0) | (c.b ? B_B : 0) | (c.x ? B_X : 0) | (c.y ? B_Y : 0)
    | (c.lb ? B_LB : 0) | (c.rb ? B_RB : 0) | (c.start ? B_START : 0) | (c.back ? B_BACK : 0)
    | (c.s1_z ? B_L3 : 0) | (c.s2_z ? B_R3 : 0);
  p->lx = c.s1_x; p->ly = c.s1_y; p->rx = c.s2_x; p->ry = c.s2_y; p->lt = c.lt; p->rt = c.rt;
  return 1;
}

void plat_draw_jpeg(const uint8_t *d, size_t n) {
  int w, h, c;
  unsigned char *px = stbi_load_from_memory(d, (int)n, &w, &h, &c, 4);
  if (!px) return;
  for (int y = 0; y < h && y < VH; y++)
    for (int x = 0; x < w && x < VW; x++) {
      unsigned char *p = px + (y * w + x) * 4;
      rgba[y * VW + x] = 0xFF000000 | p[0] << 16 | p[1] << 8 | p[2];
    }
  stbi_image_free(px);
}
void plat_clear(void) { console_clrscr(); }
void plat_line(const char *s, int hl) { printf(hl ? "> %s\n" : "  %s\n", s); }
void plat_present(void) {
  // TODO (parte que exige o console): subir `rgba` como textura Xenos e desenhar
  // um quad de tela cheia, como no exemplo de textura da libxenon. Nao implementado.
}
void plat_audio(const uint8_t *pcm, size_t n) {
  // Audio do servidor: s16le estereo 24000 Hz. Saida da libxenon: 48000 Hz,
  // entao duplicamos cada amostra. VERIFICAR ordem de bytes (PPC e big-endian).
  static int16_t out[1 << 16];
  size_t ns = n / 4;
  if (ns > (sizeof out / sizeof *out) / 4) ns = (sizeof out / sizeof *out) / 4;
  for (size_t i = 0; i < ns; i++) {
    int16_t l = pcm[i * 4] | pcm[i * 4 + 1] << 8, r = pcm[i * 4 + 2] | pcm[i * 4 + 3] << 8;
    out[i * 4] = out[i * 4 + 2] = l;
    out[i * 4 + 1] = out[i * 4 + 3] = r;
  }
  xenon_sound_submit(out, ns * 8);
}
