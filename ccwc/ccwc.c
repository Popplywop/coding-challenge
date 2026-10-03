#define _POSIX_C_SOURCE 200809L

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

struct ccwcConfig {
  bool read_bytes;
  bool read_lines;
  bool read_words;
  bool read_characters;
};

struct ccwcFiles {
  FILE **files;
  char **filenames;
  int file_count;
};

struct ccwcOutput {
  unsigned long long total_bytes;
  unsigned long long total_lines;
  unsigned long long total_words;
  unsigned long long total_chars;
};

#define CCWC_INIT {false, false, false, false}
#define CCWC_FILE_INIT {NULL, NULL, 0}
#define CCWC_OUTPUT_INIT {0, 0, 0, 0}

int main(int argc, char *argv[]) {
  if (!setlocale(LC_ALL, "")) {
    fwprintf(stderr, L"Error: Could not set the default locale.\n");
    return EXIT_FAILURE;
  }

  struct ccwcConfig config = CCWC_INIT;
  struct ccwcFiles files = CCWC_FILE_INIT;

  size_t byte_count;
  int c;

  while ((c = getopt(argc, argv, "clwm")) != -1) {
    switch (c) {
    case 'c':
      config.read_bytes = true;
      break;
    case 'l':
      config.read_lines = true;
      break;
    case 'w':
      config.read_words = true;
      break;
    case 'm':
      config.read_characters = true;
      break;
    }
  }

  if (!config.read_bytes && !config.read_lines && !config.read_words) {
    config.read_bytes = true;
    config.read_lines = true;
    config.read_words = true;
  }

  int slots = argc - optind > 0 ? argc - optind : 1;

  files.files = calloc(slots, sizeof(FILE *));
  if (files.files == NULL) {
    return EXIT_FAILURE;
  }

  files.filenames = calloc(slots, sizeof(char *));
  if (files.filenames == NULL) {
    return EXIT_FAILURE;
  }

  if (argc == optind) {
    files.files[0] = stdin;
    files.file_count = 1;
  } else {
    for (; optind < argc; optind++) {
      FILE *fp = fopen(argv[optind], "rb");
      if (!fp) {
        perror(argv[optind]);
        continue;
      }
      files.files[files.file_count] = fp;
      files.filenames[files.file_count] = argv[optind];
      files.file_count++;
    }
  }

  struct ccwcOutput total = CCWC_OUTPUT_INIT;

  for (int i = 0; i < files.file_count; i++) {
    struct ccwcOutput output = CCWC_OUTPUT_INIT;
    char buf[4096];
    int in_word = 0;
    mbstate_t state = {0};

    while ((byte_count = fread(buf, 1, sizeof(buf), files.files[i])) > 0) {
      output.total_bytes += byte_count;
      size_t idx = 0;
      while (idx < byte_count) {
        wchar_t wc;
        size_t result = mbrtowc(&wc, &buf[idx], byte_count - idx, &state);

        if (result == (size_t)-1) {
          idx++;
          output.total_chars++;
          continue;
        } else if (result == (size_t)-2) {
          break;
        }

        size_t bytes_consumed = (result == 0) ? 1 : result;

        output.total_chars += 1;

        if (wc == L'\n') {
          output.total_lines++;
        }

        if (iswspace(wc)) {
          in_word = 0;
        } else if (!in_word) {
          in_word = 1;
          output.total_words++;
        }
        idx += bytes_consumed;
      }
    }

    if (config.read_lines) {
      printf("%llu ", output.total_lines);
    }

    if (config.read_words) {
      printf("%llu ", output.total_words);
    }

    if (config.read_bytes) {
      printf("%llu ", output.total_bytes);
    }

    if (config.read_characters) {
      printf("%llu ", output.total_chars);
    }

    if (files.filenames[i] != NULL) {
      printf("%s", files.filenames[i]);
    }
    printf("\n");

    total.total_bytes += output.total_bytes;
    total.total_chars += output.total_chars;
    total.total_words += output.total_words;
    total.total_lines += output.total_lines;
  }

  for (int i = 0; i < files.file_count; i++) {
    if (files.files[i] != NULL) {
      if (files.files[i] != stdin) {
        fclose(files.files[i]);
      }
      files.files[i] = NULL;
    }
    if (files.filenames[i] != NULL) {
      files.filenames[i] = NULL;
    }
  }

  if (files.file_count > 1) {
    if (config.read_lines) {
      printf("%llu ", total.total_lines);
    }

    if (config.read_words) {
      printf("%llu ", total.total_words);
    }

    if (config.read_bytes) {
      printf("%llu ", total.total_bytes);
    }

    if (config.read_characters) {
      printf("%llu ", total.total_chars);
    }

    printf("total\n");
  }

  free(files.files);
  free(files.filenames);
  files.files = NULL;
  files.filenames = NULL;
  files.file_count = 0;

  return 0;
}
