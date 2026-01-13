#include <ccap.h>

#include <iostream>

int main() {
    // Minimal smoke test: no camera access.
    auto s = ccap::pixelFormatToString(ccap::PixelFormat::BGR24);
    std::cout << "ccap pixel format: " << s << "\n";
    return 0;
}
