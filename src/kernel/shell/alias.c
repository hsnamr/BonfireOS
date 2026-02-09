/**
 * Simple alias table: name -> value.
 * alias_parse_and_expand replaces first word with value if it's an alias.
 */

#include <kernel/alias.h>
#include <kernel/types.h>

static char alias_names[ALIAS_MAX][ALIAS_NAME_MAX];
static char alias_values[ALIAS_MAX][ALIAS_VALUE_MAX];
static size_t alias_count;

int alias_set(const char *name, const char *value)
{
    size_t i = 0;
    while (name[i] && i < ALIAS_NAME_MAX - 1) i++;
    if (!i || i >= ALIAS_NAME_MAX) return -1;
    size_t j = 0;
    while (value[j] && j < ALIAS_VALUE_MAX - 1) j++;
    for (size_t k = 0; k < alias_count; k++) {
        size_t n = 0;
        while (alias_names[k][n] && name[n] && alias_names[k][n] == name[n]) n++;
        if (!alias_names[k][n] && !name[n]) {
            for (size_t t = 0; t < j && t < ALIAS_VALUE_MAX - 1; t++) alias_values[k][t] = value[t];
            alias_values[k][j] = '\0';
            return 0;
        }
    }
    if (alias_count >= ALIAS_MAX) return -1;
    for (size_t t = 0; t < i; t++) alias_names[alias_count][t] = name[t];
    alias_names[alias_count][i] = '\0';
    for (size_t t = 0; t < j && t < ALIAS_VALUE_MAX - 1; t++) alias_values[alias_count][t] = value[t];
    alias_values[alias_count][j] = '\0';
    alias_count++;
    return 0;
}

const char *alias_get(const char *name)
{
    for (size_t k = 0; k < alias_count; k++) {
        size_t n = 0;
        while (alias_names[k][n] && name[n] && alias_names[k][n] == name[n]) n++;
        if (!alias_names[k][n] && !name[n]) return alias_values[k];
    }
    return NULL;
}

int alias_parse_and_expand(const char *line, char *out, size_t out_len)
{
    while (*line == ' ') line++;
    if (!*line) { out[0] = '\0'; return 0; }
    char word[ALIAS_NAME_MAX];
    size_t i = 0;
    while (*line && *line != ' ' && i < ALIAS_NAME_MAX - 1) word[i++] = *line++;
    word[i] = '\0';
    const char *exp = alias_get(word);
    if (exp) {
        size_t j = 0;
        while (*exp && j < out_len - 1) out[j++] = *exp++;
        while (*line == ' ') line++;
        if (*line && j < out_len - 1) out[j++] = ' ';
        while (*line && j < out_len - 1) out[j++] = *line++;
        out[j] = '\0';
        return 0;
    }
    /* No alias: copy full line (word + rest) to out */
    size_t j = 0;
    for (size_t k = 0; k < i && j < out_len - 1; k++) out[j++] = word[k];
    while (*line && j < out_len - 1) out[j++] = *line++;
    out[j] = '\0';
    return 0;
}
