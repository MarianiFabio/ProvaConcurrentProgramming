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


std::mutex coutMutex;

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


class SortedResultMonitor {
    private:
    
    static const int CAPACITY = 30; 
    ClimbResult buffer[CAPACITY];

    int count = 0;                  

    std::mutex mtx;   

    public:

    void addItemSorted(const ClimbResult& item) {
        std::lock_guard<std::mutex> lock(mtx); 

        int i = count - 1; 

        while (i >= 0 && buffer[i].normalizedPower < item.normalizedPower) {
            buffer[i + 1] = buffer[i]; 
            i--;                       
        }


        buffer[i + 1] = item;
        count++; 
    }

    int getCount() {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

    ClimbResult getItem(int index) {
        std::lock_guard<std::mutex> lock(mtx);
        return buffer[index];
    }

};

void writeResultFile(const std::string& outputPath, 
                     SortedResultMonitor& resMon, 
                     const std::vector<int>& processed, 
                     const std::vector<int>& passed) {
    
    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "Errore nella creazione del file di output." << std::endl;
        return;
    }

    // Intestazione tabella
    outFile << std::left 
            << std::setw(5)  << "No." 
            << std::setw(30) << "Segment Name" 
            << std::setw(15) << "Duration (s)" 
            << std::setw(15) << "Avg Watts" 
            << std::setw(15) << "Norm Power" << "\n";
    outFile << std::string(80, '-') << "\n";

    // Righe ordinate
    int totalResults = resMon.getCount();
    for (int i = 0; i < totalResults; i++) {
        auto item = resMon.getItem(i);
        outFile << std::left 
                << std::setw(5)  << (i + 1)
                << std::setw(30) << item.originalData.segmentName
                << std::setw(15) << item.originalData.durationSeconds
                << std::setw(15) << std::fixed << std::setprecision(1) << item.originalData.averageWatts
                << std::setw(15) << std::fixed << std::setprecision(2) << item.normalizedPower 
                << "\n";
    }
    outFile << std::string(80, '-') << "\n\n";

    // Statistiche per ciascun thread richieste dal docente
    for (size_t id = 0; id < processed.size(); id++) {
        outFile << "Thread " << id << ": " 
                << passed[id] << " items passed, " 
                << processed[id] << " total\n";
    }

    outFile.close();
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


void workerTask(int workerId, DataMonitor& dataMonitor, SortedResultMonitor& resultMonitor,
                int& processedCount, int& passedCount) {
    ClimbData item;

    // Continua finché ci sono dati nel monitor[cite: 1, 4]
    while (dataMonitor.removeItem(item)) {
        processedCount++;

        // Singola riga: notifica dell'avvenuto prelievo protetta da lock
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "[WORKER " << workerId << "] Ha prelevato: " << item.segmentName << std::endl;
        }

        // Calcolo CPU-bound fuori dal monitor e senza blocchi[cite: 1, 8]
        double np = computeNormalizedPower(item);

        if (np >= 300.0) {
            passedCount++;
            ClimbResult resItem{item, np};
            resultMonitor.addItemSorted(resItem);
        }
    }

    // Singola riga finale: notifica che il worker ha terminato il lavoro[cite: 1, 4]
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[WORKER " << workerId << "] Ha finito tutto." << std::endl;
    }
}



int main() {
    DataMonitor dataMonitor;
    SortedResultMonitor resultMonitor;

    std::string filename = "IFU-3_MarianiF_L1_dat_1.json";
    std::vector<ClimbData> inputData = readDataFile(filename);

    const int NUM_WORKERS = 4;
    std::vector<std::jthread> workers;
    std::vector<int> processedCounts(NUM_WORKERS, 0);
    std::vector<int> passedCounts(NUM_WORKERS, 0);
    
    // workers thread
    for (int i = 0; i < NUM_WORKERS; i++) {
    workers.push_back(std::jthread(
        workerTask, i, std::ref(dataMonitor), std::ref(resultMonitor),
        std::ref(processedCounts[i]), std::ref(passedCounts[i])
    ));
    }

    // main thread
    std::cout << "[MAIN] Avvio inserimento dati nel DataMonitor..." << std::endl;

    for (size_t i = 0; i < inputData.size(); ++i) {
    dataMonitor.addItem(inputData[i]);
    
    // Stampa sincronizzata
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[MAIN] Inserito elemento " << (i + 1) << "/" << inputData.size() 
                  << ": " << inputData[i].segmentName << std::endl;
    }
    }

{
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "[MAIN] Tutti gli elementi inseriti. Chiamata a setFinished()." << std::endl;
}
dataMonitor.setFinished();


    for (int i = 0; i < NUM_WORKERS; i++) {
    if (workers[i].joinable()) {
        workers[i].join();
    }
    }

writeResultFile("IFU-3_MarianiF_L1_rez.txt", resultMonitor, processedCounts, passedCounts);

    std::cout << "Elaborazione completata. File generato." << std::endl;
    

    return 0;
} 
