#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"

#define usize size_t;

///////////
// Types //
///////////

enum TokenType {
  CONTENT_TEXT,
  CONTENT_TEXT_DELIM_QUOTE,
  CONTENT_TEXT_DELIM_PIPE,

  CHOICE_BULLETS,
  GATHER_BULLETS,

  CHOICE_CONDITION_OPEN,

  INLINE_CONDITIONAL_OPEN,

  SWITCH_CASE_OPEN,

  INLINE_SEQUENCE_OPEN,
  INLINE_SEQUENCE_SEP,

  TOKEN_TYPE_COUNT
};

/////////////
// Scanner //
/////////////

typedef struct Scanner {
  bool in_gather_line;
  bool in_choice_line;
  bool in_choice_condition;
  size_t inline_sequence_depth;
  size_t inline_conditional_depth;
  size_t switch_case_depth;
  size_t chars_advanced_this_parse;
  size_t chars_matched_this_parse;
} Scanner;

void *tree_sitter_ink_external_scanner_create() {
  Scanner *s = ts_malloc(sizeof(Scanner));
  s->in_gather_line = 0;
  s->in_choice_line = 0;
  s->in_choice_condition = 0;
  s->inline_sequence_depth = 0;
  s->inline_conditional_depth = 0;
  s->switch_case_depth = 0;
  s->chars_advanced_this_parse = 0;
  s->chars_matched_this_parse = 0;

  return s;
}

void tree_sitter_ink_external_scanner_destroy(void *payload) { ts_free((Scanner *)(payload)); }

unsigned tree_sitter_ink_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  Scanner *buf = (Scanner *)buffer;

  memcpy(buf, scanner, sizeof(Scanner));
  return sizeof(Scanner);
}

void tree_sitter_ink_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  Scanner *buf = (Scanner *)buffer;

  memcpy(scanner, buf, length);
}

///////////
// Utils //
///////////

static int32_t cursor(TSLexer *lexer) { return lexer->lookahead; }
static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }
static void advance(TSLexer *lexer, Scanner *scanner) {
  lexer->advance(lexer, false);
  scanner->chars_advanced_this_parse = scanner->chars_advanced_this_parse + 1;
}
static void checkpoint(TSLexer *lexer, Scanner *scanner) {
  lexer->mark_end(lexer);
  size_t chars_advanced_this_parse = scanner->chars_advanced_this_parse;
  scanner->chars_matched_this_parse = chars_advanced_this_parse;
}

static bool consume_until(TSLexer *lexer, Scanner *scanner, char c) {
  while (cursor(lexer) != c) {
    if (lexer->eof(lexer)) {
      return false;
    }
    advance(lexer, scanner);
  }
  return true;
}

static bool match_str_now(TSLexer *lexer, Scanner *scanner, char *str) {
  checkpoint(lexer, scanner);
  int32_t i = 0;

  for (;;) {
    if (lexer->eof(lexer)) return false;
    if (str[i] == '\0') return true;
    if (cursor(lexer) != str[i]) return false;

    i++;
    advance(lexer, scanner);
  }
}

static bool is_unicode_char(unsigned long ch) {
  // Basic Latin letters and numbers
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
    return true;
  }

  // Unicode letter ranges (covers most common scripts)
  return (ch >= 0x0080 && ch <= 0x1FFF) || // Latin Extended to Greek Extended
         (ch >= 0x2C00 && ch <= 0x2FEF) || // Glagolitic to Kangxi Radicals
         (ch >= 0x3040 && ch <= 0x318F) || // Hiragana to Hangul
         (ch >= 0x3300 && ch <= 0x9FFF) || // CJK
         (ch >= 0xAC00 && ch <= 0xD7AF) || // Hangul Syllables
         (ch >= 0xF900 && ch <= 0xFAFF);   // CJK Compatibility
}

//////////////
// BitValid //
//////////////

typedef uint64_t BitValid;

static void bit_set(BitValid *bitfield, unsigned int index, bool value) {
  if (value) {
    *bitfield = *bitfield | (1ULL << index);
  } else {
    *bitfield = *bitfield & ~(1ULL << index);
  }
}

static bool bit_get(BitValid bitfield, unsigned int index) { return (bitfield & (1ULL << index)) != 0; }

static bool bit_any(BitValid bitfield) { return bitfield != 0; }

static void bitfield_toggle(BitValid *bitfield, unsigned int index) { *bitfield = *bitfield ^ (1ULL << index); }

static BitValid bit_init(const bool *valid_symbols) {
  BitValid x = 0ULL;
  for (size_t i = 0; i < TOKEN_TYPE_COUNT; i++) {
    bit_set(&x, i, valid_symbols[i]);
  }
  return x;
}

static BitValid bit_init_empty() { return 0ULL; }

static void bit_log(TSLexer *lexer, BitValid bf) { lexer->log(lexer, "Bitfield (hex): 0x%016lX", bf); }

static void print_bit_valid(TSLexer *lexer, BitValid bf) {
  if (bit_get(bf, CONTENT_TEXT)) lexer->log(lexer, "\tCONTENT_TEXT");
  if (bit_get(bf, CONTENT_TEXT_DELIM_QUOTE)) lexer->log(lexer, "\tCONTENT_TEXT_DELIM_QUOTE");
  if (bit_get(bf, CONTENT_TEXT_DELIM_PIPE)) lexer->log(lexer, "\tCONTENT_TEXT_DELIM_PIPE");
  if (bit_get(bf, CHOICE_BULLETS)) lexer->log(lexer, "\tCHOICE_BULLETS");
  if (bit_get(bf, GATHER_BULLETS)) lexer->log(lexer, "\tGATHER_BULLETS");
  if (bit_get(bf, CHOICE_CONDITION_OPEN)) lexer->log(lexer, "\tCHOICE_CONDITION_OPEN");
  if (bit_get(bf, INLINE_CONDITIONAL_OPEN)) lexer->log(lexer, "\tINLINE_CONDITIONAL_OPEN");
  if (bit_get(bf, SWITCH_CASE_OPEN)) lexer->log(lexer, "\tSWITCH_CASE_OPEN");
  if (bit_get(bf, INLINE_SEQUENCE_OPEN)) lexer->log(lexer, "\tINLINE_SEQUENCE_OPEN");
  if (bit_get(bf, INLINE_SEQUENCE_SEP)) lexer->log(lexer, "\tINLINE_SEQUENCE_SEP");
}

/////////////
// Parsers //
/////////////

static bool parse_as_identifier(Scanner *scanner, TSLexer *lexer) {
  lexer->log(lexer, "parse_as_identifier");

  size_t buf_len = 0;
  Array(char) buf = array_new();

  for (;;) {
    if (lexer->eof(lexer)) return true;
    if (!is_unicode_char(cursor(lexer))) {
      lexer->log(lexer, "identifier complete at '%c'", cursor(lexer));

      if (buf_len == 0) {
        lexer->log(lexer, "identifier zero len");
        return false;
      }

      // some control words can LOOK like identifiers, but aren't actually
      if (strncmp(buf.contents, "not", buf_len) == 0) return false;
      if (strncmp(buf.contents, "and", buf_len) == 0) return false;
      if (strncmp(buf.contents, "or", buf_len) == 0) return false;

      return true;
    }

    array_push(&buf, cursor(lexer));
    buf_len++;
    advance(lexer, scanner);
  }
}

static bool parse_as_choice_bullets(Scanner *scanner, TSLexer *lexer) {
  lexer->log(lexer, "parse_as_choice_bullets");

  char bullet_char = cursor(lexer);

  while (cursor(lexer) == bullet_char || cursor(lexer) == ' ' || cursor(lexer) == '\t') {
    checkpoint(lexer, scanner);
    advance(lexer, scanner);
    if (lexer->eof(lexer)) return false;
  }

  checkpoint(lexer, scanner);

  return true;
}

static bool parse_as_gather_bullets(Scanner *scanner, TSLexer *lexer) {
  lexer->log(lexer, "parse_as_choice_bullets");

  char bullet_char = '-';

  // this is a multi-line conditional branch
  if (scanner->switch_case_depth) {
    return false;
  }

  while (cursor(lexer) == bullet_char || cursor(lexer) == ' ' || cursor(lexer) == '\t') {
    checkpoint(lexer, scanner);
    advance(lexer, scanner);
    if (lexer->eof(lexer)) return false;
  }

  lexer->log(lexer, "cursor ends at '%lc'", cursor(lexer));

  if (cursor(lexer) == '>') {
    // this was a divert symbol
    return false;
  }

  checkpoint(lexer, scanner);

  return true;
}

static bool parse_as_content_text(Scanner *scanner, TSLexer *lexer, char delim) {
  lexer->log(lexer, "parse_as_content_text");
  lexer->log(lexer, "start at '%lc'", cursor(lexer));
  if (match_str_now(lexer, scanner, "=")) return false; // stitch or knot
  if (match_str_now(lexer, scanner, "~")) return false; // statement
  if (match_str_now(lexer, scanner, "(")) return false; // label
  if (match_str_now(lexer, scanner, "INCLUDE")) return false;
  if (match_str_now(lexer, scanner, "VAR")) return false;
  if (match_str_now(lexer, scanner, "CONST")) return false;
  if (match_str_now(lexer, scanner, "LIST")) return false;
  if (match_str_now(lexer, scanner, "EXTERNAL")) return false;

  for (;;) {

    if (lexer->eof(lexer)) {
      return false;
    }

    if (cursor(lexer) == '\n') {
      checkpoint(lexer, scanner);
      advance(lexer, scanner);
      return true;
    }

    if (cursor(lexer) == delim) {
      checkpoint(lexer, scanner);
      return true;
    }

    // ending at line comment
    if (cursor(lexer) == '/') {
      checkpoint(lexer, scanner);
      advance(lexer, scanner);
      if (cursor(lexer) == '/' || cursor(lexer) == '*') {
        return true;
      }
    }

    if (match_str_now(lexer, scanner, "TODO")) return true;
    if (match_str_now(lexer, scanner, "#")) return true;
    if (match_str_now(lexer, scanner, "->")) return true;

    // <> glue and <- threads
    if (cursor(lexer) == '<') {
      checkpoint(lexer, scanner);
      advance(lexer, scanner);
      if (cursor(lexer) == '>' || cursor(lexer) == '-') return true;
    }

    if (scanner->in_choice_line && match_str_now(lexer, scanner, "[")) return true;
    if (scanner->in_choice_line && match_str_now(lexer, scanner, "]")) return true;

    if (scanner->inline_sequence_depth && match_str_now(lexer, scanner, "|")) return true;

    if (match_str_now(lexer, scanner, "{")) return true;
    if (match_str_now(lexer, scanner, "}")) return true;

    advance(lexer, scanner);
  }
}

static bool parse_starting_at_opening_brace(TSLexer *lexer, Scanner *scanner, BitValid valid_symbols) {
  lexer->log(lexer, "parse_starting_at_opening_brace");

  // we can tell if we're in choice conditions just by context, don't need to
  // parse further
  if (bit_get(valid_symbols, CHOICE_CONDITION_OPEN) && scanner->in_choice_line) {
    lexer->result_symbol = CHOICE_CONDITION_OPEN;
    scanner->in_choice_condition = true;
    advance(lexer, scanner);
    checkpoint(lexer, scanner);
    return true;
  }

  advance(lexer, scanner);
  checkpoint(lexer, scanner);

  BitValid sym_valid_upper = valid_symbols;
  BitValid sym_valid_lower = bit_init_empty();

  size_t depth = 1;
  size_t pipes_seen = 0;

  for (;;) {
    if (depth == 0) {
      lexer->log(lexer, "upper:");
      print_bit_valid(lexer, sym_valid_upper);
      lexer->log(lexer, "lower:");
      print_bit_valid(lexer, sym_valid_lower);

      // DONE! figure out what we could be
      if (bit_get(sym_valid_upper, INLINE_CONDITIONAL_OPEN) && bit_get(sym_valid_lower, INLINE_CONDITIONAL_OPEN)) {
        lexer->result_symbol = INLINE_CONDITIONAL_OPEN;
        scanner->inline_conditional_depth++;
        return true;
      }

      if (bit_get(sym_valid_upper, SWITCH_CASE_OPEN) && bit_get(sym_valid_lower, SWITCH_CASE_OPEN)) {
        lexer->result_symbol = SWITCH_CASE_OPEN;
        scanner->switch_case_depth++;
        return true;
      }

      if (bit_get(sym_valid_upper, INLINE_SEQUENCE_OPEN) && bit_get(sym_valid_lower, INLINE_SEQUENCE_OPEN)) {
        lexer->result_symbol = INLINE_SEQUENCE_OPEN;
        scanner->inline_sequence_depth++;
        return true;
      }

      return false;
    }

    if (lexer->eof(lexer)) {
      return !depth;
    }

    if (cursor(lexer) == '{') depth++;
    if (cursor(lexer) == '}') depth--;

    if (depth == 1) {
      if (cursor(lexer) == ':') {
        bit_set(&sym_valid_upper, INLINE_SEQUENCE_OPEN, false);
        bit_set(&sym_valid_lower, INLINE_CONDITIONAL_OPEN, true);
        bit_set(&sym_valid_lower, SWITCH_CASE_OPEN, true);
      }

      if (cursor(lexer) == '\n') {
        bit_set(&sym_valid_upper, INLINE_SEQUENCE_OPEN, false);
        bit_set(&sym_valid_upper, INLINE_CONDITIONAL_OPEN, false);

        bit_set(&sym_valid_lower, INLINE_SEQUENCE_OPEN, false);
        bit_set(&sym_valid_lower, INLINE_CONDITIONAL_OPEN, false);
        bit_set(&sym_valid_lower, SWITCH_CASE_OPEN, true);
      }

      if (cursor(lexer) == '|') {
        pipes_seen++;
        bit_set(&sym_valid_lower, INLINE_SEQUENCE_OPEN, true);

        bit_set(&sym_valid_upper, SWITCH_CASE_OPEN, false);
        if (pipes_seen > 1) {
          bit_set(&sym_valid_upper, INLINE_CONDITIONAL_OPEN, false);
        }
      }
    }

    advance(lexer, scanner);
  }

  return false;
}

static void print_init_log(TSLexer *lexer, BitValid valid_symbols) {
  lexer->log(lexer, "enter external scanner");
  print_bit_valid(lexer, valid_symbols);
}

static void trim_leading_whitespace(TSLexer *lexer) {
  lexer->log(lexer, "start at '%lc'", cursor(lexer));
  while (cursor(lexer) == ' ' || cursor(lexer) == '\t') {
    skip(lexer);
  }
  lexer->log(lexer, "start at '%lc' after skipping whitespace", cursor(lexer));
}

static void reset_scanner_at_start_of_line(TSLexer *lexer, Scanner *scanner) {
  bool start_at_col_0 = lexer->get_column(lexer) == 0;
  if (start_at_col_0) {
    scanner->in_choice_line = false;
  }
}

static void reset_scanner_at_start_of_parse(TSLexer *lexer, Scanner *scanner) {
  scanner->chars_advanced_this_parse = 0;
  scanner->chars_matched_this_parse = 0;
}

bool tree_sitter_ink_external_scanner_scan(void *payload, TSLexer *lexer, const bool *_valid_symbols) {
  BitValid valid_symbols = bit_init(_valid_symbols);
  Scanner *scanner = (Scanner *)payload;
  print_init_log(lexer, valid_symbols);

  reset_scanner_at_start_of_line(lexer, scanner);
  reset_scanner_at_start_of_parse(lexer, scanner);

  bool start_at_col_0 = lexer->get_column(lexer) == 0;

  trim_leading_whitespace(lexer);

  if (cursor((lexer)) == '\n') {
    scanner->in_choice_line = false;
    scanner->in_gather_line = false;
    return false;
  }

  if (bit_get(valid_symbols, CHOICE_BULLETS) && start_at_col_0 && (cursor(lexer) == '*' || cursor(lexer) == '+')) {
    parse_as_choice_bullets(scanner, lexer);
    lexer->result_symbol = CHOICE_BULLETS;
    scanner->in_choice_line = true;
    return true;
  }

  if (bit_get(valid_symbols, GATHER_BULLETS) && start_at_col_0 && (cursor(lexer) == '-')) {
    parse_as_gather_bullets(scanner, lexer);
    if (scanner->chars_matched_this_parse == 0) {
      return false;
    }
    lexer->result_symbol = GATHER_BULLETS;
    scanner->in_gather_line = true;
    return true;
  }

  if (cursor(lexer) == '{') return parse_starting_at_opening_brace(lexer, scanner, valid_symbols);

  if (bit_get(valid_symbols, INLINE_SEQUENCE_SEP) && cursor(lexer) == '|' && scanner->inline_sequence_depth) {
    lexer->result_symbol = INLINE_SEQUENCE_SEP;
    advance(lexer, scanner);
    checkpoint(lexer, scanner);
    return true;
  }

  if (scanner->chars_matched_this_parse != 0) {
    return false;
  }

  if (bit_get(valid_symbols, CONTENT_TEXT_DELIM_QUOTE) && parse_as_content_text(scanner, lexer, '"')) {
    if (scanner->chars_matched_this_parse == 0) return false;
    lexer->result_symbol = CONTENT_TEXT_DELIM_QUOTE;
    return true;
  }

  if (bit_get(valid_symbols, CONTENT_TEXT_DELIM_PIPE) && parse_as_content_text(scanner, lexer, '|')) {
    if (scanner->chars_matched_this_parse == 0) return false;
    lexer->result_symbol = CONTENT_TEXT_DELIM_PIPE;
    return true;
  }

  if (bit_get(valid_symbols, CONTENT_TEXT) && parse_as_content_text(scanner, lexer, 0)) {
    if (scanner->chars_matched_this_parse == 0) return false;
    lexer->result_symbol = CONTENT_TEXT;
    return true;
  }

  return false;
}
