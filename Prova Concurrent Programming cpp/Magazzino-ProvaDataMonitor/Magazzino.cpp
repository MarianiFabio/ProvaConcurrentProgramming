#include <iostream>
#include <string>
#include <thread>
#include <mutex>


std::mutex mtx;
std::condition_variable cv;
int pacco = -1;

void depostitaPacco(int id) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return pacco == -1; });
    
        pacco = id;
        std::cout << "Pacco " << id << " depositato." << std::endl;
    
    cv.notify_one();
}

void ritiraPacco() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return pacco != -1; });
    if (pacco != -1) {
        std::cout << "Pacco " << pacco << " ritirato." << std::endl;
        pacco = -1;
    } else {
        std::cout << "Nessun pacco da ritirare." << std::endl;
    }
    cv.notify_one();
}

int main() {
    for (int i = 0; i < 5; ++i) {
        depostitaPacco(42);
    std::jthread consumatore(ritiraPacco);
    }
    

    std::cout << "Fine del main" << std::endl;

  return 0;
} 