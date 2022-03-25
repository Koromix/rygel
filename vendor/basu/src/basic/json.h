/* SPDX-License-Identifier: LGPL-2.1+ */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "string-util.h"
#include "util.h"

/*
  In case you wonder why we have our own JSON implementation, here are a couple of reasons why this implementation has
  benefits over various other implementatins:

  - We need support for 64bit signed and unsigned integers, i.e. the full 64,5bit range of -9223372036854775808…18446744073709551615
  - All our variants are immutable after creation
  - Special values such as true, false, zero, null, empty strings, empty array, empty objects require zero dynamic memory
  - Progressive parsing
  - Our integer/real type implicitly converts, but only if that's safe and loss-lessly possible
  - There's a "builder" for putting together objects easily in varargs function calls
  - There's a "dispatcher" for mapping objects to C data structures
  - Every variant optionally carries parsing location information, which simplifies debugging and parse log error generation
  - Formatter has color, line, column support

  Limitations:
  - Doesn't allow embedded NUL in strings
  - Can't store integers outside of the -9223372036854775808…18446744073709551615 range (it will use 'long double' for
    values outside this range, which is lossy)
  - Can't store negative zero (will be treated identical to positive zero, and not retained across serialization)
  - Can't store non-integer numbers that can't be stored in "long double" losslessly
  - Allows creation and parsing of objects with duplicate keys. The "dispatcher" will refuse them however. This means
    we can parse and pass around such objects, but will carefully refuse them when we convert them into our own data.

  (These limitations should be pretty much in line with those of other JSON implementations, in fact might be less
  limiting in most cases even.)
*/

typedef struct JsonVariant JsonVariant;

typedef enum JsonVariantType {
        JSON_VARIANT_STRING,
        JSON_VARIANT_INTEGER,
        JSON_VARIANT_UNSIGNED,
        JSON_VARIANT_REAL,
        JSON_VARIANT_NUMBER, /* This a pseudo-type: we can never create variants of this type, but we use it as wildcard check for the above three types */
        JSON_VARIANT_BOOLEAN,
        JSON_VARIANT_ARRAY,
        JSON_VARIANT_OBJECT,
        JSON_VARIANT_NULL,
        _JSON_VARIANT_TYPE_MAX,
        _JSON_VARIANT_TYPE_INVALID = -1
} JsonVariantType;

int json_variant_new_stringn(JsonVariant **ret, const char *s, size_t n);
int json_variant_new_integer(JsonVariant **ret, intmax_t i);
int json_variant_new_unsigned(JsonVariant **ret, uintmax_t u);
int json_variant_new_real(JsonVariant **ret, long double d);
int json_variant_new_boolean(JsonVariant **ret, bool b);
int json_variant_new_array(JsonVariant **ret, JsonVariant **array, size_t n);
int json_variant_new_object(JsonVariant **ret, JsonVariant **array, size_t n);
int json_variant_new_null(JsonVariant **ret);

static inline int json_variant_new_string(JsonVariant **ret, const char *s) {
        return json_variant_new_stringn(ret, s, strlen_ptr(s));
}

JsonVariant *json_variant_ref(JsonVariant *v);
JsonVariant *json_variant_unref(JsonVariant *v);
void json_variant_unref_many(JsonVariant **array, size_t n);

DEFINE_TRIVIAL_CLEANUP_FUNC(JsonVariant *, json_variant_unref);

const char *json_variant_string(JsonVariant *v);
intmax_t json_variant_integer(JsonVariant *v);
uintmax_t json_variant_unsigned(JsonVariant *v);
long double json_variant_real(JsonVariant *v);
bool json_variant_boolean(JsonVariant *v);

JsonVariantType json_variant_type(JsonVariant *v);
bool json_variant_has_type(JsonVariant *v, JsonVariantType type);

static inline bool json_variant_is_string(JsonVariant *v) {
        return json_variant_has_type(v, JSON_VARIANT_STRING);
}

bool json_variant_is_negative(JsonVariant *v);

size_t json_variant_elements(JsonVariant *v);
JsonVariant *json_variant_by_index(JsonVariant *v, size_t index);
JsonVariant *json_variant_by_key(JsonVariant *v, const char *key);
JsonVariant *json_variant_by_key_full(JsonVariant *v, const char *key, JsonVariant **ret_key);

bool json_variant_equal(JsonVariant *a, JsonVariant *b);

enum {
        JSON_FORMAT_NEWLINE = 1 << 0, /* suffix with newline */
        JSON_FORMAT_PRETTY  = 1 << 1, /* add internal whitespace to appeal to human readers */
        JSON_FORMAT_COLOR   = 1 << 2, /* insert ANSI color sequences */
        JSON_FORMAT_SOURCE  = 1 << 3, /* prefix with source filename/line/column */
        JSON_FORMAT_SSE     = 1 << 4, /* prefix/suffix with W3C server-sent events */
        JSON_FORMAT_SEQ     = 1 << 5, /* prefix/suffix with RFC 7464 application/json-seq */
};

void json_variant_dump(JsonVariant *v, unsigned flags, FILE *f, const char *prefix);

enum {
        _JSON_BUILD_STRING,
        _JSON_BUILD_OBJECT_BEGIN,
        _JSON_BUILD_OBJECT_END,
        _JSON_BUILD_PAIR,
        _JSON_BUILD_VARIANT,
        _JSON_BUILD_MAX,
};

#define JSON_BUILD_STRING(s) _JSON_BUILD_STRING, ({ const char *_x = s; _x; })
#define JSON_BUILD_OBJECT(...) _JSON_BUILD_OBJECT_BEGIN, __VA_ARGS__, _JSON_BUILD_OBJECT_END
#define JSON_BUILD_PAIR(n, ...) _JSON_BUILD_PAIR, ({ const char *_x = n; _x; }), __VA_ARGS__
#define JSON_BUILD_VARIANT(v) _JSON_BUILD_VARIANT, ({ JsonVariant *_x = v; _x; })

int json_build(JsonVariant **ret, ...);
int json_buildv(JsonVariant **ret, va_list ap);
