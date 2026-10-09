#include <iostream>
#include <omp.h> // L'header obbligatorio di OpenMP

int main() {
    const int N = 30;
    const int NUM_TOTAL_THREADS = 4;

    //auto total_threads = omp_get_num_threads();

    

    const int baseChunkSize = N / NUM_TOTAL_THREADS; // quanti spettano a tutti
    const int remainingElements = N % NUM_TOTAL_THREADS;  // quanti elementi avanzano
    
    std::cout << "[MAIN] Prima della regione parallela (eseguito da 1 thread)\n";

    // Direttiva che crea la squadra di thread
    #pragma omp parallel num_threads(NUM_TOTAL_THREADS)
    {
        // 1. Come fa ogni thread a sapere qual è il suo ID univoco (0, 1, 2, 3)?
        int threadID = omp_get_thread_num();        

        int start, end;

        if (threadID < remainingElements) {
        // I primi 'remainder' thread prendono (base_chunk + 1) elementi
        start = threadID * (baseChunkSize + 1);
        end = start + (baseChunkSize + 1);
    } else {
        // Gli altri thread prendono 'base_chunk' elementi
        start = remainingElements * (baseChunkSize + 1) + (threadID  - remainingElements) * baseChunkSize;
        end = start + baseChunkSize;
    }        
        
        std::cout << "Ciao dal thread " << threadID << " su un totale di " << NUM_TOTAL_THREADS
        << ", il mio start e': " << start << ", mentre il mio end e': " << "\n";
        
    }

    //std::cout << "[MAIN] Dopo la regione parallela (i thread hanno fatto il join!)\n";
    return 0;
}