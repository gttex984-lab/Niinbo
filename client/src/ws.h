#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { int op; uint8_t *data; size_t len; } ws_msg;  // op 1 = texto, 2 = binario
int  ws_open(const char *ip, int port);
int  ws_send(const char *text);
int  ws_poll(int ms, ws_msg *m);    // 1 = mensagem, 0 = nada, -1 = caiu
void ws_close(void);
