#include <iostream>
#include <string>
#include <cmath>
#include <chrono>

struct SelectedDataStructure {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Funzione di calcolo: puoi regolare STEPS se vuoi più o meno carico
double computeNormalizedPower(const SelectedDataStructure& climb) {
    double accumulatedStress = 0.0;
    const int STEPS = 1500000; // Impostato a 500.000 iterazioni
    double frequency = 1.0 / static_cast<double>(climb.durationSeconds);

    for (int i = 0; i < STEPS; ++i) {
        double deltaWatts = std::sin(i * frequency) * 25.0;
        double instantaneousWatts = climb.averageWatts + deltaWatts;
        accumulatedStress += std::pow(instantaneousWatts, 4.0);
    }
    return std::pow(accumulatedStress / STEPS, 0.25);
}

int main() {
    SelectedDataStructure testClimb = {"Passo dello Stelvio", 4200, 345.5};

    std::cout << "Avvio calcolo per singolo elemento...\n";
    auto start = std::chrono::high_resolution_clock::now();

    double result = computeNormalizedPower(testClimb);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "Risultato: " << result << " W\n";
    std::cout << "Tempo per 1 elemento: " << duration.count() << " ms\n";
    std::cout << "Stima per 30 elementi in sequenziale: " 
              << (duration.count() * 30.0) / 1000.0 << " secondi\n";

    return 0;
}