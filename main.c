#include <stdio.h>
#include <stdlib.h>
#include "editor.h"

/* main function */
int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("SHORTCUT: Ctrl - Q = quit; Ctrl - S  = save");

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
