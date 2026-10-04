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

// SelectedDataStructure: Cycling Climb Data taken from json file
struct ClimbData {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Sequential file reading
std::vector<ClimbData> readDataFile(const std::string& filePath) {
    std::vector<ClimbData> climbs;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Errore: impossibile aprire il file " << filePath << std::endl;
        return climbs;
    }

    try {
        json j;
        file >> j;

        for (const auto& item : j["climbs"]) {
            ClimbData climb;
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

// Climb Data with Computed Value
struct ClimbResult {
    ClimbData originalData;
    double normalizedPower;
};

// Calcolo computazionalmente pesante (CPU-bound: 20.000.000 iterazioni)
double computeNormalizedPower(const ClimbData& climb) {
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


class DataMonitor {
    private:
    static const int CAPACITY = 15;
    ClimbData buffer[CAPACITY];

    int count = 0;
    int head = 0;
    int tail = 0;

    std::mutex mtx;                         
    std::condition_variable not_full;        
    std::condition_variable not_empty;      

    bool finished = false;

    public:

    void setFinished() {
    std::unique_lock<std::mutex> lock(mtx);
    finished = true;
    not_empty.notify_all();     // wakes up every worker that is on wait
    }

    void addItem(ClimbData item){
        std::unique_lock<std::mutex> lock(mtx);
        
        not_full.wait(lock, [this]() { return count < CAPACITY; });

        buffer[tail] = item;
        tail = (tail + 1) % CAPACITY;
        count++;

        not_empty.notify_one();
    }

    bool removeItem(ClimbData& climb){

        std::unique_lock<std::mutex> lock(mtx);
        
        not_empty.wait(lock, [this]() { return count > 0 || finished == true;});

        if (count == 0 && finished == true) {
            return false;
        }

        climb = buffer[head];
        head = (head + 1) % CAPACITY;
        count--;

        not_full.notify_one();

    return true;
    }


};

class SortedResultMonitor {
    private:
    public:
};

void workerTask(int workerId,
                DataMonitor& dataMonitor,
                SortedResultMonitor& resultMonitor,
                int& processedCount,
                int& matchedCount) {
    
}


/* 
int main() {
    DataMonitor dataMonitor;
    SortedResultMonitor resultMonitor;

    std::string filename = "IFU-3_MarianiF_L1_dat_1.json";
    std::vector<ClimbData> inputData = readDataFile(filename);

    const int NUM_WORKERS = 4;
    std::vector<std::jthread> workers;
    int processedCounts[NUM_WORKERS] = {0};
    int matchedCounts[NUM_WORKERS] = {0};
    

    for (int i = 0; i < NUM_WORKERS; i++) {
    workers.push_back(std::jthread(
        workerTask, 
        i, 
        std::ref(dataMonitor), 
        std::ref(resultMonitor),
        std::ref(processedCounts[i]),
        std::ref(matchedCounts[i])
    ));
    }

    for (size_t i = 0; i < inputData.size(); i++) {
    dataMonitor.addItem(inputData[i]);
    }

    
    dataMonitor.setFinished();

    return 0;
} */

// Mutex solo per non accavallare le stampe su console
std::mutex cout_mtx;

void testWorker(int id, DataMonitor& monitor, int& localCount) {
    ClimbData climb;
    while (monitor.removeItem(climb)) {
        localCount++;
        {
            std::lock_guard<std::mutex> printLock(cout_mtx);
            std::cout << "[Worker " << id << "] Elabora: " << climb.segmentName 
                      << " (" << climb.averageWatts << " W)\n";
        }
    }
    {
        std::lock_guard<std::mutex> printLock(cout_mtx);
        std::cout << ">>> Worker " << id << " ha terminato. Totale presi: " << localCount << "\n";
    }
}

int main() {
    DataMonitor monitor;
    const int NUM_WORKERS = 4;
    std::vector<int> counts(NUM_WORKERS, 0);
    std::vector<std::jthread> workers;

    // 1. Avvio dei 4 worker concorrenti
    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.push_back(std::jthread(testWorker, i, std::ref(monitor), std::ref(counts[i])));
    }

    // 2. Il Main Thread inserisce 30 elementi (il buffer tiene solo 15 posti)
    std::cout << "[Main] Inizio inserimento di 30 salite...\n";
    for (int i = 1; i <= 30; ++i) {
        ClimbData data{"Salita_" + std::to_string(i), 1000 + i * 10, 250.0 + i * 3.0};
        monitor.addItem(data);
    }
    std::cout << "[Main] Inserite tutte le 30 salite. Chiamata a setFinished().\n";

    // 3. Segnala fine dei dati
    monitor.setFinished();

    // 4. I jthread effettuano automaticamente il join all'uscita dello scope
    workers.clear();

    // 5. Verifica quadratura dei conti
    int totaleElaborati = 0;
    for (int i = 0; i < NUM_WORKERS; ++i) {
        totaleElaborati += counts[i];
    }

    std::cout << "\n----------------------------------------\n";
    std::cout << "Elementi totali immessi dal Main: 30\n";
    std::cout << "Elementi totali estratti dai Worker: " << totaleElaborati << "\n";
    if (totaleElaborati == 30) {
        std::cout << "[ESITO TEST: OK] Nessun dato perso, nessun blocco o deadlock!\n";
    } else {
        std::cout << "[ESITO TEST: FALLITO] Errore nel conteggio.\n";
    }

    return 0;
}