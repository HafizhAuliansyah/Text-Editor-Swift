

#include <stdio.h>
#include "rterm.h"
#include "keyb.h"



int read_keytrail(char chartrail[5]){

char ch;
int i;
   chartrail[0] = K_ESCAPE;
   for(i = 1; i < 5; i++) {
     if(kbhit() == 1) {
        ch=readch();
        chartrail[i] = ch;
     } else {
        chartrail[i] = 0;
     }
   }
   resetch();
   return 1;
}


int read_accent(char *ch, char accentchar[2])
{

  int result;
  result = 0;
    //Accents and special chars
   accentchar[0] = 0;
   accentchar[1] = *ch;
    if(*ch == SPECIAL_CHARS_SET1) {
      accentchar[0] = SPECIAL_CHARS_SET1;   //Accents and special chars SET1
      accentchar[1] = readch();
      result = 1;
      resetch();
    }
    if(*ch == SPECIAL_CHARS_SET2) {
      accentchar[0] = SPECIAL_CHARS_SET2;   //Accents and special chars SET2
      accentchar[1] = readch();
      result = 2;
      resetch();
    }
    return result;
}
