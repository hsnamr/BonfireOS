#ifndef BONFIRE_ALIAS_H
#define BONFIRE_ALIAS_H

#include <kernel/types.h>

#define ALIAS_NAME_MAX  32
#define ALIAS_VALUE_MAX 128
#define ALIAS_MAX       16

int alias_set(const char *name, const char *value);
const char *alias_get(const char *name);
int alias_parse_and_expand(const char *line, char *out, size_t out_len);

#endif /* BONFIRE_ALIAS_H */
