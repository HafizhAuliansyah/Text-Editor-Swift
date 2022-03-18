#ifndef editor_H
#define editor_H

/* library */
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include "buffer.h"
/* define */
#define CTRL_KEY(k) ((k)&0x1f)
#define SWIFT_TAB_STOP 8
#define SWIFT_VERSION "0.0.1"
#define SWIFT_QUIT_TIMES 1

#define MAX_COLUMN 10
#define MAX_ROW 10

enum editorKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/* data */
typedef struct erow
{
    int size;
    int rsize;
    char chars[MAX_COLUMN];
    char render[MAX_COLUMN];
} erow;

struct editorConfig
{
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow row[MAX_ROW];
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};
struct cursorHandler{
	int x;
	int y;
    int rx;
};
struct selection{
    int x;
    int y;
    int len;
    bool isOn;
};

struct editorConfig E;

/* terminal */
void die(const char *s);

void disableRawMode();

void enableRawMode();

int editorReadKey();

int getCursorPosition(int *rows, int *cols);

int getWindowSize(int *rows, int *cols);

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx);
/* melakukan rendering pada tab berapa jarak untuk setiap ketika
kita menekan tab */

int editorRowRxToCx (erow *row,int rx);


void editorUpdateRow(erow *row);
/* ngatur untuk apa yang d render atau ditampilkan ke layar */

void editorInsertRow(int at, char *s, size_t len);
/* masukin ke array nya harusnya lewat sini tapi per row */

// void editorFreeRow(erow *row);
/* nge dealloc di memory sebesar erow*/

void editorDelRow(int at);
/* kayak editordelchar tapi ini ma kalau di awal row*/

void editorRowInsertChar(erow *row, int at, int c);
/* menambahkan karakter to posisi cursor */

void editorRowAppendString(erow *row, char *s, size_t len);
/* menambahkan row ke row baru setelah dihapus biasanya*/

void editorRowDelChar(erow *row, int at);
/* menghapus 1 karakter posisi cursor*/

/*** editor operations ***/
void editorInsertChar(int c);
/* menambahkan satu karakter dari inputan lalu akan manggil editorrow insert char untuk dimasukan dalam row*/

void editorDelChar();
/* menghapus 1 karakter lalu manggil editrRowDelChar */

void editorInsertNewline();
/* handle enter*/

/*** file i/o ***/
char *editorRowsToString(int *buflen);

void editorOpen(char *filename);

void editorSave();

/** Find **/
void editorFind();

/*** input ***/
char *editorPrompt(char *prompt, int start_cx);

void editorMoveCursor(int key);

void editorProcessKeypress();

/* output */
void editorScroll();

void editorDrawRows(struct abuf *ab);

void editorDrawStatusBar(struct abuf *ab);

void editorDrawMessageBar(struct abuf *ab);

void editorRefreshScreen();

void editorSetStatusMessage(const char *fmt, ...);

void addSelectionText(struct abuf *ab, char *row, int len);

/* init */
void initEditor();

#endif
