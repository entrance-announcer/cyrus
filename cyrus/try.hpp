#pragma once


#define TRY(var, expression)                          \
  {                                                   \
    auto tmp_result = (expression);                   \
    if (tmp_result) [[likely]] {                      \
      var = tmp_result.value();                       \
    } else {                                          \
      return tl::make_unexpected(tmp_result.error()); \
    }                                                 \
  }
