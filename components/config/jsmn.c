/* JSMN Implementation */
#include "jsmn.h"

void jsmn_init(jsmn_parser *parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens, size_t num_tokens) {
    if (parser->toknext >= num_tokens) return NULL;
    jsmntok_t *tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
    tok->parent = -1;
    return tok;
}

int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, unsigned int num_tokens) {
    jsmntok_t *token;
    int count = parser->toknext;

    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        char c = js[parser->pos];
        jsmntype_t type;

        switch (c) {
            case '{': case '[':
                count++;
                token = jsmn_alloc_token(parser, tokens, num_tokens);
                if (token == NULL) return -1;
                if (parser->toksuper != -1) tokens[parser->toksuper].size++;
                token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
                token->start = parser->pos;
                parser->toksuper = parser->toknext - 1;
                break;
            case '}': case ']':
                type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
                for (int i = parser->toknext - 1; i >= 0; i--) {
                    token = &tokens[i];
                    if (token->start != -1 && token->end == -1) {
                        if (token->type != type) return -1;
                        parser->toksuper = token->parent;
                        token->end = parser->pos + 1;
                        break;
                    }
                }
                break;
            case '\"':
                parser->pos++;
                int start = parser->pos;
                for (; parser->pos < len && js[parser->pos] != '\"'; parser->pos++) {
                    if (js[parser->pos] == '\\' && parser->pos + 1 < len) parser->pos++;
                }
                token = jsmn_alloc_token(parser, tokens, num_tokens);
                if (token == NULL) return -1;
                token->type = JSMN_STRING;
                token->start = start;
                token->end = parser->pos;
                if (parser->toksuper != -1) tokens[parser->toksuper].size++;
                break;
            case ' ': case '\t': case '\r': case '\n': case ':': case ',':
                break;
            default:
                if ((c >= '0' && c <= '9') || c == '-' || c == 't' || c == 'f' || c == 'n') {
                    token = jsmn_alloc_token(parser, tokens, num_tokens);
                    if (token == NULL) return -1;
                    token->type = JSMN_PRIMITIVE;
                    token->start = parser->pos;
                    for (; parser->pos < len; parser->pos++) {
                        c = js[parser->pos];
                        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                            c == ',' || c == ']' || c == '}') {
                            parser->pos--;
                            break;
                        }
                    }
                    token->end = parser->pos + 1;
                    if (parser->toksuper != -1) tokens[parser->toksuper].size++;
                } else {
                    return -1;
                }
                break;
        }
    }

    return parser->toknext;
}