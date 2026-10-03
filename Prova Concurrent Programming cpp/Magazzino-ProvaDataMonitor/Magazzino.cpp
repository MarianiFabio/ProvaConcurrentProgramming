#include <iostream>
#include <string>
#include <thread>
#include <mutex>




class MailBox {
private:
    static const int dim = 15;
    int magazzino[dim];
    int count = 0;
    std::mutex mtx;
    std::condition_variable cv;


public:

    void deposita(int idPacco){

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return count < dim; });
        magazzino[count] = idPacco;
        std::cout << "Depositato pacco: " << magazzino[count] << std::endl;
        count++;
        cv.notify_one();
    }

    void ritira(){
        int pacco;
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return count > 0; });
        count--;
        pacco = magazzino[count];
        magazzino[count] = -1;
        std::cout << "Ritirato pacco: " << pacco << std::endl;
        cv.notify_one();
    }









    /* void deposita(int id) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return pacco == -1; }); // aspetta che sia vuoto

        pacco = id;
        std::cout << "Depositato pacco: " << id << std::endl;

        cv.notify_one(); // sveglia chi aspetta di ritirare
    }

    int ritira() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return pacco != -1; }); // aspetta che ci sia un pacco

        int valore = pacco;
        pacco = -1;
        std::cout << "Ritirato pacco: " << valore << std::endl;

        cv.notify_one(); // sveglia chi aspetta di depositare
        return valore;
    } */


};


int main() {

MailBox mailbox;

std::jthread produttore ([&mailbox]{
    for (int i = 0; i < 25; ++i)
            mailbox.deposita(i);
});

std::jthread consumatore([&mailbox] {
        for (int i = 0; i < 15; ++i)
            mailbox.ritira();
});





    /* {
    std::jthread produttore([&mailbox] {
        for (int i = 0; i < 5; ++i)
            mailbox.deposita(i);
    });

    std::jthread consumatore([&mailbox] {
        for (int i = 0; i < 5; ++i)
            mailbox.ritira();
    });
    }// Qui i distruttori attendono la fine dei thread */

    std::cout << "Fine del main" << std::endl;

  return 0;
} 