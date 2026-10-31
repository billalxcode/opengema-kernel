#pragma once

// Canonical owner of kernel assertions.
// Macros are used only to capture the expression text.

#include "og/panic.h"

#define OG_ASSERT(expr)                       \
  do {                                        \
    if (!(expr)) {                            \
      ::og::Panic("assert: " #expr);          \
    }                                         \
  } while (false)

#define OG_ASSERT_MSG(expr, msg)              \
  do {                                        \
    if (!(expr)) {                            \
      ::og::Panic("assert: " #expr " " msg);  \
    }                                         \
  } while (false)

#define OG_UNREACHABLE() ::og::Panic("unreachable")
