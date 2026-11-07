#include "core/Composer.hpp"

#include <chrono>
#include <iostream>

int main() {
    using namespace ec;
    for (auto style : {DemoStyle::Chamber, DemoStyle::Cinematic, DemoStyle::Kinetic}) {
        const auto spec = demoSpec(style, 1234);
        const auto start = std::chrono::steady_clock::now();
        GenerationDiagnostics diag;
        const Project p = composeProject(spec, &diag);
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        int notes = 0;
        for (const auto& t : p.tracks) notes += static_cast<int>(t.notes.size());
        std::cout << demoId(style) << ": notes=" << notes << " chords=" << p.chords.size()
                  << " phrases=" << p.phrases.size() << " bars=" << p.totalBars()
                  << " compose_ms=" << elapsed << "\n";
    }
    return 0;
}
