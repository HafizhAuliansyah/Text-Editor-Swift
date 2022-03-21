#ifndef text_op_h
#define text_op_h
#include "buffer.h"

struct selection{
    int x;
    int y;
    int len;
    bool isOn;
};

void addSelectionText(struct abuf *ab, char *row, int len);

#endif