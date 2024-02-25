#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define BAZEENGA_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT                                                              \
  { NULL, 0 }

enum editor_key { ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN };

/*** data ***/
struct editor_config {
  int cursor_x, cursor_y;
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

int editor_read_key() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        return ARROW_UP;
      case 'B':
        return ARROW_DOWN;
      case 'C':
        return ARROW_RIGHT;
      case 'D':
        return ARROW_LEFT;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
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
void editor_move_cursor(int key) {
  switch (key) {
  case ARROW_LEFT:
    E.cursor_x--;
    break;
  case ARROW_RIGHT:
    E.cursor_x++;
    break;
  case ARROW_UP:
    E.cursor_y--;
    break;
  case ARROW_DOWN:
    E.cursor_y++;
    break;
  }
}
void editor_process_key_press() {
  int c = editor_read_key();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;
  }
}

/*** output ***/
void editor_draw_rows(struct editor_abuf *e_ab) {
  int y;
  for (y = 0; y < E.screen_rows; y++) {
    if (y == E.screen_rows / 3) {
      char welcome[80];
      int welcome_len =
          snprintf(welcome, sizeof(welcome), "Bazeenga Editor -- version %s",
                   BAZEENGA_VERSION);
      if (welcome_len > E.screen_cols)
        welcome_len = E.screen_cols;
      int padding = (E.screen_cols - welcome_len) / 2;
      if (padding) {
        ab_append(e_ab, "~", 1);
        padding--;
      }
      while (padding--)
        ab_append(e_ab, " ", 1);
      ab_append(e_ab, welcome, welcome_len);
    } else {
      ab_append(e_ab, "~", 1);
    }
    ab_append(e_ab, "\x1b[K", 3);
    if (y < E.screen_rows - 1) {
      ab_append(e_ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen() {
  struct editor_abuf e_ab = ABUF_INIT;
  ab_append(&e_ab, "\x1b[?25l", 6);
  ab_append(&e_ab, "\x1b[H", 3);
  editor_draw_rows(&e_ab);
  ab_append(&e_ab, "\x1b[H", 3);
  char buf[32];
  int buf_len =
      snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y + 1, E.cursor_x + 1);
  ab_append(&e_ab, buf, buf_len);
  ab_append(&e_ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, e_ab.b, e_ab.len);
  ab_free(&e_ab);
}

/*** init **/
void init_editor() {
  E.cursor_x = 20;
  E.cursor_y = 20;
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
