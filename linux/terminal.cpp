//
// Created by m8 on 9/28/18.
//

#include <termios.h>
#include <stdio.h>
#include "terminal.h"


static struct termios oldTermos, newTermos;

/* Initialize new terminal i/o settings */
void initTermios(int echo) {
  tcgetattr(0, &oldTermos); /* grab old terminal i/o settings */
  newTermos = oldTermos; /* make new settings same as old settings */
  newTermos.c_lflag &= ~ICANON; /* disable buffered i/o */
  if (echo) {
    newTermos.c_lflag |= ECHO; /* set echo mode */
  } else {
    newTermos.c_lflag &= ~ECHO; /* set no echo mode */
  }
  tcsetattr(0, TCSANOW, &newTermos); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) {
  tcsetattr(0, TCSANOW, &oldTermos);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo) {
  char ch;
  initTermios(echo);
  ch = getchar();
  resetTermios();
  return ch;
}

/* Read 1 character without echo */
char getch(void) {
  return getch_(0);
}

/* Read 1 character with echo */
char getche(void) {
  return getch_(1);
}
