#include "text_operation.h"
char *hasil_c;
void addSelectionText(struct abuf *ab, char *row, int len){
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
void clearSelected(struct selection *s){
    s->x = -1;
    s->y = -1;
    s->len = 0;
    s->isOn = false;
}
void copy(erow row[]){
    hasil_c = realloc(hasil_c , selection.len+1);
    memmove(hasil_c, &row[selection.y].chars[selection.x], selection.len);
    hasil_c[selection.len] = '\0';
}