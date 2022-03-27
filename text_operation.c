#include "text_operation.h"

char *hasil_c;
void addSelectionText(struct abuf *ab, char *row, int len)
{
    // var at, sebagai penampung koordinat kolom
    int at = 0;
    // Memasukkan kolom sebelum kata terselect ke ab
    abAppend(ab, &row[at], selection.x);
    // Select Text sesuai kolom selection.x, sejumlah selection.len ke kanan
    abAppend(ab, "\x1b[7m", 4);
    at = selection.x;
    abAppend(ab, &row[at], selection.len);
    abAppend(ab, "\x1b[m", 3);
    // Memasukkan kolom setelah kata terselect ke ab
    at = selection.x + selection.len;
    abAppend(ab, &row[at], len - at);
}
void selectMoveCursor(int c)
{
    // editorSetStatusMessage("%d cx, %d size", C.x,E.row[C.y].size);
    struct selection dest;
    dest.x = C.x;
    dest.y = C.y;
    dest.len = selection.len;
    switch (c)
    {
    case SHIFT_ARROW_LEFT:
        if (C.x == 0)
            return;
        editorMoveCursor(ARROW_LEFT);
        dest.x = C.x;
        dest.len++;
        break;
    case SHIFT_ARROW_RIGHT:
        if (C.x >= E.row[C.y].size)
            return;
        dest.x = C.x - dest.len;
        editorMoveCursor(ARROW_RIGHT);
        dest.len++;
        break;
    case SHIFT_ARROW_UP:
    case SHIFT_ARROW_DOWN:
        editorSetStatusMessage("FITUR INI BELUM TERSEDIA");
        break;
    default:
        editorSetStatusMessage("Other");
        break;
    }
    if (dest.y != C.y)
    {
        editorSetStatusMessage("FITUR INI BELUM TERSEDIA");
    }
    selectShift(dest);
}
void selectShift(struct selection dest)
{
    selection.x = dest.x;
    selection.y = dest.y;
    selection.len = dest.len;
    selection.isOn = true;
}
void clearSelected(struct selection *s)
{
    s->x = -1;
    s->y = -1;
    s->len = 0;
    s->isOn = false;
}

void copy(erow row[])
{
    hasil_c = realloc(hasil_c, selection.len + 1);
    memmove(hasil_c, &row[selection.y].chars[selection.x], selection.len);
    hasil_c[selection.len] = '\0';
}

void paste()
{
    int column_len = MAX_COLUMN - C.x;
    for (int x = 0; x < strlen(hasil_c); x++)
        editorInsertChar(hasil_c[x]);
}