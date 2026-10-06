#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_PARSER_IMPLEMENTATION
#include "json_parser.h"

int tests_run = 0;
int tests_failed = 0;

#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
#define COLOR_RESET "\x1b[0m"

#define ASSERT_STATUS(json_input, expected_status)                             \
  do {                                                                         \
    const char *input_ = (json_input);                                         \
    const char *shown_ = input_ ? input_ : "(null)";                           \
    tests_run++;                                                               \
    JsonStatus actual = json_validate(input_);                                 \
    if (actual != expected_status) {                                           \
      printf(COLOR_RED                                                         \
             "FAIL" COLOR_RESET                                                \
             ": line %d | Expected status %d, got %d for input: %s\n",         \
             __LINE__, expected_status, actual, shown_);                       \
      tests_failed++;                                                          \
    } else {                                                                   \
      printf(COLOR_GREEN "PASS" COLOR_RESET ": \"%s\"\n", shown_);             \
    }                                                                          \
  } while (0)

/* Generated inputs are too large to print, so they are checked by label and
 * freed here. Root object counts as depth 1. */

/* {"a":[[...[1]...]]} with `depth` containers in total. */
static char *build_nested_arrays(size_t depth) {
  size_t arrays = depth - 1;
  char *s = malloc(7 + 2 * arrays + 1);
  if (!s)
    abort();
  memcpy(s, "{\"a\":", 5);
  memset(s + 5, '[', arrays);
  s[5 + arrays] = '1';
  memset(s + 6 + arrays, ']', arrays);
  s[6 + 2 * arrays] = '}';
  s[7 + 2 * arrays] = '\0';
  return s;
}

/* {"a":{"a":...1...}} with `depth` containers in total. */
static char *build_nested_objects(size_t depth) {
  char *s = malloc(6 * depth + 2);
  if (!s)
    abort();
  for (size_t i = 0; i < depth; i++)
    memcpy(s + 5 * i, "{\"a\":", 5);
  s[5 * depth] = '1';
  memset(s + 5 * depth + 1, '}', depth);
  s[6 * depth + 1] = '\0';
  return s;
}

/* Alternating {"a": and [ with `depth` containers in total, starting with the
 * root object: {"a":[{"a":[...1...]}]} */
static char *build_nested_mixed(size_t depth) {
  size_t objects = (depth + 1) / 2;
  size_t arrays = depth / 2;
  char *s = malloc(5 * objects + arrays + 1 + depth + 1);
  if (!s)
    abort();
  size_t pos = 0;
  for (size_t level = 1; level <= depth; level++) {
    if (level % 2 == 1) {
      memcpy(s + pos, "{\"a\":", 5);
      pos += 5;
    } else {
      s[pos++] = '[';
    }
  }
  s[pos++] = '1';
  for (size_t level = depth; level >= 1; level--)
    s[pos++] = (level % 2 == 1) ? '}' : ']';
  s[pos] = '\0';
  return s;
}

/* {"a":[[1],[1],...,[1]]} with `count` sibling arrays, each at depth 2. */
static char *build_wide_arrays(size_t count) {
  char *s = malloc(6 + 4 * count + 3);
  if (!s)
    abort();
  size_t pos = 0;
  memcpy(s + pos, "{\"a\":[", 6);
  pos += 6;
  for (size_t i = 0; i < count; i++) {
    memcpy(s + pos, "[1],", 4);
    pos += 4;
  }
  pos--; /* drop the trailing comma */
  memcpy(s + pos, "]}", 3);
  return s;
}

static void assert_generated(const char *label, char *input,
                             JsonStatus expected) {
  tests_run++;
  JsonStatus actual = json_validate(input);
  if (actual != expected) {
    printf(COLOR_RED "FAIL" COLOR_RESET
                     ": %s | Expected status %d, got %d\n",
           label, expected, actual);
    tests_failed++;
  } else {
    printf(COLOR_GREEN "PASS" COLOR_RESET ": %s\n", label);
  }
  free(input);
}

/* JSON_checker corpus (https://www.json.org/JSON_checker/). Paths are relative
 * to the project root, which is where `make test` runs the binary. */
#define JSON_CHECKER_DIR "tests/json_checker/"

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);
  char *buf = malloc((size_t)size + 1);
  if (!buf)
    abort();
  size_t n = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[n] = '\0';
  return buf;
}

static void assert_file(const char *name, bool expect_valid) {
  char path[256];
  snprintf(path, sizeof path, JSON_CHECKER_DIR "%s", name);

  tests_run++;
  char *input = read_file(path);
  if (!input) {
    printf(COLOR_RED "FAIL" COLOR_RESET
                     ": cannot read %s (run from the project root)\n",
           path);
    tests_failed++;
    return;
  }

  JsonStatus actual = json_validate(input);
  bool ok = expect_valid ? actual == JSON_SUCCESS : actual != JSON_SUCCESS;
  if (!ok) {
    printf(COLOR_RED "FAIL" COLOR_RESET ": %s | expected %s, got status %d\n",
           path, expect_valid ? "valid" : "invalid", actual);
    tests_failed++;
  } else {
    printf(COLOR_GREEN "PASS" COLOR_RESET ": %s\n", path);
  }
  free(input);
}

int main(void) {
  printf("==================================================\n");
  printf(" Running JSON Parser Unit Tests (Step 1)\n");
  printf("==================================================\n\n");

  // JSON_SUCCESS
  ASSERT_STATUS("{}", JSON_SUCCESS);
  ASSERT_STATUS("   {}   ", JSON_SUCCESS);
  ASSERT_STATUS("\n{\n}\n", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"b\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"b\",\"c\":\"d\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\\\"b\":\"c\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":0}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":-1}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":12}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1.5}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":-0.5}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1e5}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1E+5}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1.5e-3}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1,\"b\":\"c\",\"d\":2.5}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":true}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":false}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":null}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":true,\"b\":false,\"c\":null}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":1,\"b\":\"c\",\"d\":true,\"e\":null}", JSON_SUCCESS);
  ASSERT_STATUS("{ \"a\" : true , \"b\" : null }", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[ ]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1,2,3]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[\"x\",\"y\"]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[true,false,null]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1,\"b\",true,null,2.5]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[-1,0,1e5,1.5e-3]}", JSON_SUCCESS);
  ASSERT_STATUS("{ \"a\" : [ 1 , 2 ] }", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[\"]\",\"[\",\"a,b\"]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[[]]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[[1,2],[3]]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[[[1]]]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[{}]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[{\"b\":1},{\"c\":2}]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1,{\"b\":[2,3]},[4]]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1],\"b\":[2]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[],\"b\":[]}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":[1],\"b\":\"c\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":{}}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":{\"b\":{\"c\":1}}}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":{\"b\":[1]}}", JSON_SUCCESS);
  ASSERT_STATUS("[]", JSON_SUCCESS);
  ASSERT_STATUS("[1,2]", JSON_SUCCESS);
  ASSERT_STATUS("[{\"a\":[true]},null]", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\n\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\\"\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\\\\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\/\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\b\\f\\n\\r\\t\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\u00e9\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\uABCD\\uabcd\\u0123\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\\n\":1}", JSON_SUCCESS);
  ASSERT_STATUS("[\"\\u0041\"]", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"x\\ny\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\\\n\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\\\\\\"\"}", JSON_SUCCESS);
  ASSERT_STATUS("{\"a\":\"\\\\u12\"}", JSON_SUCCESS);
  assert_generated("arrays nested to JSON_MAX_DEPTH",
                   build_nested_arrays(JSON_MAX_DEPTH), JSON_SUCCESS);
  assert_generated("objects nested to JSON_MAX_DEPTH",
                   build_nested_objects(JSON_MAX_DEPTH), JSON_SUCCESS);
  assert_generated("mixed objects and arrays nested to JSON_MAX_DEPTH",
                   build_nested_mixed(JSON_MAX_DEPTH), JSON_SUCCESS);
  assert_generated("1000 sibling arrays (depth restored after each)",
                   build_wide_arrays(1000), JSON_SUCCESS);

  // JSON_ERROR_EMPTY_INPUT
  ASSERT_STATUS("", JSON_ERROR_EMPTY_INPUT);
  ASSERT_STATUS("   ", JSON_ERROR_EMPTY_INPUT);
  ASSERT_STATUS(NULL, JSON_ERROR_EMPTY_INPUT);

  // JSON_ERROR_INVALID_ROOT
  ASSERT_STATUS("invalid_root", JSON_ERROR_INVALID_ROOT);
  ASSERT_STATUS("true", JSON_ERROR_INVALID_ROOT);
  ASSERT_STATUS("null", JSON_ERROR_INVALID_ROOT);
  ASSERT_STATUS("123", JSON_ERROR_INVALID_ROOT);
  ASSERT_STATUS("\"abc\"", JSON_ERROR_INVALID_ROOT);

  // JSON_ERROR_UNMATCHED_BRACE
  ASSERT_STATUS("{", JSON_ERROR_UNMATCHED_BRACE);
  ASSERT_STATUS("{\"a\":{}", JSON_ERROR_UNMATCHED_BRACE);
  ASSERT_STATUS("{\"a\":{\"b\":1", JSON_ERROR_UNMATCHED_BRACE);
  ASSERT_STATUS("{\"a\":[{", JSON_ERROR_UNMATCHED_BRACE);
  ASSERT_STATUS("{\"a\":[1]", JSON_ERROR_UNMATCHED_BRACE);

  // JSON_ERROR_UNMATCHED_BRACKET
  ASSERT_STATUS("{\"a\":[", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[1", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[1,", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[1 ", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[[", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[[1]", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[1,[2", JSON_ERROR_UNMATCHED_BRACKET);
  ASSERT_STATUS("{\"a\":[{}", JSON_ERROR_UNMATCHED_BRACKET);

  // JSON_ERROR_SYNTAX
  ASSERT_STATUS("{} trailing_garbage", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"b\",}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\" \"b\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"b}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{1:\"a\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":01}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":+1}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":.5}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":1.}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":-}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":1e}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":1e+}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":1.2.3}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":1,}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":tru}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":True}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":TRUE}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":truex}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":fals}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":t}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":truefalse}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":nul}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":Null}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":nulll}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":null,}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{true:1}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{null:1}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":True}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":False}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1,]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[,1]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[,]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1,,2]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1 2]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[\"x\" \"y\"]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1:2]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[01]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[tru]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[\"x]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[[1,]]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[{\"b\"}]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[{\"b\":1,}]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":{\"b\":1]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":{,}}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1]]}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1],}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1] \"b\":2}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{[1]:2}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":[1]}]", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\x\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\a\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\0\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\ \"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\N\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\U0041\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\u\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\u12\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\u123\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\u12G4\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\uZZZZ\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\\x\":1}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"abc\\\"}", JSON_ERROR_SYNTAX);
  ASSERT_STATUS("{\"a\":\"\\", JSON_ERROR_SYNTAX);

  // JSON_ERROR_DEPTH
  assert_generated("arrays nested to JSON_MAX_DEPTH + 1",
                   build_nested_arrays(JSON_MAX_DEPTH + 1), JSON_ERROR_DEPTH);
  assert_generated("objects nested to JSON_MAX_DEPTH + 1",
                   build_nested_objects(JSON_MAX_DEPTH + 1), JSON_ERROR_DEPTH);
  assert_generated("mixed objects and arrays nested to JSON_MAX_DEPTH + 1",
                   build_nested_mixed(JSON_MAX_DEPTH + 1), JSON_ERROR_DEPTH);
  assert_generated("arrays nested 100000 deep (no stack overflow)",
                   build_nested_arrays(100000), JSON_ERROR_DEPTH);
  assert_generated("objects nested 100000 deep (no stack overflow)",
                   build_nested_objects(100000), JSON_ERROR_DEPTH);

  // JSON_checker corpus
  for (int i = 1; i <= 3; i++) {
    char name[32];
    snprintf(name, sizeof name, "pass%d.json", i);
    assert_file(name, true);
  }
  for (int i = 1; i <= 33; i++) {
    /* fail18 is "too deep" against JSON_checker's own limit of 20 levels.
     * Our limit is JSON_MAX_DEPTH, covered by the depth tests above. */
    if (i == 18)
      continue;
    char name[32];
    snprintf(name, sizeof name, "fail%d.json", i);
    assert_file(name, false);
  }

  printf("\n==================================================\n");
  printf(" Test Execution Summary\n");
  printf("==================================================\n");
  printf(" Passed: %d / %d\n", (tests_run - tests_failed), tests_run);
  printf(" Failed: %d\n", tests_failed);
  printf("==================================================\n");

  // Return a non-zero exit status if any assertions failed
  return (tests_failed > 0) ? 1 : 0;
}
