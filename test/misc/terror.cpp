#include <iostream>
#include "error.h"
using namespace std;

int main() {
    Pos p{"1", 1, 3};
    warnp(p, "visible warn");
    enable_warning = false;
    warnp(p, "pos: %d %d: invisible warn");

    errorp(p, "unknown error: %s", "xixixi");
    error("internal error: unknown error");
    errorp(p, "unknown error: %s", "xixixi");
}