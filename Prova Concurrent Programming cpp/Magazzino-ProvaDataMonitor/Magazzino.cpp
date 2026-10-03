#include <iostream>
#include <string>
#include <thread>
#include <mutex>

class MailBox {
private:
    int pacco = -1; // -1 indica vuoto
    std::mutex mtx;
    std::condition_variable cv;

public:
    void deposita(int id) {
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
    }
};


int main() {

MailBox mailbox;

    {
    std::jthread produttore([&mailbox] {
        for (int i = 0; i < 5; ++i)
            mailbox.deposita(i);
    });

    std::jthread consumatore([&mailbox] {
        for (int i = 0; i < 5; ++i)
            mailbox.ritira();
    });
    }// Qui i distruttori attendono la fine dei thread

    std::cout << "Fine del main" << std::endl;

  return 0;
} 