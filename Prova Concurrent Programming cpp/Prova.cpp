#include <iostream>
#include <string>
#include <thread>
#include <mutex>

//using namespace std;

std::mutex mtx;
std::condition_variable cv;
int turno = 1;

void saluta(std::string nome, int mioTurno) {
  for (int i = 0; i < 6; i++) {

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //std::lock_guard<std::mutex> lock(mtx);

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [mioTurno] { return turno == mioTurno; });
    std::cout << "Ciao, " << nome << "!" << std::endl;
    turno = (turno == 1) ? 2 : 1;
    cv.notify_all();
  }
}

int main() {
  
  std::jthread t1(saluta, "Bob", 1);
  std::jthread t2(saluta, "Alice", 2);
  
std::cout << "Fine del main" << std::endl;

  return 0;
} 