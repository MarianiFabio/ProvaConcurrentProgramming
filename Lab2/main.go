package main

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"sort"
	"time"
)

// ============================================================================
// 0. LOGGING UTILS
// ============================================================================

// Memorizza l'istante esatto di avvio del programma
var startTime = time.Now()

// logMsg stampa un messaggio formattato aggiungendo i millisecondi trascorsi
func logMsg(format string, args ...interface{}) {
	elapsed := time.Since(startTime).Milliseconds()
	// Crea un prefisso fisso di 6 caratteri per i millisecondi per avere stampe allineate
	prefix := fmt.Sprintf("[%6d ms] ", elapsed)
	fmt.Printf(prefix+format+"\n", args...) // Aggiunge \n in automatico
}

// ============================================================================
// 1. STRUTTURE DATI
// ============================================================================

type ClimbData struct {
	SegmentName     string  `json:"segmentName"`
	DurationSeconds int     `json:"durationSeconds"`
	AverageWatts    float64 `json:"averageWatts"`
}

type ClimbResult struct {
	OriginalData    ClimbData
	NormalizedPower float64
}

// Struttura usata dai worker per chiedere un elemento al DataThread (Request-Reply pattern)
type WorkerRequest struct {
	ReplyChan chan ClimbData
}

type WorkerStats struct {
	WorkerID  int
	Processed int
	Passed    int
}

type FinalData struct {
	Results []ClimbResult
	Stats   []WorkerStats
}

// ============================================================================
// 2. LOGICA SEQUENZIALE (Lettura, Scrittura, Calcolo)
// ============================================================================

func readDataFile(filePath string) []ClimbData {
	// os.ReadFile apre, legge tutto e chiude il file in automatico
	bytes, err := os.ReadFile(filePath)
	if err != nil {
		fmt.Println("Errore nella lettura del file:", err)
		return nil
	}

	var j map[string][]ClimbData
	err = json.Unmarshal(bytes, &j)
	if err != nil {
		fmt.Println("Errore nel parsing del JSON:", err)
		return nil
	}
	return j["climbs"]
}

func computeNormalizedPower(climb ClimbData) float64 {
	var accumulatedStress float64 = 0.0
	const STEPS = 50000000
	frequency := 1.0 / float64(climb.DurationSeconds)

	for i := 0; i < STEPS; i++ {
		deltaWatts := math.Sin(float64(i)*frequency) * 25.0
		instantaneousWatts := climb.AverageWatts + deltaWatts
		accumulatedStress += math.Pow(instantaneousWatts, 4.0)
	}
	return math.Pow(accumulatedStress/float64(STEPS), 0.25)
}

func writeResultFile(outputPath string, finalData FinalData) {
	file, err := os.Create(outputPath)
	if err != nil {
		fmt.Println("Errore nella creazione del file:", err)
		return
	}
	defer file.Close()

	file.WriteString(fmt.Sprintf("%-5s %-30s %-15s %-15s %-15s\n", "No.", "Segment Name", "Duration (s)", "Avg Watts", "Norm Power"))
	file.WriteString("--------------------------------------------------------------------------------\n")

	for i, item := range finalData.Results {
		file.WriteString(fmt.Sprintf("%-5d %-30s %-15d %-15.1f %-15.2f\n",
			i+1, item.OriginalData.SegmentName, item.OriginalData.DurationSeconds,
			item.OriginalData.AverageWatts, item.NormalizedPower))
	}
	file.WriteString("--------------------------------------------------------------------------------\n\n")

	for _, stat := range finalData.Stats {
		file.WriteString(fmt.Sprintf("Thread %d: %d items passed, %d total\n", stat.WorkerID, stat.Passed, stat.Processed))
	}
}

// ============================================================================
// 3. PROCESSI CONCORRENTI (Memoria Distribuita e Canali Unbuffered)
// ============================================================================

// DataThread: Gestisce l'array isolato bloccando richieste invalide (troppo pieno o troppo vuoto)
func DataThread(insertChan <-chan ClimbData, removeReqChan <-chan WorkerRequest, numWorkers int) {
	const CAPACITY = 15 // Deve essere <= n/2
	buffer := make([]ClimbData, 0, CAPACITY)
	mainFinished := false
	workersToStop := numWorkers

	for {
		if workersToStop == 0 {
			break // Termina quando tutti i worker hanno ricevuto il segnale di stop
		}

		if len(buffer) == 0 {
			if mainFinished {
				// Il Main ha finito e il buffer è vuoto: invia segnale di terminazione ai worker
				req := <-removeReqChan
				req.ReplyChan <- ClimbData{SegmentName: "EOF"}
				workersToStop--
			} else {
				// Il buffer è vuoto, accetta solo messaggi di inserimento
				item, ok := <-insertChan
				if !ok {
					mainFinished = true
					insertChan = nil
				} else {
					buffer = append(buffer, item)
					time.Sleep(1 * time.Millisecond) // Simula la possibilità di riempimento/svuotamento
				}
			}
		} else if len(buffer) == CAPACITY {
			// Il buffer è pieno, accetta solo richieste di rimozione dai worker
			req := <-removeReqChan
			req.ReplyChan <- buffer[0]
			buffer = buffer[1:]
		} else {
			// Il buffer non è né vuoto né pieno: usa un costrutto select per servire il primo che arriva
			select {
			case item, ok := <-insertChan:
				if !ok {
					mainFinished = true
					insertChan = nil // Disabilita questo ramo del select
				} else {
					buffer = append(buffer, item)
				}
			case req := <-removeReqChan:
				req.ReplyChan <- buffer[0]
				buffer = buffer[1:]
			}
		}
	}
}

// ResultThread: Gestisce array locale ordinato e riceve risultati e statistiche
func ResultThread(resultChan <-chan ClimbResult, statsChan <-chan WorkerStats, numWorkers int, mainResultChan chan<- FinalData) {
	var results []ClimbResult
	var stats []WorkerStats
	finishedWorkers := 0

	for {
		if finishedWorkers == numWorkers {
			// ORDINA I RISULTATI ALLA FINE (Decrescente)
			sort.Slice(results, func(i, j int) bool {
				return results[i].NormalizedPower > results[j].NormalizedPower
			})

			// Invia tutto al MainProcess quando tutti i worker hanno terminato
			mainResultChan <- FinalData{Results: results, Stats: stats}
			break
		}

		select {
		case res := <-resultChan:
			// Semplice append, velocissimo e non blocca il canale
			results = append(results, res)

		case stat := <-statsChan:
			stats = append(stats, stat)
			finishedWorkers++
		}
	}
}

// WorkerThread: Processa dati prelevati e manda messaggi calcolati
func WorkerThread(id int, removeReqChan chan<- WorkerRequest, resultChan chan<- ClimbResult, statsChan chan<- WorkerStats) {
	processedCount := 0
	passedCount := 0
	replyChan := make(chan ClimbData) // Canale privato unbuffered di risposta

	for {
		// Invia la richiesta al gestore dati
		removeReqChan <- WorkerRequest{ReplyChan: replyChan}
		item := <-replyChan // Attende la risposta

		if item.SegmentName == "EOF" {
			break // Termina se non ci sono più dati
		}

		processedCount++
		logMsg("[WORKER %d] Took item: %s", id, item.SegmentName)
		np := computeNormalizedPower(item)

		if np >= 300.0 {
			passedCount++
			resultChan <- ClimbResult{OriginalData: item, NormalizedPower: np}
		}
	}

	// Al termine del ciclo, invia le statistiche al gestore dei risultati
	statsChan <- WorkerStats{WorkerID: id, Processed: processedCount, Passed: passedCount}
	logMsg("[WORKER %d] Done.", id)
}

// ============================================================================
// 4. MAIN PROCESS
// ============================================================================

func main() {
	inputData := readDataFile("IFU-3_MarianiF_L1_dat_1.json")
	if inputData == nil {
		return
	}

	const NUM_WORKERS = 4 // Da 2 a n/4

	// Canali Unbuffered per la comunicazione
	insertChan := make(chan ClimbData)
	removeReqChan := make(chan WorkerRequest)
	resultChan := make(chan ClimbResult)
	statsChan := make(chan WorkerStats)
	mainResultChan := make(chan FinalData)

	// Avvia gestori
	go DataThread(insertChan, removeReqChan, NUM_WORKERS)
	go ResultThread(resultChan, statsChan, NUM_WORKERS, mainResultChan)

	// Avvia Workers
	for i := 0; i < NUM_WORKERS; i++ {
		go WorkerThread(i, removeReqChan, resultChan, statsChan)
	}

	logMsg("[MAIN] Starting data insertion...")
	// Il processo principale invia tutti gli elementi uno per uno
	for _, item := range inputData {
		insertChan <- item
		logMsg("[MAIN] Sent item: %s", item.SegmentName)
	}

	// Chiude il canale per segnalare al DataThread che l'inserimento è terminato
	close(insertChan)
	logMsg("[MAIN] All items sent. Waiting for results...")
	// Riceve il consolidamento finale dal ResultThread (Operazione bloccante)
	finalData := <-mainResultChan

	// Stampa su file
	writeResultFile("IFU-3_MarianiF_L2_rez.txt", finalData)
	logMsg("Done. Output file generated.")
}
