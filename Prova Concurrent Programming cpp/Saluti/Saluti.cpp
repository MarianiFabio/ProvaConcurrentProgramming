#include <iostream>
#include <string>
#include <thread>
#include <mutex>

//using namespace std;

std::mutex mtx;
std::condition_variable cv;
int turno = 1;

void saluta(std::string nome, int mio_turno) {
    for (int i = 0; i < 6; i++) {
        // 1. Prendo il lucchetto
        std::unique_lock<std::mutex> lock(mtx);

        // 2. Controllo: è il mio turno? 
        // Se turno != mio_turno, lascio la chiave ed entro in coma
        cv.wait(lock, [mio_turno]() { return turno == mio_turno; });

        // --- Se arrivo qui, ho la chiave ED È IL MIO TURNO ---
        std::cout << "Ciao, " << nome << "!" << std::endl;

        // 3. Passo il testimone all'altro
        if (turno == 1) {
            turno = 2;
        } else {
            turno = 1;
        }

        // 4. Suono il campanello per svegliare l'altro thread addormentato
        cv.notify_one();
        
        // Alla fine del ciclo for, 'lock' esce di scena e cede la chiave
    }
}

int main() {
  
  std::jthread t1(saluta, "Bob", 1);
  std::jthread t2(saluta, "Alice", 2);
  //std::jthread t3(saluta, "Charlie", 3);

std::cout << "Fine del main" << std::endl;

  return 0;
} 