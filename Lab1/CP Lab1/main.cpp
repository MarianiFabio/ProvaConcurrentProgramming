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

// This program reads cycling climb data from a JSON file,
// computes a heavy CPU-bound value for each segment,
// and then prepares the data for a concurrent pipeline.

// A single piece of data extracted from the JSON file.
struct ClimbData {
    std::string segmentName;
    int durationSeconds;
    double averageWatts;
};

// Read the input file and convert each item into a ClimbData object.
// This part is done sequentially because the file is read only once.
std::vector<ClimbData> readDataFile(const std::string& filePath) {
    std::vector<ClimbData> climbs;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << filePath << std::endl;
        return climbs;
    }

    try {
        json j;
        file >> j;

        // Each element in the "climbs" array represents one segment.
        for (const auto& item : j["climbs"]) {
            ClimbData climb;
            climb.segmentName = item.at("segmentName").get<std::string>();
            climb.durationSeconds = item.at("durationSeconds").get<int>();
            climb.averageWatts = item.at("averageWatts").get<double>();
            climbs.push_back(climb);
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }

    return climbs;
}

// Store the original climb data together with the computed result.
// In other words: "this is the input, and this is the value produced from it".
struct ClimbResult {
    ClimbData originalData;
    double normalizedPower;
};

// This is the expensive calculation.
// It simulates a heavy CPU task by doing many mathematical operations.
// The result is a normalized power value that summarizes the effort.
double computeNormalizedPower(const ClimbData& climb) {
    if (climb.durationSeconds <= 0) {
        return 0.0;
    }

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

// This class acts like a shared buffer used by producer/consumer threads.
// Producers add items to the queue, and consumers remove them later.
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
    // Put a new item into the buffer only when there is free space.
    void addItem(const ClimbData& item) {
        std::unique_lock<std::mutex> lock(mtx);

        not_full.wait(lock, [this]() { return count < CAPACITY; });

        buffer[tail] = item;
        tail = (tail + 1) % CAPACITY;
        count++;

        not_empty.notify_one();
    }

    // Remove one item from the buffer when an element is available.
    bool removeItem() {
        std::unique_lock<std::mutex> lock(mtx);

        not_empty.wait(lock, [this]() { return count > 0 || finished; });

        if (count == 0 && finished) {
            return false;
        }

        head = (head + 1) % CAPACITY;
        count--;

        not_full.notify_one();
        return true;
    }
};

// This monitor is meant to hold sorted results.
// The idea is to collect computed values and order them later.
class SortedResultMonitor {
private:
public:
};

int main() {
    // Number of worker threads we want to create.
    // This is a simple example of parallelism, even if the work is not yet fully implemented.
    const int NUM_WORKERS = 4;
    std::vector<std::jthread> workers;

    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.emplace_back([]() {
            // Each worker can later process one climb or one chunk of climbs.
            // For now, the thread just starts and exits immediately.
        });
    }

    // Load the data file into memory before any heavy computation begins.
    std::string filename = "IFU-3_MarianiF_L1_dat_1.json";
    std::vector<ClimbData> inputData = readDataFile(filename);

    // At this point we have:
    // 1) input data loaded from JSON
    // 2) a class ready to manage shared buffers
    // 3) a place where computed results can be stored

    return 0;
}