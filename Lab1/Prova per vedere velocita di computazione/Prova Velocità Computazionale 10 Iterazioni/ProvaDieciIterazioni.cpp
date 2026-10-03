#include <iostream>
#include <string>
#include <thread>
#include <cmath>
#include <chrono>

struct SelectedDataStructure {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Funzione pesante: 500.000 iterazioni con calcoli trigonometrici e potenze
double computeNormalizedPower(const SelectedDataStructure& climb) {
    double accumulatedStress = 0.0;
    const int STEPS = 20000000;
    double frequency = 1.0 / static_cast<double>(climb.durationSeconds);

    for (int i = 0; i < STEPS; ++i) {
        double deltaWatts = std::sin(i * frequency) * 25.0;
        double instantaneousWatts = climb.averageWatts + deltaWatts;
        accumulatedStress += std::pow(instantaneousWatts, 4.0);
    }
    return std::pow(accumulatedStress / STEPS, 0.25);
}

// Funzione eseguita dal thread
void workerLoop(SelectedDataStructure climb, int repetitions) {
    std::cout << "[Worker Thread] Avviato con ID: " 
              << std::this_thread::get_id() << std::endl;

    for (int i = 1; i <= repetitions; ++i) {
        auto t_start = std::chrono::high_resolution_clock::now();

        double np = computeNormalizedPower(climb);

        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = t_end - t_start;

        std::cout << " Iterazione " << i << "/10 completata in " 
                  << elapsed.count() << " ms | NP calcolata: " << np << " W\n";
    }

    std::cout << "[Worker Thread] Ha completato tutte le 10 iterazioni.\n";
}

int main() {
    SelectedDataStructure testClimb = {"Passo dello Stelvio", 4200, 345.5};

    std::cout << "[Main Thread] Avvio del test con 1 thread e 10 ripetizioni...\n";
    auto total_start = std::chrono::high_resolution_clock::now();

    {
        // Creazione del thread: parte subito ed esegue workerLoop
        std::jthread worker(workerLoop, testClimb, 10);
        
        // Alla chiusura di questa parentesi graffa il distruttore di jthread
        // attende automaticamente il completamento del worker (RAII / join automatico)
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> total_elapsed = total_end - total_start;

    std::cout << "[Main Thread] Tempo totale complessivo: " 
              << total_elapsed.count() / 1000.0 << " secondi.\n";

    return 0;
}