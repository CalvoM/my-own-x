#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define BAZEENGA_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT                                                              \
  { NULL, 0 }

enum editor_key {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/
typedef struct erow {
  int size;
  char *chars;
} editor_row_t;

struct editor_config {
  int cursor_x, cursor_y;
  int row_offset;
  int col_offset;
  int screen_rows;
  int screen_cols;
  int num_rows;
  editor_row_t *row;
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
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
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

      } else {
        switch (seq[1]) {
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
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
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

/*** row operations ***/
void editor_append_row(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(editor_row_t) * (E.num_rows + 1));
  int at = E.num_rows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.num_rows++;
}
/*** file i/o ***/
void editor_open(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editor_append_row(line, linelen);
  }
  free(line);
  fclose(fp);
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
  editor_row_t *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
  switch (key) {
  case ARROW_LEFT:
    if (E.cursor_x != 0) {
      E.cursor_x--;
    } else if (E.cursor_y > 0) {
      E.cursor_y--;
      E.cursor_x = E.row[E.cursor_y].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cursor_x < row->size) {
      E.cursor_x++;
    } else if (row && E.cursor_x == row->size) {
      E.cursor_y++;
      E.cursor_x = 0;
    }
    break;
  case ARROW_UP:
    if (E.cursor_y != 0)
      E.cursor_y--;
    break;
  case ARROW_DOWN:
    if (E.cursor_y < E.num_rows)
      E.cursor_y++;
    break;
  }
  row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
  int row_len = row ? row->size : 0;
  if (E.cursor_x > row_len)
    E.cursor_x = row_len;
}
void editor_process_key_press() {
  int c = editor_read_key();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case HOME_KEY:
    E.cursor_x = 0;
    break;
  case END_KEY:
    E.cursor_x = E.screen_cols - 1;
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    int times = E.screen_rows;
    while (times--)
      editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editor_move_cursor(c);
    break;
  }
}

/*** output ***/
void editor_scroll() {
  if (E.cursor_y < E.row_offset) {
    E.row_offset = E.cursor_y;
  }
  if (E.cursor_y >= (E.row_offset + E.screen_rows)) {
    E.row_offset = E.cursor_y - E.screen_rows + 1;
  }
  if (E.cursor_x < E.col_offset)
    E.col_offset = E.cursor_x;
  if (E.cursor_x >= (E.col_offset + E.screen_cols))
    E.col_offset = E.cursor_x - E.screen_cols + 1;
}

void editor_draw_rows(struct editor_abuf *e_ab) {
  int y;
  for (y = 0; y < E.screen_rows; y++) {
    int file_row = y + E.row_offset;
    if (file_row >= E.num_rows) {
      if (E.num_rows == 0 && y == E.screen_rows / 3) {
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
    } else {
      int len = E.row[file_row].size - E.col_offset;
      if (len < 0)
        len = 0;
      if (len > E.screen_cols)
        len = E.screen_cols;
      ab_append(e_ab, &E.row[file_row].chars[E.col_offset], len);
    }
    ab_append(e_ab, "\x1b[K", 3);
    if (y < E.screen_rows - 1) {
      ab_append(e_ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen() {
  editor_scroll();
  struct editor_abuf e_ab = ABUF_INIT;
  ab_append(&e_ab, "\x1b[?25l", 6);
  ab_append(&e_ab, "\x1b[H", 3);
  editor_draw_rows(&e_ab);
  ab_append(&e_ab, "\x1b[H", 3);
  char buf[32];
  int buf_len =
      snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursor_y - E.row_offset) + 1,
               (E.cursor_x - E.col_offset) + 1);
  ab_append(&e_ab, buf, buf_len);
  ab_append(&e_ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, e_ab.b, e_ab.len);
  ab_free(&e_ab);
}

/*** init **/
void init_editor() {
  E.cursor_x = 0;
  E.cursor_y = 0;
  E.num_rows = 0;
  E.row = NULL;
  E.row_offset = 0;
  E.col_offset = 0;
  if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
    die("get_window_size");
}
int main(int argc, char **argv) {
  enable_raw_mode();
  init_editor();
  if (argc >= 2) {
    editor_open(argv[1]);
  }
  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
  return 0;
}
