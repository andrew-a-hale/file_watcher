#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_CMD_LEN 256

typedef struct {
  int capacity;
  int length;
  char buf[MAX_CMD_LEN];
} Buffer;

typedef struct {
  int pid;
  uint64_t ts;
  Buffer *cmd;
} Watcher;

int append_buffer(Buffer *buf, char *s, char *sep) {
  int len = strlen(s);
  int seplen = strlen(sep);

  if (buf->length == 0) {
    if (len >= buf->capacity) {
      printf("ERROR: failed to append to cmd buffer\n");
      return 1;
    }
    strcpy(buf->buf, s);
    buf->length += len;
  } else {
    if (buf->length + len + seplen + 1 > buf->capacity) {
      printf("ERROR: failed to append to cmd buffer\n");
      return 1;
    }
    strcat(buf->buf, sep);
    strcat(buf->buf, s);
    buf->length += seplen + len;
  }

  return 0;
}

uint64_t to_ts(struct timespec ts) {
  return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint64_t get_ts() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return to_ts(ts);
}

void update_watcher_and_run_cmd(Watcher *w) {
  w->ts = get_ts();
  system(w->cmd->buf);
};

void print_watcher(Watcher *w) {
  printf("pid: %i | ts: %lu | cmd: %s\n", w->pid, w->ts, w->cmd->buf);
}

int traverse_fs(Buffer *path, Watcher *watcher) {
  struct stat st;
  if (lstat(path->buf, &st) != 0)
    return 0;

  if (!S_ISDIR(st.st_mode)) {
    if (to_ts(st.st_mtim) > watcher->ts) {
      print_watcher(watcher);
      return 1;
    }
  }

  DIR *d = opendir(path->buf);
  if (!d)
    return 0;

  struct dirent *e;
  while ((e = readdir(d))) {
    if (0 == strcmp(e->d_name, ".") || 0 == strcmp(e->d_name, ".."))
      continue;

    int mark = path->length;
    append_buffer(path, e->d_name, "/");
    if (traverse_fs(path, watcher)) {
      closedir(d);
      return 1;
    }
    path->buf[mark] = '\0';
    path->length = mark;
  }

  closedir(d);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("ERROR: expected 2 args, path and cmd, got %d\n", argc - 1);
    return 1;
  }

  Buffer root = (Buffer){.capacity = MAX_CMD_LEN, .length = 0};
  append_buffer(&root, *(++argv), "");
  Buffer path = root;

  Buffer cmd = (Buffer){.capacity = MAX_CMD_LEN, .length = 0};
  char *arg;
  while ((arg = *(++argv)))
    if (0 != append_buffer(&cmd, arg, " "))
      break;

  Watcher watcher = (Watcher){.pid = getpid(), .ts = get_ts(), .cmd = &cmd};

  while (1) {
    path = root;
    if (traverse_fs(&path, &watcher))
      update_watcher_and_run_cmd(&watcher);
    sleep(1);
  }

  return 0;
}
