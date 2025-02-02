#ifndef RGB_DEFINE_HELPER
#define RGB_DEFINE_HELPER

#define RGB_GROUP_HELPER(a, b, c, d, ...) {a, b, c, d}
#define RGB_GROUP(...) RGB_GROUP_HELPER(__VA_ARGS__, -1, -1, -1, -1)

#endif