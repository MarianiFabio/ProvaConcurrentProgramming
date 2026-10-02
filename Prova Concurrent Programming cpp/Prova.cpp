#include <iostream>
#include <string>
#include <thread>
#include <mutex>

//using namespace std;

std::mutex mtx;

void saluta(std::string nome) {
  for (int i = 0; i < 6; i++) {
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "Ciao, " << nome << "!" << std::endl;
  }
}

int main() {

  std::jthread t1(saluta, "Bob");
  std::jthread t2(saluta, "Alice");
  
std::cout << "Fine del main" << std::endl;

  return 0;
} 