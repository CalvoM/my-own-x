#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT                                                              \
  { NULL, 0 }

/*** data ***/
struct editor_config {
  int screen_rows;
  int screen_cols;
  struct termios orig_termios;
};

struct editor_config E;

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disable_raw_mode);
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

char editor_read_key() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

int get_window_size(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

struct editor_abuf {
  char *b;
  int len;
};

void ab_append(struct editor_abuf *e_ab, const char *s, int len) {
  char *new = realloc(e_ab->b, e_ab->len + len);
  if (new == NULL)
    return;
  memcpy(&new[e_ab->len], s, len);
  e_ab->b = new;
  e_ab->len += len;
}

void ab_free(struct editor_abuf *e_ab) { free(e_ab->b); }
/*** input ***/
void editor_process_key_press() {
  char c = editor_read_key();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

/*** output ***/
void editor_draw_rows(struct editor_abuf *e_ab) {
  int y;
  for (y = 0; y < E.screen_rows; y++) {
    ab_append(e_ab, "~", 1);
    if (y < E.screen_rows - 1) {
      ab_append(e_ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen() {
  struct editor_abuf e_ab = ABUF_INIT;
  ab_append(&e_ab, "\x1b[?25l", 6);
  ab_append(&e_ab, "\x1b[2J", 4);
  ab_append(&e_ab, "\x1b[H", 3);
  editor_draw_rows(&e_ab);
  ab_append(&e_ab, "\x1b[H", 3);
  ab_append(&e_ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, e_ab.b, e_ab.len);
}

/*** init **/
void init_editor() {
  if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
    die("get_window_size");
}
int main() {
  enable_raw_mode();
  init_editor();
  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
  return 0;
}
