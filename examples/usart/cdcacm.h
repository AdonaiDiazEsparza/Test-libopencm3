#ifndef CDCACM_H
#define CDCACM_H

#include <stddef.h>

void cdcacm_init(void);
void cdcacm_poll(void);

int cdcacm_write(const void *data, size_t len);
int cdcacm_print(const char *s);
int cdcacm_puts(const char *s);
int cdcacm_read(void *data, size_t maxlen);

#endif
