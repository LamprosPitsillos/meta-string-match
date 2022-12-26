#include "meta_string_match.h"

using size_t = std::size_t;

CREATE_META_STRING_MATCHER(string_matcher, "hi", [](size_t) { });

int main() {
}
