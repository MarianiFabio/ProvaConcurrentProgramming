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

// Mutex used to avoid mixed terminal output
std::mutex coutMutex;

// Structure to store raw climb data from the file
struct ClimbData {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Reads data from the input JSON file
std::vector<ClimbData> readDataFile(const std::string& filePath) {
    std::vector<ClimbData> climbs;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filePath << std::endl;
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
        std::cerr << "JSON error: " << e.what() << std::endl;
    }

    return climbs;
}

// Stores the original item plus the computed normalized power
struct ClimbResult {
    ClimbData originalData;
    double normalizedPower;
};

// Heavy calculation running 20 million steps (CPU bound)
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

// Monitor for storing processed results in descending order
class SortedResultMonitor {
private:
    static const int CAPACITY = 30; 
    ClimbResult buffer[CAPACITY];
    int count = 0;                  
    std::mutex mtx;   

public:
    // Inserts an item keeping the array sorted by normalizedPower
    void addItemSorted(const ClimbResult& item) {
        std::lock_guard<std::mutex> lock(mtx); 

        int i = count - 1; 

        // Shift smaller items to the right
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

// Writes the final results and worker stats to the text file
void writeResultFile(const std::string& outputPath, 
                     SortedResultMonitor& resMon, 
                     const std::vector<int>& processed, 
                     const std::vector<int>& passed) {
    
    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: could not create output file." << std::endl;
        return;
    }

    // Header table
    outFile << std::left 
            << std::setw(5)  << "No." 
            << std::setw(30) << "Segment Name" 
            << std::setw(15) << "Duration (s)" 
            << std::setw(15) << "Avg Watts" 
            << std::setw(15) << "Norm Power" << "\n";
    outFile << std::string(80, '-') << "\n";

    // Sorted data rows
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

    // Print how much each worker did
    for (size_t id = 0; id < processed.size(); id++) {
        outFile << "Thread " << id << ": " 
                << passed[id] << " items passed, " 
                << processed[id] << " total\n";
    }

    outFile.close();
}

// Bounded buffer monitor for items waiting to be processed
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
    // Called by the main thread when all items are pushed
    void setFinished() {
        std::unique_lock<std::mutex> lock(mtx);
        finished = true;
        not_empty.notify_all(); // Wake up all waiting workers so they can exit
    }

    // Called by the main thread to add raw data
    void addItem(ClimbData item) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait if buffer is full
        not_full.wait(lock, [this]() { return count < CAPACITY; });

        buffer[tail] = item;
        tail = (tail + 1) % CAPACITY;
        count++;

        not_empty.notify_one();
    }

    // Called by workers to take an item
    bool removeItem(ClimbData& climb) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until there is an item or input is done
        not_empty.wait(lock, [this]() { return count > 0 || finished == true; });

        // If empty and main finished, stop working
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

// Task executed by worker threads
void workerTask(int workerId, DataMonitor& dataMonitor, SortedResultMonitor& resultMonitor,
                int& processedCount, int& passedCount) {
    ClimbData item;

    // Loop until removeItem returns false
    while (dataMonitor.removeItem(item)) {
        processedCount++;

        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "[WORKER " << workerId << "] Took item: " << item.segmentName << std::endl;
        }

        // Heavy math done outside the monitors in parallel
        double np = computeNormalizedPower(item);

        // Filter: keep items above or equal to 300 watts
        if (np >= 300.0) {
            passedCount++;
            ClimbResult resItem{item, np};
            resultMonitor.addItemSorted(resItem);
        }
    }

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[WORKER " << workerId << "] Done." << std::endl;
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
    
    // Spawn worker threads
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers.push_back(std::jthread(
            workerTask, i, std::ref(dataMonitor), std::ref(resultMonitor),
            std::ref(processedCounts[i]), std::ref(passedCounts[i])
        ));
    }

    // Main thread acts as the producer
    std::cout << "[MAIN] Starting data insertion into DataMonitor..." << std::endl;

    for (size_t i = 0; i < inputData.size(); ++i) {
        dataMonitor.addItem(inputData[i]);
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "[MAIN] Inserted item " << (i + 1) << "/" << inputData.size() 
                      << ": " << inputData[i].segmentName << std::endl;
        }
    }

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[MAIN] All items inserted. Calling setFinished()." << std::endl;
    }
    dataMonitor.setFinished();

    // Wait for all workers to complete
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].joinable()) {
            workers[i].join();
        }
    }

    // Save final output file
    writeResultFile("IFU-3_MarianiF_L1_rez.txt", resultMonitor, processedCounts, passedCounts);

    std::cout << "Done. Output file generated." << std::endl;
    return 0;
}