// Camada de plataforma: tudo que depende do console fica aqui.
#pragma once
#include <stdint.h>
#include <stddef.h>
enum { B_UP=1, B_DOWN=2, B_LEFT=4, B_RIGHT=8, B_A=16, B_B=32, B_X=64, B_Y=128,
       B_LB=256, B_RB=512, B_START=1024, B_BACK=2048, B_L3=4096, B_R3=8192 };
typedef struct { uint32_t btn; int lx, ly, rx, ry; int lt, rt; } pad_t;  // sticks -32768..32767, gatilhos 0..255
void     plat_init(void);
uint32_t plat_ms(void);
void     plat_sleep_ms(int ms);
int      plat_pad(pad_t *p);                       // 1 se ha controle
void     plat_draw_jpeg(const uint8_t *d, size_t n); // decodifica para o buffer de tras
void     plat_clear(void);
void     plat_line(const char *s, int highlight);  // linha de texto (menus)
void     plat_present(void);
void     plat_audio(const uint8_t *pcm, size_t n); // s16le estereo 24000 Hz
const char *plat_data_dir(void);                   // pasta com nimbo.cfg e token.txt
