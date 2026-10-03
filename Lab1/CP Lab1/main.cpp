#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>

// 1. Dati del ciclismo da prendere da file
struct SelectedDataStructure {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

struct SelectedDataStructureWithComputedValue {
    SelectedDataStructure originalData;
    double normalizedPower;
};

// 2. Calcolo computazionalmente pesante (CPU-bound: 200.000 iterazioni)
double computeNormalizedPower(const SelectedDataStructure& climb) {
    double accumulatedStress = 0.0;
    const int STEPS = 200000;
    double frequency = 1.0 / static_cast<double>(climb.durationSeconds);

    for (int i = 0; i < STEPS; ++i) {
        double deltaWatts = std::sin(i * frequency) * 25.0;
        double instantaneousWatts = climb.averageWatts + deltaWatts;
        accumulatedStress += std::pow(instantaneousWatts, 4.0);
    }
    return std::pow(accumulatedStress / STEPS, 0.25);
}

int main() {
    return 0;
}