/* library */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "rterm.h"
#include "listc.h"
#include "scbuf.h"
#include "opfile.h"
#include "uintf.h"
#include "fileb.h"
#include "keyb.h"
#include "tm.h"
/* define */
#define CTRL_KEY(k) ((k)&0x1f)
#define SWIFT_VERSION "0.0.1"

//DROP-DOWN MENUS
#define OPTION_1 0
#define OPTION_2 1
#define OPTION_3 2
#define OPTION_4 3
#define OPTION_5 4
#define OPTION_NIL -1       //Reset option
#define CONFIRMATION 1
#define STATUS_BAR_MSG1  " [C-Edit] | F2,CTRL+L: MENU | F1: HELP"
#define STATUS_BAR_MSG2 " [C-Edit] Press ESC to exit menu.             "
#define STATUS_BAR_MSG3 " ENTER: SELECT | <- -> ARROW KEYS             "
#define ROWS_FAILSAFE 25
int     rows = 0, columns = 0, old_rows = 0, old_columns = 0;
char    kglobal = 0;   

enum editorKey {
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
typedef struct erow {
  int size;
  char *chars;
} erow;


struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  struct termios orig_termios;
};
struct editorConfig E;

struct termios orig_termios;
LISTCHOICE *mylist, data; //menus handler

/* terminal */
void die(const char *s)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode()
{
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_iflag &= ~(ICRNL | IXON);
  raw.c_cflag |= (CS8);
  raw.c_oflag &= ~(OPOST);
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
   struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/
void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}
void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
//   editorUpdateRow(row);
}

/*** editor operations ***/
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

/*** file i/o ***/
void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}


/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}


/*** input ***/
void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy++;
      }
      break;
  }
}
void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case '\r':
      /* TODO */
      break;
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      E.cx = E.screencols - 1;
      break;
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      /* TODO */
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
    case CTRL_KEY('l'):
    case '\x1b':
      break;
    default:
      editorInsertChar(c);
      break;
  }
}

/* output */
void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Swift editor -- version %s", SWIFT_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[y].size;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, E.row[y].chars, len);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}
void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);
  editorDrawRows(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}
/* init */
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}
/* main function */
int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}

void    cleanStatusBar();
void    filemenu();
void    optionsmenu();
void    helpmenu();


/*-------------*/
/*  File menu  */
/*-------------*/

void filemenu() {
  data.index = OPTION_NIL;
  write_str(1, rows, STATUS_BAR_MSG2, STATUSBAR, STATUSMSG);
  loadmenus(mylist, FILE_MENU);
  write_str(1, 1, "File", MENU_SELECTOR, MENU_FOREGROUND1);
  draw_window(1, 2, 13, 8, MENU_PANEL, MENU_FOREGROUND0,0, 1,0);
  kglobal = start_vmenu(&data);
  close_window();
  write_str(1, 1, "File  Options  Help", MENU_PANEL, MENU_FOREGROUND0);
  write_str(1, 1, "F", MENU_PANEL, F_RED);
  write_str(8, 1, "p", MENU_PANEL, F_RED);
  write_str(16, 1, "H", MENU_PANEL, F_RED);
  update_screen();
  free_list(mylist);

  if(data.index == OPTION_1) {
    

  }
  if(data.index == OPTION_2) {
    
  }
  if(data.index == OPTION_3) {
    
  }

  if(data.index == OPTION_4) {
    
  }

  if(data.index == OPTION_5) {
   
  }
  data.index = OPTION_NIL;
  cleanStatusBar();
   //Restore message in status bar
   write_str(1, rows, STATUS_BAR_MSG1, STATUSBAR, STATUSMSG);
}

/*--------------------------*/
/* Display Options menu     */
/*--------------------------*/

void optionsmenu() {
  int  setColor;
  data.index = OPTION_NIL;
  cleanStatusBar();
  write_str(1, rows, STATUS_BAR_MSG2, STATUSBAR, STATUSMSG);
  loadmenus(mylist, OPT_MENU);
  write_str(7, 1, "Options", MENU_SELECTOR, MENU_FOREGROUND1);
  draw_window(7, 2, 20, 6, MENU_PANEL, MENU_FOREGROUND0,0, 1,0);
  kglobal = start_vmenu(&data);
  close_window();
  write_str(1, 1, "File  Options  Help", MENU_PANEL, MENU_FOREGROUND0);
  write_str(1, 1, "F", MENU_PANEL, F_RED);
  write_str(8, 1, "p", MENU_PANEL, F_RED);
  write_str(16, 1, "H", MENU_PANEL, F_RED);
  update_screen();

  free_list(mylist);
  if(data.index == OPTION_1) {
   
  }
  if(data.index == OPTION_3) {
  
  }
  data.index = OPTION_NIL;
  //Restore message in status bar
  cleanStatusBar();
  write_str(1, rows, STATUS_BAR_MSG1, STATUSBAR, STATUSMSG);

}

/*--------------------------*/
/* Display Help menu        */
/*--------------------------*/

void helpmenu() {
  cleanStatusBar();
  data.index = OPTION_NIL;
  write_str(1, rows, STATUS_BAR_MSG2, STATUSBAR, STATUSMSG);
  loadmenus(mylist, HELP_MENU);
  write_str(16, 1, "Help", MENU_SELECTOR, MENU_FOREGROUND1);
  draw_window(16, 2, 26, 5, MENU_PANEL, MENU_FOREGROUND0, 0,1,0);
  kglobal = start_vmenu(&data);
  close_window();
  write_str(1, 1, "File  Options  Help", MENU_PANEL, MENU_FOREGROUND0);
  write_str(1, 1, "F", MENU_PANEL, F_RED);
  write_str(8, 1, "p", MENU_PANEL, F_RED);
  write_str(16, 1, "H", MENU_PANEL, F_RED);
  update_screen();
  free_list(mylist);
  if(data.index == OPTION_1) {
    
  }
  if(data.index == OPTION_2) {
   
  }
  data.index = -1;
  //Restore message in status bar
  cleanStatusBar();
  write_str(1, rows, STATUS_BAR_MSG1, STATUSBAR, STATUSMSG);
}