#include "../src/formats/StandardFont.h"
#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace fontmaster;
using Bytes = std::vector<uint8_t>;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
// Complete deterministic FontTools FontBuilder fixtures: .notdef,A,B,space,Acomp,last.
// The TrueType Acomp references A; CFF/CFF2 use independent outlines.
static const Bytes ttf = {
    0,   1,   0,   0,   0,   11,  0,   128, 0,   3,   0,   48,  79,  83,  47,  50,  69,  0,   67,  234, 0,
    0,   1,   56,  0,   0,   0,   96,  84,  69,  83,  84,  24,  31,  164, 241, 0,   0,   3,   120, 0,   0,
    0,   30,  99,  109, 97,  112, 0,   95,  1,   49,  0,   0,   1,   168, 0,   0,   0,   74,  103, 108, 121,
    102, 75,  244, 241, 190, 0,   0,   2,   4,   0,   0,   0,   112, 104, 101, 97,  100, 89,  3,   164, 156,
    0,   0,   0,   188, 0,   0,   0,   54,  104, 104, 101, 97,  4,   178, 1,   146, 0,   0,   0,   244, 0,
    0,   0,   36,  104, 109, 116, 120, 1,   244, 0,   0,   0,   0,   1,   152, 0,   0,   0,   14,  108, 111,
    99,  97,  0,   116, 0,   92,  0,   0,   1,   244, 0,   0,   0,   14,  109, 97,  120, 112, 0,   10,  0,
    11,  0,   0,   1,   24,  0,   0,   0,   32,  110, 97,  109, 101, 133, 181, 129, 97,  0,   0,   2,   116,
    0,   0,   0,   198, 112, 111, 115, 116, 221, 13,  213, 75,  0,   0,   3,   60,  0,   0,   0,   57,  0,
    1,   0,   0,   0,   1,   0,   0,   120, 148, 144, 132, 95,  15,  60,  245, 0,   1,   3,   232, 0,   0,
    0,   0,   124, 37,  176, 128, 0,   0,   0,   0,   124, 37,  176, 128, 0,   0,   0,   0,   1,   164, 2,
    188, 0,   0,   0,   3,   0,   2,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   3,   32,  255, 56,
    0,   0,   1,   244, 0,   0,   0,   100, 1,   144, 0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   1,   0,   1,   0,   0,   0,   6,   0,   4,   0,   1,   0,   4,   0,   1,
    0,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   1,   0,   3,   1,
    244, 1,   144, 0,   5,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   63,  63,  63,  63,  0,   0,   0,   32,
    0,   90,  3,   32,  255, 56,  0,   0,   3,   32,  0,   200, 0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   32,  0,   0,   1,   244, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   2,   0,   0,   0,   3,   0,   0,   0,   20,  0,   3,   0,   1,   0,
    0,   0,   20,  0,   4,   0,   54,  0,   0,   0,   8,   0,   8,   0,   2,   0,   0,   0,   32,  0,   67,
    0,   90,  255, 255, 0,   0,   0,   32,  0,   65,  0,   90,  255, 255, 255, 227, 0,   0,   255, 171, 0,
    1,   0,   0,   0,   6,   0,   0,   0,   0,   0,   1,   0,   2,   0,   4,   0,   0,   0,   0,   0,   12,
    0,   24,  0,   36,  0,   36,  0,   44,  0,   56,  0,   0,   0,   1,   0,   0,   0,   0,   1,   144, 2,
    188, 0,   3,   0,   0,   49,  33,  17,  33,  1,   144, 254, 112, 2,   188, 0,   1,   0,   0,   0,   0,
    1,   144, 2,   188, 0,   3,   0,   0,   49,  33,  17,  33,  1,   144, 254, 112, 2,   188, 0,   1,   0,
    0,   0,   0,   1,   144, 2,   188, 0,   3,   0,   0,   49,  33,  17,  33,  1,   144, 254, 112, 2,   188,
    255, 255, 0,   20,  0,   0,   1,   164, 2,   188, 0,   6,   0,   1,   20,  0,   0,   1,   0,   0,   0,
    0,   1,   144, 2,   188, 0,   3,   0,   0,   49,  33,  17,  33,  1,   144, 254, 112, 2,   188, 0,   0,
    0,   10,  0,   126, 0,   1,   0,   0,   0,   0,   0,   1,   0,   17,  0,   0,   0,   1,   0,   0,   0,
    0,   0,   2,   0,   7,   0,   17,  0,   1,   0,   0,   0,   0,   0,   3,   0,   17,  0,   0,   0,   1,
    0,   0,   0,   0,   0,   4,   0,   17,  0,   0,   0,   1,   0,   0,   0,   0,   0,   6,   0,   17,  0,
    0,   0,   3,   0,   1,   4,   9,   0,   1,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   2,
    0,   14,  0,   58,  0,   3,   0,   1,   4,   9,   0,   3,   0,   34,  0,   24,  0,   3,   0,   1,   4,
    9,   0,   4,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   6,   0,   34,  0,   24,  83,  121,
    110, 116, 104, 101, 116, 105, 99,  83,  116, 97,  110, 100, 97,  114, 100, 82,  101, 103, 117, 108, 97,
    114, 0,   83,  0,   121, 0,   110, 0,   116, 0,   104, 0,   101, 0,   116, 0,   105, 0,   99,  0,   83,
    0,   116, 0,   97,  0,   110, 0,   100, 0,   97,  0,   114, 0,   100, 0,   82,  0,   101, 0,   103, 0,
    117, 0,   108, 0,   97,  0,   114, 0,   0,   0,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    6,   0,   0,   0,   36,  0,   37,  0,   3,   1,   2,   1,   3,   5,   65,  99,  111, 109, 112, 4,   108,
    97,  115, 116, 0,   0,   0,   112, 114, 101, 115, 101, 114, 118, 101, 32,  117, 110, 114, 101, 108, 97,
    116, 101, 100, 32,  99,  117, 115, 116, 111, 109, 32,  100, 97,  116, 97,  0,   0,
};
static const Bytes cff = {
    79,  84,  84,  79,  0,   10,  0,   128, 0,   3,   0,   32,  67,  70,  70,  32,  1,   156, 181, 25,  0,
    0,   2,   164, 0,   0,   0,   173, 79,  83,  47,  50,  69,  0,   67,  234, 0,   0,   1,   16,  0,   0,
    0,   96,  84,  69,  83,  84,  24,  31,  164, 241, 0,   0,   3,   84,  0,   0,   0,   30,  99,  109, 97,
    112, 0,   95,  1,   49,  0,   0,   2,   56,  0,   0,   0,   74,  104, 101, 97,  100, 88,  241, 164, 156,
    0,   0,   0,   172, 0,   0,   0,   54,  104, 104, 101, 97,  4,   178, 1,   146, 0,   0,   0,   228, 0,
    0,   0,   36,  104, 109, 116, 120, 1,   244, 0,   0,   0,   0,   3,   116, 0,   0,   0,   14,  109, 97,
    120, 112, 0,   6,   80,  0,   0,   0,   1,   8,   0,   0,   0,   6,   110, 97,  109, 101, 133, 181, 129,
    97,  0,   0,   1,   112, 0,   0,   0,   198, 112, 111, 115, 116, 0,   3,   0,   0,   0,   0,   2,   132,
    0,   0,   0,   32,  0,   1,   0,   0,   0,   1,   0,   0,   9,   177, 88,  190, 95,  15,  60,  245, 0,
    3,   3,   232, 0,   0,   0,   0,   124, 37,  176, 128, 0,   0,   0,   0,   124, 37,  176, 128, 0,   0,
    0,   0,   1,   144, 2,   188, 0,   0,   0,   3,   0,   2,   0,   0,   0,   0,   0,   0,   0,   1,   0,
    0,   3,   32,  255, 56,  0,   0,   1,   244, 0,   0,   0,   100, 1,   144, 0,   1,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   80,  0,   0,   6,   0,   0,   0,
    3,   1,   244, 1,   144, 0,   5,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   63,  63,  63,  63,  0,   0,
    0,   32,  0,   90,  3,   32,  255, 56,  0,   0,   3,   32,  0,   200, 0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   32,  0,   0,   0,   0,   0,   10,  0,   126, 0,   1,   0,   0,
    0,   0,   0,   1,   0,   17,  0,   0,   0,   1,   0,   0,   0,   0,   0,   2,   0,   7,   0,   17,  0,
    1,   0,   0,   0,   0,   0,   3,   0,   17,  0,   0,   0,   1,   0,   0,   0,   0,   0,   4,   0,   17,
    0,   0,   0,   1,   0,   0,   0,   0,   0,   6,   0,   17,  0,   0,   0,   3,   0,   1,   4,   9,   0,
    1,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   2,   0,   14,  0,   58,  0,   3,   0,   1,
    4,   9,   0,   3,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   4,   0,   34,  0,   24,  0,
    3,   0,   1,   4,   9,   0,   6,   0,   34,  0,   24,  83,  121, 110, 116, 104, 101, 116, 105, 99,  83,
    116, 97,  110, 100, 97,  114, 100, 82,  101, 103, 117, 108, 97,  114, 0,   83,  0,   121, 0,   110, 0,
    116, 0,   104, 0,   101, 0,   116, 0,   105, 0,   99,  0,   83,  0,   116, 0,   97,  0,   110, 0,   100,
    0,   97,  0,   114, 0,   100, 0,   82,  0,   101, 0,   103, 0,   117, 0,   108, 0,   97,  0,   114, 0,
    0,   0,   0,   0,   2,   0,   0,   0,   3,   0,   0,   0,   20,  0,   3,   0,   1,   0,   0,   0,   20,
    0,   4,   0,   54,  0,   0,   0,   8,   0,   8,   0,   2,   0,   0,   0,   32,  0,   67,  0,   90,  255,
    255, 0,   0,   0,   32,  0,   65,  0,   90,  255, 255, 255, 227, 0,   0,   255, 171, 0,   1,   0,   0,
    0,   6,   0,   0,   0,   0,   0,   1,   0,   2,   0,   4,   0,   0,   0,   3,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   1,   0,   4,   1,   0,   1,   1,   1,   18,  83,  121, 110, 116, 104, 101, 116, 105,
    99,  83,  116, 97,  110, 100, 97,  114, 100, 0,   1,   1,   1,   25,  248, 29,  2,   248, 29,  3,   248,
    24,  4,   139, 139, 248, 36,  249, 80,  5,   229, 15,  139, 247, 65,  18,  239, 17,  0,   3,   1,   1,
    6,   10,  27,  65,  99,  111, 109, 112, 108, 97,  115, 116, 83,  121, 110, 116, 104, 101, 116, 105, 99,
    83,  116, 97,  110, 100, 97,  114, 100, 0,   0,   1,   0,   34,  1,   0,   1,   0,   1,   135, 1,   0,
    6,   1,   1,   13,  25,  37,  40,  52,  64,  248, 136, 139, 22,  248, 36,  249, 80,  252, 36,  6,   14,
    248, 136, 139, 22,  248, 36,  249, 80,  252, 36,  6,   14,  248, 136, 139, 22,  248, 36,  249, 80,  252,
    36,  6,   14,  248, 136, 14,  248, 136, 139, 22,  248, 36,  249, 80,  252, 36,  6,   14,  248, 136, 139,
    22,  248, 36,  249, 80,  252, 36,  6,   14,  0,   0,   0,   112, 114, 101, 115, 101, 114, 118, 101, 32,
    117, 110, 114, 101, 108, 97,  116, 101, 100, 32,  99,  117, 115, 116, 111, 109, 32,  100, 97,  116, 97,
    0,   0,   1,   244, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
static const Bytes cff2 = {
    79,  84,  84,  79,  0,   10,  0,   128, 0,   3,   0,   32,  67,  70,  70,  50,  7,   168, 117, 176, 0,
    0,   2,   180, 0,   0,   0,   81,  79,  83,  47,  50,  69,  0,   67,  234, 0,   0,   1,   16,  0,   0,
    0,   96,  84,  69,  83,  84,  24,  31,  164, 241, 0,   0,   3,   8,   0,   0,   0,   30,  99,  109, 97,
    112, 0,   95,  1,   49,  0,   0,   1,   128, 0,   0,   0,   74,  104, 101, 97,  100, 88,  241, 164, 156,
    0,   0,   0,   172, 0,   0,   0,   54,  104, 104, 101, 97,  4,   178, 1,   146, 0,   0,   0,   228, 0,
    0,   0,   36,  104, 109, 116, 120, 1,   244, 0,   0,   0,   0,   1,   112, 0,   0,   0,   14,  109, 97,
    120, 112, 0,   6,   80,  0,   0,   0,   1,   8,   0,   0,   0,   6,   110, 97,  109, 101, 133, 181, 129,
    97,  0,   0,   1,   204, 0,   0,   0,   198, 112, 111, 115, 116, 0,   3,   0,   0,   0,   0,   2,   148,
    0,   0,   0,   32,  0,   1,   0,   0,   0,   1,   0,   0,   253, 153, 218, 102, 95,  15,  60,  245, 0,
    3,   3,   232, 0,   0,   0,   0,   124, 37,  176, 128, 0,   0,   0,   0,   124, 37,  176, 128, 0,   0,
    0,   0,   1,   144, 2,   188, 0,   0,   0,   3,   0,   2,   0,   0,   0,   0,   0,   0,   0,   1,   0,
    0,   3,   32,  255, 56,  0,   0,   1,   244, 0,   0,   0,   100, 1,   144, 0,   1,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   80,  0,   0,   6,   0,   0,   0,
    3,   1,   244, 1,   144, 0,   5,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   63,  63,  63,  63,  0,   0,
    0,   32,  0,   90,  3,   32,  255, 56,  0,   0,   3,   32,  0,   200, 0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   32,  0,   0,   1,   244, 0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   0,   0,   0,   3,   0,   0,   0,   20,  0,   3,   0,
    1,   0,   0,   0,   20,  0,   4,   0,   54,  0,   0,   0,   8,   0,   8,   0,   2,   0,   0,   0,   32,
    0,   67,  0,   90,  255, 255, 0,   0,   0,   32,  0,   65,  0,   90,  255, 255, 255, 227, 0,   0,   255,
    171, 0,   1,   0,   0,   0,   6,   0,   0,   0,   0,   0,   1,   0,   2,   0,   4,   0,   0,   0,   0,
    0,   10,  0,   126, 0,   1,   0,   0,   0,   0,   0,   1,   0,   17,  0,   0,   0,   1,   0,   0,   0,
    0,   0,   2,   0,   7,   0,   17,  0,   1,   0,   0,   0,   0,   0,   3,   0,   17,  0,   0,   0,   1,
    0,   0,   0,   0,   0,   4,   0,   17,  0,   0,   0,   1,   0,   0,   0,   0,   0,   6,   0,   17,  0,
    0,   0,   3,   0,   1,   4,   9,   0,   1,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   2,
    0,   14,  0,   58,  0,   3,   0,   1,   4,   9,   0,   3,   0,   34,  0,   24,  0,   3,   0,   1,   4,
    9,   0,   4,   0,   34,  0,   24,  0,   3,   0,   1,   4,   9,   0,   6,   0,   34,  0,   24,  83,  121,
    110, 116, 104, 101, 116, 105, 99,  83,  116, 97,  110, 100, 97,  114, 100, 82,  101, 103, 117, 108, 97,
    114, 0,   83,  0,   121, 0,   110, 0,   116, 0,   104, 0,   101, 0,   116, 0,   105, 0,   99,  0,   83,
    0,   116, 0,   97,  0,   110, 0,   100, 0,   97,  0,   114, 0,   100, 0,   82,  0,   101, 0,   103, 0,
    117, 0,   108, 0,   97,  0,   114, 0,   0,   0,   3,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,
    0,   5,   0,   5,   210, 12,  36,  153, 17,  0,   0,   0,   0,   0,   0,   0,   6,   1,   1,   10,  19,
    28,  28,  37,  46,  139, 22,  248, 36,  249, 80,  252, 36,  6,   139, 22,  248, 36,  249, 80,  252, 36,
    6,   139, 22,  248, 36,  249, 80,  252, 36,  6,   139, 22,  248, 36,  249, 80,  252, 36,  6,   139, 22,
    248, 36,  249, 80,  252, 36,  6,   0,   0,   0,   1,   1,   1,   4,   139, 220, 18,  0,   0,   0,   112,
    114, 101, 115, 101, 114, 118, 101, 32,  117, 110, 114, 101, 108, 97,  116, 101, 100, 32,  99,  117, 115,
    116, 111, 109, 32,  100, 97,  116, 97,  0,   0,
};
static void write(const std::string &path, const Bytes &bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    require(file.good(), "write fixture");
}
template <class Action> static void rejects(Action action, const char *message) {
    bool threw = false;
    try {
        action();
    } catch (const FontException &) {
        threw = true;
    }
    require(threw, message);
}
static Bytes table(const Bytes &bytes, const std::string &name) {
    const auto tables = utils::parseTTFTables(bytes);
    const auto *found = utils::findTable(tables, name);
    require(found, "expected table missing");
    return Bytes(bytes.begin() + found->offset, bytes.begin() + found->offset + found->length);
}
static void stableCount(const Bytes &bytes, uint16_t count = 6) {
    const auto maxp = table(bytes, "maxp");
    require(maxp.at(4) == 0 && maxp.at(5) == count, "glyph ID count changed after removal");
    const auto extra = table(bytes, "TEST");
    const std::string expected = "preserve unrelated custom data";
    require(std::string(extra.begin(), extra.end()) == expected, "custom table silently lost");
}
static Bytes redPNG() {
    return {137, 80,  78, 71, 13, 10,  26,  10, 0,   0,   0,   13,  73,  72,  68,  82, 0, 0,
            0,   1,   0,  0,  0,  1,   8,   6,  0,   0,   0,   31,  21,  196, 137, 0,  0, 0,
            13,  73,  68, 65, 84, 120, 156, 99, 248, 207, 192, 240, 31,  0,   5,   0,  1, 255,
            137, 153, 61, 29, 0,  0,   0,   0,  73,  69,  78,  68,  174, 66,  96,  130};
}
int main(int argc, char **argv) {
    try {
        const std::string dir = argc > 2 ? argv[2] : argc > 1 ? argv[1] : "/tmp";
        std::filesystem::create_directories(dir);
        for (const auto &entry :
             std::vector<std::pair<std::string, Bytes>>{{"ttf", ttf}, {"cff", cff}, {"cff2", cff2}}) {
            const auto path = dir + "/standard-" + entry.first + ".otf";
            write(path, entry.second);
            auto font = Font::load(path);
            require(font->getFormat() == FontFormat::STANDARD, "ordinary outline format not detected");
            require(font->listGlyphs().size() == 6, "outline or mapped whitespace missing");
            require(font->getGlyphInfo("glyph_2").format == "outline", "outline glyph metadata missing");
            require(font->getGlyphInfo("glyph_2").unicode == 66, "outline Unicode metadata missing");
            if (entry.first != "cff2")
                require(font->getGlyphInfo("A").unicode == 65, "source outline name was not resolved");
            require(!font->findGlyphName(32).empty(), "mapped whitespace unavailable");
            require(!font->removeGlyph("glyph_65538") && !font->removeGlyph("glyph_2junk"),
                    "malformed glyph alias accepted");
            rejects([&] { font->getGlyphInfo("glyph_2junk"); }, "invalid glyph lookup accepted");
            auto before = font->getFontData();
            rejects([&] { font->removeGlyph("glyph_0"); }, ".notdef removed unsafely");
            require(font->getFontData() == before, "failed .notdef removal mutated state");
            if (entry.first == "ttf") {
                rejects([&] { font->removeGlyph("glyph_1"); }, "referenced component removed unsafely");
                require(font->getFontData() == before, "failed component removal mutated state");
            }
            require(font->removeGlyph(uint32_t(66)), "ordinary outline removal failed");
            require(font->findGlyphName(66).empty(), "removed Unicode mapping retained");
            rejects([&] { font->getGlyphInfo("glyph_2"); }, "removed outline remains listed");
            require(font->getGlyphInfo("glyph_1").format == "outline", "unrelated outline lost");
            require(font->getGlyphInfo("glyph_5").format == "outline", "later glyph ID changed");
            require(font->listGlyphs().size() == 5, "outline removal count wrong");
            stableCount(font->getFontData());
            require(font->removeGlyph("glyph_5"), "trailing glyph removal failed");
            stableCount(font->getFontData(), 5);
            require(font->removeGlyph(uint32_t(32)), "whitespace removal failed");
            require(font->findGlyphName(32).empty(), "whitespace mapping remains");
            require(font->listGlyphs().size() == 3, "whitespace removal count wrong");
            before = font->getFontData();
            rejects([&] { font->setFontData(Bytes{0, 1, 0, 0}); }, "malformed setFontData accepted");
            require(font->getFontData() == before, "failed setFontData mutated font");
            require(!font->replaceGlyphImage("glyph_1", {1, 2, 3}), "invalid replacement accepted");
            require(font->getFontData() == before, "failed replacement mutated font");
            require(font->replaceGlyphImage("glyph_1", redPNG()), "ordinary PNG replacement failed");
            const auto image = font->getGlyphInfo("glyph_1");
            require(image.format == "svg", "PNG presentation was not persisted as SVG");
            require(std::string(image.image_data.begin(), image.image_data.end())
                            .find("data:image/png;base64,") != std::string::npos,
                    "PNG image payload missing");
            const std::string svg = "<svg xmlns='http://www.w3.org/2000/svg'><rect y='-700' width='400' "
                                    "height='700' fill='red'/></svg>";
            require(font->replaceGlyphImage("glyph_1", Bytes(svg.begin(), svg.end())),
                    "ordinary SVG replacement failed");
            const auto output = dir + "/edited-standard-" + entry.first + ".otf";
            require(font->save(output), "ordinary save failed");
            auto reload = Font::load(output);
            require(reload->getGlyphInfo("glyph_1").format == "svg", "saved presentation lost");
            require(reload->getGlyphInfo("glyph_4").format == "outline", "SVG reload lost unrelated outline");
            require(reload->listGlyphs().size() == 3, "SVG reload lost ordinary glyphs");
            stableCount(reload->getFontData(), 5);
            // An override must not turn deletion into "restore old outline" after reload.
            auto removal = detail::createStandardFont(path);
            require(removal->replaceGlyphImage("glyph_2", redPNG()), "removal fixture replacement failed");
            const auto removalPath = dir + "/standard-removal-" + entry.first + ".otf";
            require(removal->save(removalPath), "removal fixture save failed");
            auto reloadedRemoval = Font::load(removalPath);
            require(reloadedRemoval->removeGlyph("glyph_2"), "saved override removal failed");
            require(reloadedRemoval->findGlyphName(66).empty(),
                    "saved override deletion restored its original outline");
            rejects([&] { reloadedRemoval->getGlyphInfo("glyph_2"); },
                    "deleted overridden glyph remains available");
            require(reloadedRemoval->listGlyphs().size() == 5,
                    "saved override deletion lost unrelated outlines");
            font->setFontData(entry.second);
            require(font->listGlyphs().size() == 6 && font->getGlyphInfo("glyph_1").format == "outline",
                    "setFontData failed to reset edits");
        }
        std::cout << "Standard font tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Standard test failure: " << e.what() << '\n';
        return 1;
    }
}
