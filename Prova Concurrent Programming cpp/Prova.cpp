#include <iostream>
#include <string>
#include <thread>
//using namespace std;

void saluta(std::string nome) {
  for (int i = 0; i < 3; i++) {
    std::cout << "Ciao, " << nome << "!" << std::endl;
  }
}

int main() {

  std::jthread t1(saluta, "Bob");
  //std::jthread t2;
  
std::cout << "Fine del main" << std::endl;

  return 0;
} 