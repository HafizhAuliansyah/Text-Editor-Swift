#ifndef text_op_h
#define text_op_h
#include "buffer.h"
#include "editor.h"
#include <stdbool.h>

struct selection{
    int x;
    int y;
    int len;
    bool isOn;
};
struct selection selection;

void addSelectionText(struct abuf *ab, char *row, int len);
void clearSelected(struct selection *s);
void copy(erow row[]);

#endif