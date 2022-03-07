#pragma once

// Converted from:
// https://github.com/SerenityOS/serenity/commit/8a879e205b9f336319572d181e5acf6c2fc31dfb
#define TRY(expression)                         \
  __extension__({                               \
    auto _tmp = (expression);                   \
    if (!_tmp) [[unlikely]]                     \
      return tl::make_unexpected(_tmp.error()); \
    _tmp.value();                               \
  })

#define REQ(expression)                                       \
  {                                                           \
    if (const auto _tmp = (expression); !_tmp) [[unlikely]] { \
      return tl::make_unexpected(_tmp.error());               \
    }                                                         \
  }
