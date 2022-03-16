#ifndef buffer_H
#define buffer_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*** append buffer ***/
struct abuf
{
  char *b;
  int len;
};

#define ABUF_INIT \
  {               \
    NULL, 0       \
  }

void abAppend(struct abuf *ab, const char *s, int len);

void abFree(struct abuf *ab);
#endif
