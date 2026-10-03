#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>





class MailBox {
private:
    static const int capacita = 15;
    int magazzino[capacita];
    int count = 0;

    int tail = 0;
    int head = 0;

    bool finished = false; // 1. Flag di fine lavoro

    std::mutex mtx;
    std::condition_variable not_full;
    std::condition_variable not_empty;

public:

    void addItem (int item) {
        magazzino[tail] = item;
        tail = (tail + 1) % capacita;
        count++;
    }

    int removeItem () {
        int item = 0;
        item = magazzino[tail];
        head = (head + 1) % capacita;
        count--;
        return item;
    }

    void deposita(int idPacco) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Aspetta se il magazzino è pieno
        not_full.wait(lock, [this]() { return count < dim; });

        magazzino[count] = idPacco;
        std::cout << "Depositato pacco: " << idPacco << " (Presenti: " << count + 1 << ")" << std::endl;
        count++;

        // Sveglia un consumatore in attesa
        not_empty.notify_one();
    }

    // Restituisce true se ha estratto un pacco, false se è finito il lavoro
    bool ritira(int& pacco) {
        std::unique_lock<std::mutex> lock(mtx);

        // Aspetta finché c'è almeno un pacco OPPURE finché non finisce tutto
        not_empty.wait(lock, [this]() { return count > 0 || finished; });

        // Se il magazzino è vuoto e il produttore ha finito: spegni il thread
        if (count == 0 && finished) {
            return false;
        }

        count--;
        pacco = magazzino[count];
        std::cout << "Ritirato pacco: " << pacco << " (Rimasti: " << count << ")" << std::endl;

        // Sveglia il produttore se era fermo per magazzino pieno
        not_full.notify_one();
        return true;
    }

    // Chiamato dal produttore alla fine dell'invio dei dati
    void termina() {
        std::unique_lock<std::mutex> lock(mtx);
        finished = true;
        // Sveglia TUTTI i consumatori per farli uscire puliti
        not_empty.notify_all();
    }
};

int main() {
    MailBox mailbox;

    {
        // Thread Produttore
        std::jthread produttore([&mailbox]() {
            for (int i = 0; i < 25; ++i) {
                mailbox.deposita(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
            }
            // Avvisa che non ci saranno altri pacchi
            mailbox.termina();
        });

        // Thread Consumatore
        std::jthread consumatore([&mailbox]() {
            int paccoEstratto = -1;
            // Continua a consumare finché ritira() restituisce true
            while (mailbox.ritira(paccoEstratto)) {
                // Qui verrebbe svolto il calcolo o l'elaborazione del pacco
                // Simula l'elaborazione del pacco FUORI dal monitor
                std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
}
            std::cout << "Consumatore: lavoro terminato, esco." << std::endl;
        });
    } // I distruttori di jthread attendono il completamento (auto-join)

    std::cout << "Fine del main" << std::endl;
    return 0;
}