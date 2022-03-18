#include "editor.h"

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
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

int editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }
        return '\x1b';
    }
    else
    {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (SWIFT_TAB_STOP - 1) - (rx % SWIFT_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxToCx (erow *row,int rx) {
	int cur_rx = 0;
	int cx;
	for (cx = 0; cx < row->size; cx++){
		if (row->chars[cx] == '\t')
			cur_rx += (SWIFT_TAB_STOP) - (cur_rx % SWIFT_TAB_STOP);
		cur_rx++;
		
		if(cur_rx > rx) return cx;
	}
	return cx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;
    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % SWIFT_TAB_STOP != 0 && idx < MAX_COLUMN)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
        if (idx >= MAX_COLUMN)
            break;
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorInsertRow(int at, char *s, size_t len)
{
    if (at < 0 || at > E.numrows)
        return;

    if (E.numrows >= MAX_ROW)
        return;

    // erow *dest = &E.row[at + 1];
    // erow *src = &E.row[at];
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    E.row[at].size = len;
    memcpy(&E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render[0] = '\0';
    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

// void editorFreeRow(erow *row)
// {
// free(row->render);
// free(row->chars);
// }

void editorDelRow(int at)
{
    if (at < 0 || at >= E.numrows)
        return;
    // editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c)
{
    if (at < 0 || at > row->size)
        at = row->size;
    char *dest = row->chars;
    char *src = row->chars;
    memmove(dest + at + 1, src + at, row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(&(*row));
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len)
{
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(&(*row));
    E.dirty++;
}

void editorRowDelChar(erow *row, int at)
{
    if (at < 0 || at >= row->size)
        return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c)
{
    if (C.y == E.numrows)
    {
        editorInsertRow(E.numrows, "", 0);
    }
    if (E.row[C.y].size < MAX_COLUMN)
    {
        editorRowInsertChar(&E.row[C.y], C.x, c);
        C.x++;
    }
    else
    {
        editorSetStatusMessage("PERINGATAN ! MENCAPAI BATAS KOLOM");
    }
}

void editorDelChar()
{
    if (C.y == E.numrows)
        return;
    if (C.x == 0 && C.y == 0)
        return;

    // erow *row = ;
    if (C.x > 0)
    {
        editorRowDelChar(&E.row[C.y], C.x - 1);
        C.x--;
    }
    else
    {
        C.x = E.row[C.y - 1].size;
        if (E.row[C.y - 1].size + E.row[C.y].size <= MAX_COLUMN)
        {
            editorRowAppendString(&E.row[C.y - 1], E.row[C.y].chars, E.row[C.y].size);
            editorDelRow(C.y);
            C.y--;
        }
        else
        {
            editorSetStatusMessage("PERINGATAN ! TIDAK BISA MENGHAPUS, KOLOM TIDAK MEMADAI");
            C.x = E.row[C.y].size;
        }
    }
}

void editorInsertNewline()
{
    if (E.numrows < MAX_ROW)
    {
        if (C.x == 0)
        {
            editorInsertRow(C.y, "", 0);
        }
        else
        {
            erow *row = &E.row[C.y];
            editorInsertRow(C.y + 1, &row->chars[C.x], row->size - C.x);
            row = &E.row[C.y];
            row->size = C.x;
            row->chars[row->size] = '\0';
            editorUpdateRow(&(*row));
        }
        C.y++;
        C.x = 0;
    }
    else
    {
        editorSetStatusMessage("PERINGATAN ! MENCAPAI BATAS BARIS");
    }
}

/*** file i/o ***/
char *editorRowsToString(int *buflen)
{
    int totlen = 0;
    int j;
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.numrows; j++)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void editorOpen(char *filename)
{
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n') || line[linelen - 1] == '\r')
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave()
{
    // Jika argumen filename kosong
    if (E.filename == NULL)
    {
        // Memasukkan nama file penyimpanan
        E.filename = editorPrompt("Nama File Penyimpanan : %s (ESC to cancel)");
        if (E.filename == NULL)
        {
            editorSetStatusMessage("GAGAL MENYIMPAN");
            return;
        }
    }
    int len;
    char *buf = editorRowsToString(&len);

    // Membuka file dengan Read & Write dan Permission Create File 0644
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    // Jika file yang dicari tersedia
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes saved to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Error saving file: %s", strerror(errno));
}

/*** input ***/
char *editorPrompt(char *prompt)
{
    // Deklarasi variabel penampung nama file, dengan size 128 bytes
    size_t name_size = 128;
    char *filename = malloc(name_size);

    // Inisialisasi awal size isi, dan zero character
    size_t name_len = 0;
    name_len[0] = '\0';

    while (1)
    {
        // Membuka status bar, untuk menerima input ke filename
        editorSetStatusMessage(prompt, filename);
        editorRefreshScreen();

        // Membaca input untuk mengisi file name
        int c = editorReadKey();

        // Mini delete character handler
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if (name_len != 0)
                filename[--name_len] = '\0';
        }
        // Escape, untuk keluar dari editor Prompt
        else if (c == '\x1b')
        {
            editorSetStatusMessage("");
            free(filename);
            return NULL;
        }
        // Memasukkan input ke filename
        else
        {
            // Enter, untuk menghentikan input
            if (c == '\r')
            {
                if (name_len != 0)
                {
                    editorSetStatusMessage("");
                    return name_len;
                }
            }
            // Jika input bukan control character, masukkan input ke filename
            else if (!iscntrl(c) && c < 128)
            {
                // Menambah size filename, ketika hampir penuh
                if (name_len == name_size - 1)
                {
                    name_size *= 2;
                    filename = realloc(filename, name_size);
                }
                // Mengisi filename
                filename[name_len++] = c;
                filename[name_len] = '\0';
            }
        }
    }
}

void editorMoveCursor(int key)
{
    // Cek apakah baris, dimana cursor berada tidak kosong
    erow *row = (C.y >= E.numrows) ? NULL : &E.row[C.y];
    switch (key)
    {
    case ARROW_LEFT:
        if (C.x != 0)
        {
            // Memindahkan kursor ke kiri 1 kolom
            C.x--;
        }
        else if (C.y > 0 && C.x == 0)
        {
            // Memindahkan kurosr ke baris sebelumnya, kolom terakhir
            C.y--;
            C.x = E.row[C.y].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && C.x < row->size)
        {
            // Memindahkan kursor ke kanan 1 kolom
            C.x++;
        }
        else if (row && C.x == row->size && C.y != E.numrows - 1)
        {
            // Memindahkan kursor ke baris setelahnya, kolom pertama
            C.y++;
            C.x = 0;
        }
        break;
    case ARROW_UP:
        if (C.y != 0)
        {
            // Memindahkan kursor turun 1 baris
            C.y--;
        }
        break;
    case ARROW_DOWN:
        if (C.y < E.numrows - 1)
        {
            // Memindahkan kursor naik 1 baris
            C.y++;
        }
        break;
    }

    // Error Handling, cursor melebihi jumlah kolom
    row = (C.y >= E.numrows) ? NULL : &E.row[C.y];
    int rowlen = row ? row->size : 0;
    if (C.x > rowlen)
    {
        C.x = rowlen;
    }

}

void editorProcessKeypress()
{
    static int quit_times = SWIFT_QUIT_TIMES;
    int c = editorReadKey();
    switch (c)
    {
    case '\r':
        editorInsertNewline();
        break;
    case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0)
        {
            editorSetStatusMessage("WARNING !! File has unsaved changes. Press Ctrl-Q %d more times to quit", quit_times);
            quit_times--;
            return;
        }
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case CTRL_KEY('s'):
        editorSave();
        break;
    case HOME_KEY:
        C.x = 0;
        break;
    case END_KEY:
        if (C.y < E.numrows)
            C.x = E.row[C.y].size;
        break;
    case CTRL_KEY('f'):
    	editorFind();
    	break;
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if (c == DEL_KEY)
            editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        if (c == PAGE_UP)
        {
            C.y = E.rowoff;
        }
        else if (c == PAGE_DOWN)
        {
            C.y = E.rowoff + E.screenrows - 1;
            if (C.y > E.numrows)
                C.y = E.numrows - 1;
        }
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
    // Arrow untuk memindahkan cursor
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
    quit_times = SWIFT_QUIT_TIMES;
}

/* output */
void editorScroll()
{
    // Tab Detector and Handler
    C.rx = 0;
    if (C.y < E.numrows)
    {
        C.rx = editorRowCxToRx(&E.row[C.y], C.x);
    }
    // Pengaturan row offset ketika scroll keatas
    if (C.y < E.rowoff)
    {
        E.rowoff = C.y;
    }
    // Pengaturan row offset ketika scroll kebawah
    if (C.y >= E.rowoff + E.screenrows)
    {
        E.rowoff = C.y - E.screenrows + 1;
    }
    // Pengaturan coll offset ketika scroll ke kiri
    if (C.rx < E.coloff)
    {
        E.coloff = C.rx;
    }
    // Pengaturan coll offset ketika scroll ke kanan
    if (C.rx >= E.coloff + E.screencols)
    {
        E.coloff = C.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows)
        {
            if (E.numrows == 0 && y == MAX_ROW / 2)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Swift editor -- version %s", SWIFT_VERSION);
                if (welcomelen > E.screencols)
                    welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding)
                {
                    if (y < MAX_ROW)
                    {
                        abAppend(ab, "~", 1);
                    }
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            }
            else
            {
                if (y < MAX_ROW)
                {
                    abAppend(ab, "~", 1);
                }
            }
        }
        else
        {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (len > E.screencols)
                len = E.screencols;

            // Konversi char ke char*
            char *c = &E.row[filerow].render[E.coloff];
            abAppend(ab, c, len);
        }
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", C.y + 1, E.numrows);
    if (len > E.screencols)
        len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols)
    {
        if (E.screencols - len == rlen)
        {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else
        {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols)
        msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen()
{
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (C.y - E.rowoff) + 1, (C.rx - E.coloff) + 1);

    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/* init */
void initEditor()
{
    C.x = 0;
    C.y = 0;
    C.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    // E.row = NULL;
    E.filename = NULL;
    E.dirty = 0;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    E.screenrows -= 2;
}
/** Find **/
void editorFind() {
	char *query = editorPrompt("Search : %s (Tekan ESC Untuk Batalkan)");
	if (query == NULL) return;
	
	int i;
	for (i = 0; i < E.numrows; i++) {
		erow *row = &E.row[i]; 
		char *match = strstr(row->render, query);
		if (match) {
			C.y = i;
			C.x = editorRowRxToCx(row,match - row->render);
			E.rowoff = E.numrows;
			break;
		}
		editorSetStatusMessage("String Tidak Ada!");
	}
	
	free(query);
}

