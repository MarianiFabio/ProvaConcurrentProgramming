#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include "nlohmann/json.hpp" 

using json = nlohmann::json;

// Dati del ciclismo da prendere da file
struct SelectedDataStructure {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Funzione sequenziale per la lettura del file JSON
std::vector<SelectedDataStructure> readDataFile(const std::string& filePath) {
    std::vector<SelectedDataStructure> climbs;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Errore: impossibile aprire il file " << filePath << std::endl;
        return climbs;
    }

    try {
        json j;
        file >> j;

        for (const auto& item : j["climbs"]) {
            SelectedDataStructure climb;
            climb.segmentName = item.at("segmentName").get<std::string>();
            climb.durationSeconds = item.at("durationSeconds").get<int>();
            climb.averageWatts = item.at("averageWatts").get<double>();
            climbs.push_back(climb);
        }
    } catch (const json::exception& e) {
        std::cerr << "Errore nel parsing JSON: " << e.what() << std::endl;
    }

    return climbs;
}



struct SelectedDataStructureWithComputedValue {
    SelectedDataStructure originalData;
    double normalizedPower;
};

// Calcolo computazionalmente pesante (CPU-bound: 20.000.000 iterazioni)
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

int main() {

    const int NUM_WORKERS = 4;
    std::vector<std::jthread> workers;
    

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        workers.push_back
        std::jthread 
    }
    




    std::string filename = "IFU-3_MarianiF_L1_dat_1.json";
    std::vector<SelectedDataStructure> inputData = readDataFile(filename);

    

    return 0;
}