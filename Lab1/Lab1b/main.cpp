#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include "include/nlohmann/json.hpp" 
#include <omp.h>


using json = nlohmann::json;

/*
 * - This code process cycling climb segments loaded from a JSON file.
 * - Each climb consists of:
 *     1) segmentName (string)
 *     2) durationSeconds (int)
 *     3) averageWatts (double)
 *
 * Computation & Filter:
 * - For each climb, workers compute "Normalized Power" using a heavy,
 *   CPU-bound loop (50 million iterations simulating variable watt stress).
 * - Filter criterion: only climbs with Normalized Power >= 300.0 W are kept.
 * 
 * 
 *
 * AI USAGE DECLARATION:
 * - Generative AI tools were used EXCLUSIVELY for sequential support tasks:
 *      1) JSON data parsing (readDataFile)
 *      2) Formatted text table export (writeResultFile)
 *      3) Synthetic CPU-bound workload simulation (computeNormalizedPower)
 *      4) Thread-safe console logging utility (logLine)
 * - All concurrent architectures (DataMonitor, SortedResultMonitor, thread
 *  lifecycle, critical sections, and conditional variables) were designed
 *   and implemented without AI
 */

std::chrono::steady_clock::time_point programStart;

// [AI-GENERATED]
// Thread-safe console logger with millisecond timestamps to visualize concurrency
template <typename... Args>
void logLine(const Args&... args) {
    #pragma omp critical(console_log)
    {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - programStart).count();

        std::cout << "[+" << std::setw(6) << std::setfill('0')
                  << elapsed << " ms] " << std::setfill(' ');
        (std::cout << ... << args) << std::endl;
    }
}

// Structure to store raw climb data from the file
struct ClimbData {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// [AI-GENERATED]
// Reads data from the input JSON file into a dynamic vector
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

// [AI-GENERATED]
// Heavy calculation running 50 million steps (CPU bound workload)
double computeNormalizedPower(const ClimbData& climb) {
    double accumulatedStress = 0.0;
    const int STEPS = 50000000;
    double frequency = 1.0 / static_cast<double>(climb.durationSeconds);

    for (int i = 0; i < STEPS; ++i) {
        double deltaWatts = std::sin(i * frequency) * 25.0;
        double instantaneousWatts = climb.averageWatts + deltaWatts;
        accumulatedStress += std::pow(instantaneousWatts, 4.0);
    }
    return std::pow(accumulatedStress / STEPS, 0.25);
}

// [NO-AI]
// Monitor to save results in descending order using OpenMP critical section
class SortedResultMonitor {
private:
    static const int CAPACITY = 30; 
    ClimbResult buffer[CAPACITY];       // Static array for results
    int count = 0;                      // Current number of items

public:
    // Inserts an item keeping the array sorted by normalizedPower
    void addItemSorted(const ClimbResult& item) {
        //Critical section, only one thread enters at a time to prevent race conditions
        #pragma omp critical(sorted_result_monitor)
        {

        int i = count - 1; 

        // Shift smaller items to the right to make room
        while (i >= 0 && buffer[i].normalizedPower < item.normalizedPower) {
            buffer[i + 1] = buffer[i]; 
            i--;                       
        }

        buffer[i + 1] = item;
        count++; 
        }
    }
    // Called sequentially after all threads finish, so no lock needed
    // (const because i need only to read the data and not to modify it)
    int getCount() const {
        return count;
    }
    // Called sequentially to export table rows
    ClimbResult getItem(int index)  const {
        return buffer[index];
    }
};

// [AI-GENERATED]
// Exports sorted results, per-worker statistics, and required total sums
void writeResultFile(const std::string& outputPath, 
                     const SortedResultMonitor& resMon, 
                     const std::vector<int>& processed, 
                     const std::vector<int>& passed,
                     int totalDuration,
                     double totalWatts) {
    
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

    // Per thread summary
    for (size_t id = 0; id < processed.size(); id++) {
        outFile << "Thread " << id << ": " 
                << passed[id] << " items passed, " 
                << processed[id] << " total\n";
    }

    // Appended numerical field sums required by Lab 1b specification
    outFile << "\n" << std::string(80, '-') << "\n";
    outFile << "TOTAL SUMS OF FILTERED RESULTS (Calculated via OpenMP):\n";
    outFile << "Total Duration (int):   " << totalDuration << " seconds\n";
    outFile << "Total Avg Watts (float): " << std::fixed << std::setprecision(2) << totalWatts << " W\n";

    outFile.close();
}

// [NO-AI]
// orker function: divides data equally between threads and processes each slice
void processDataPartition(int threadId, int totalThreads,
                          const std::vector<ClimbData>& inputData,
                          SortedResultMonitor& resultMonitor,
                          int& totalDuration, double& totalWatts,
                          int& processedCount, int& passedCount) {
    int n = inputData.size();
    int baseChunkSize = n / totalThreads;
    int remainingElements = n % totalThreads;

    // Distribute remainder items (+1) to the first few threads so chunks are as equal as possible
    int start, end;
    if (threadId < remainingElements) {
        start = threadId * (baseChunkSize + 1);
        end = start + (baseChunkSize + 1);
    } else {
        start = remainingElements * (baseChunkSize + 1) + (threadId - remainingElements) * baseChunkSize;
        end = start + baseChunkSize;
    }

    logLine("[Thread ", threadId, "] Started range [", start, " to ", end, ")");

    // Loop through this thread's assigned items
    for (int i = start; i < end; ++i) {
        processedCount++;
        ClimbData item = inputData[i];

        // Heavy calculation runs in parallel outside any lock
        double np = computeNormalizedPower(item);

        // Filter
        // Keep only climbs with normalized power >= 300 W
        if (np >= 300.0) {
            passedCount++;
            ClimbResult resItem{item, np};
                // Monitor protects itself internally with #pragma omp critical
                resultMonitor.addItemSorted(resItem);
                
                // Atomic updates: fast hardware increment without full lock overhead
                #pragma omp atomic
                totalDuration += item.durationSeconds;
                #pragma omp atomic
                totalWatts += item.averageWatts;
            
                logLine("[Thread ", threadId, "]   -> Inserted: ", item.segmentName, " (NP: ", np, " W)");
        }
    }
}

// [NO-AI]
int main() {
    #ifdef _OPENMP
    std::cout << "OpenMP abilitato! Versione: " << _OPENMP << std::endl;
#else
    std::cout << "ATTENZIONE: OpenMP NON e' abilitato nel compilatore!" << std::endl;
#endif
    programStart = std::chrono::steady_clock::now();

    SortedResultMonitor resultMonitor;

    std::string filename = "IFU-3_MarianiF_L1_dat_1.json";
    std::vector<ClimbData> inputData = readDataFile(filename);

    const int NUM_WORKERS = 4;
    omp_set_dynamic(0);

    int totalDuration = 0;
    double totalWatts = 0.0;
    std::vector<int> processedCounts(NUM_WORKERS, 0);
    std::vector<int> passedCounts(NUM_WORKERS, 0);

    // OpenMP Fork-Join: creates N threads running the block below
    // default(none) forces explicit declaration of all shared references    
    #pragma omp parallel num_threads(NUM_WORKERS) default(none) \
        shared(inputData, resultMonitor, totalDuration, totalWatts, processedCounts, passedCounts)
    {
        // Each thread gets its ID (0 to N-1) and total thread count
        int threadID = omp_get_thread_num();
        int total = omp_get_num_threads();

        processDataPartition(threadID, total, inputData, resultMonitor, 
                             totalDuration, totalWatts, 
                             processedCounts[threadID], passedCounts[threadID]);
    } // OpenMP barrier: all threads wait and join here before moving on

    // Save table and sums to file
    writeResultFile("IFU-3_MarianiF_L1_rez.txt", resultMonitor, processedCounts, passedCounts, totalDuration, totalWatts);

    logLine("Done. Output file generated.");
    return 0;
}