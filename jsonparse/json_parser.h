#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

/* ============================================================================
 * 1. PUBLIC API DECLARATIONS
 * ============================================================================
 * This section is visible to any file that includes this header.
 */

typedef enum {
  JSON_SUCCESS = 0,
  JSON_ERROR_EMPTY_INPUT,
  JSON_ERROR_INVALID_ROOT,
  JSON_ERROR_UNMATCHED_BRACE,
  JSON_ERROR_UNMATCHED_BRACKET,
  JSON_ERROR_SYNTAX,
  JSON_ERROR_DEPTH
} JsonStatus;

JsonStatus json_validate(const char *json_str);

#endif /* JSON_PARSER_H */

/* ============================================================================
 * 2. CORE IMPLEMENTATION
 * ============================================================================
 * This section is only compiled ONCE in the file that defines the macro.
 */

#ifdef JSON_PARSER_IMPLEMENTATION

#include <stdbool.h>
#include <string.h>

typedef enum {
  TOKEN_EOF = 0,
  TOKEN_BOOL,
  TOKEN_COLON,
  TOKEN_COMMA,
  TOKEN_ERROR,
  TOKEN_LBRACE,
  TOKEN_LBRACKET,
  TOKEN_NULL,
  TOKEN_NUMERIC,
  TOKEN_RBRACE,
  TOKEN_RBRACKET,
  TOKEN_STRING,
} TokenKind;

typedef struct {
  TokenKind kind;
} Token;

typedef struct {
  const char *source;
  size_t cursor;
} Lexer;

typedef struct {
  Lexer lexer;
  Token current;
  int depth;
} Parser;

#define JSON_MAX_DEPTH 128

static bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_hex_digit(char c) {
  return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_number_start(char c) { return c == '-' || is_digit(c); }

static char lexer_peek(const Lexer *lexer) {
  return lexer->source[lexer->cursor];
}

static size_t lexer_consume_digits(Lexer *lexer) {
  size_t consumed = 0;

  while (is_digit(lexer_peek(lexer))) {
    lexer->cursor++;
    consumed++;
  }

  return consumed;
}

static bool lexer_read_escape(Lexer *lexer) {
  lexer->cursor++;

  switch (lexer_peek(lexer)) {
  case '"':
  case '\\':
  case '/':
  case 'b':
  case 'f':
  case 'n':
  case 'r':
  case 't':
    lexer->cursor++;
    return true;
  case 'u':
    lexer->cursor++;
    /* \uXXXX: exactly four hex digits */
    for (int i = 0; i < 4; i++) {
      if (!is_hex_digit(lexer_peek(lexer))) {
        return false;
      }
      lexer->cursor++;
    }
    return true;
  default:
    return false;
  }
}

static Token lexer_read_literal(Lexer *lexer, const char *literal,
                                TokenKind kind) {
  Token token;
  size_t len = strlen(literal);

  if (strncmp(lexer->source + lexer->cursor, literal, len) == 0) {
    lexer->cursor += len;
    token.kind = kind;
    return token;
  }

  token.kind = TOKEN_ERROR;
  return token;
}

static Token lexer_read_number(Lexer *lexer) {
  Token token;

  if (lexer_peek(lexer) == '-') {
    lexer->cursor++;
  }

  char c = lexer_peek(lexer);

  if (c == '0') {
    lexer->cursor++;
  } else if (is_digit(c)) {
    lexer->cursor++;
    lexer_consume_digits(lexer);
  } else {
    token.kind = TOKEN_ERROR;
    return token;
  }

  if (lexer_peek(lexer) == '.') {
    lexer->cursor++;

    if (lexer_consume_digits(lexer) == 0) {
      token.kind = TOKEN_ERROR;
      return token;
    }
  }

  if (lexer_peek(lexer) == 'e' || lexer_peek(lexer) == 'E') {
    lexer->cursor++;

    if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-') {
      lexer->cursor++;
    }

    if (lexer_consume_digits(lexer) == 0) {
      token.kind = TOKEN_ERROR;
      return token;
    }
  }

  token.kind = TOKEN_NUMERIC;
  return token;
}

static Token lexer_read_string(Lexer *lexer) {
  Token token;
  lexer->cursor++;

  while (lexer_peek(lexer) != '\0') {
    char c = lexer_peek(lexer);

    if (c == '"') {
      lexer->cursor++;
      token.kind = TOKEN_STRING;
      return token;
    }

    if (c == '\\') {
      if (!lexer_read_escape(lexer)) {
        break;
      }
      continue;
    }

    if ((unsigned char)c < 0x20) {
      break;
    }

    lexer->cursor++;
  }

  token.kind = TOKEN_ERROR;
  return token;
}

static Token lexer_next_token(Lexer *lexer) {
  Token token;

  while (lexer_peek(lexer) != '\0' && is_whitespace(lexer_peek(lexer))) {
    lexer->cursor++;
  }

  char current = lexer_peek(lexer);

  if (current == '\0') {
    token.kind = TOKEN_EOF;
    return token;
  }

  switch (current) {
  case '{':
    token.kind = TOKEN_LBRACE;
    lexer->cursor++;
    return token;
  case '}':
    token.kind = TOKEN_RBRACE;
    lexer->cursor++;
    return token;
  case '[':
    token.kind = TOKEN_LBRACKET;
    lexer->cursor++;
    return token;
  case ']':
    token.kind = TOKEN_RBRACKET;
    lexer->cursor++;
    return token;
  case ':':
    token.kind = TOKEN_COLON;
    lexer->cursor++;
    return token;
  case ',':
    token.kind = TOKEN_COMMA;
    lexer->cursor++;
    return token;
  case '"':
    return lexer_read_string(lexer);
  case 't':
    return lexer_read_literal(lexer, "true", TOKEN_BOOL);
  case 'f':
    return lexer_read_literal(lexer, "false", TOKEN_BOOL);
  case 'n':
    return lexer_read_literal(lexer, "null", TOKEN_NULL);
  default:
    if (is_number_start(current)) {
      return lexer_read_number(lexer);
    }

    token.kind = TOKEN_ERROR;
    lexer->cursor++;
    return token;
  }
}

static void advance(Parser *p) { p->current = lexer_next_token(&p->lexer); }

static JsonStatus parse_object(Parser *p);
static JsonStatus parse_value(Parser *p);
static JsonStatus parse_pair(Parser *p);

static JsonStatus parse_array_body(Parser *p) {
  advance(p);

  if (p->current.kind == TOKEN_RBRACKET) {
    advance(p);
    return JSON_SUCCESS;
  }

  if (p->current.kind == TOKEN_EOF) {
    return JSON_ERROR_UNMATCHED_BRACKET;
  }

  for (;;) {
    JsonStatus status = parse_value(p);
    if (status != JSON_SUCCESS) {
      return status;
    }

    if (p->current.kind == TOKEN_COMMA) {
      advance(p);
      if (p->current.kind == TOKEN_EOF) {
        return JSON_ERROR_UNMATCHED_BRACKET;
      }
      continue;
    }

    if (p->current.kind == TOKEN_RBRACKET) {
      advance(p);
      return JSON_SUCCESS;
    }

    if (p->current.kind == TOKEN_EOF) {
      return JSON_ERROR_UNMATCHED_BRACKET;
    }

    return JSON_ERROR_SYNTAX;
  }
}

static JsonStatus parse_object_body(Parser *p) {
  advance(p);

  if (p->current.kind == TOKEN_RBRACE) {
    advance(p);
    return JSON_SUCCESS;
  }

  if (p->current.kind == TOKEN_EOF) {
    return JSON_ERROR_UNMATCHED_BRACE;
  }

  for (;;) {
    JsonStatus status = parse_pair(p);
    if (status != JSON_SUCCESS)
      return status;

    if (p->current.kind == TOKEN_COMMA) {
      advance(p);
      continue;
    }
    if (p->current.kind == TOKEN_RBRACE) {
      advance(p);
      return JSON_SUCCESS;
    }
    if (p->current.kind == TOKEN_EOF) {
      return JSON_ERROR_UNMATCHED_BRACE;
    }

    return JSON_ERROR_SYNTAX;
  }
}

static JsonStatus parse_array(Parser *p) {
  p->depth++;
  if (p->depth > JSON_MAX_DEPTH) {
    return JSON_ERROR_DEPTH;
  }

  JsonStatus status = parse_array_body(p);
  p->depth--;
  return status;
}

static JsonStatus parse_value(Parser *p) {
  switch (p->current.kind) {
  case TOKEN_STRING:
  case TOKEN_NUMERIC:
  case TOKEN_BOOL:
  case TOKEN_NULL:
    advance(p);
    return JSON_SUCCESS;
  case TOKEN_LBRACE:
    return parse_object(p);
  case TOKEN_LBRACKET:
    return parse_array(p);
  default:
    return JSON_ERROR_SYNTAX;
  }
}

static JsonStatus parse_pair(Parser *p) {
  if (p->current.kind != TOKEN_STRING)
    return JSON_ERROR_SYNTAX;
  advance(p);

  if (p->current.kind != TOKEN_COLON)
    return JSON_ERROR_SYNTAX;
  advance(p);

  return parse_value(p);
}

static JsonStatus parse_object(Parser *p) {
  p->depth++;
  if (p->depth > JSON_MAX_DEPTH) {
    return JSON_ERROR_DEPTH;
  }

  JsonStatus status = parse_object_body(p);
  p->depth--;
  return status;
}

JsonStatus json_validate(const char *json_str) {
  if (json_str == NULL)
    return JSON_ERROR_EMPTY_INPUT;

  Parser parser;
  parser.depth = 0;
  parser.lexer.source = json_str;
  parser.lexer.cursor = 0;

  parser.current = lexer_next_token(&parser.lexer);

  if (parser.current.kind == TOKEN_EOF) {
    return JSON_ERROR_EMPTY_INPUT;
  }

  if (parser.current.kind != TOKEN_LBRACE &&
      parser.current.kind != TOKEN_LBRACKET) {
    return JSON_ERROR_INVALID_ROOT;
  }

  JsonStatus status = parse_value(&parser);
  if (status != JSON_SUCCESS)
    return status;

  if (parser.current.kind != TOKEN_EOF) {
    return JSON_ERROR_SYNTAX;
  }

  return JSON_SUCCESS;
}

#endif /* JSON_PARSER_IMPLEMENTATION */
