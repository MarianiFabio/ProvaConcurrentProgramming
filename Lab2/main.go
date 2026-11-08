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

// Stores the exact program startup timestamp
var startTime = time.Now()

// logMsg prints a formatted message including the elapsed milliseconds
func logMsg(format string, args ...interface{}) {
	elapsed := time.Since(startTime).Milliseconds()
	// Creates a fixed 6-character prefix for milliseconds to keep output aligned
	prefix := fmt.Sprintf("[%6d ms] ", elapsed)
	fmt.Printf(prefix+format+"\n", args...) // Adds a newline automatically
}

// ============================================================================
// 1. DATA STRUCTURES
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

// Structure used by workers to request an item from the DataThread (Request-Reply pattern)
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
// 2. SEQUENTIAL LOGIC (Read, Write, Compute)
// ============================================================================

func readDataFile(filePath string) []ClimbData {
	// os.ReadFile opens, reads the entire file, and closes it automatically
	bytes, err := os.ReadFile(filePath)
	if err != nil {
		fmt.Println("Error reading file:", err)
		return nil
	}

	var j map[string][]ClimbData
	err = json.Unmarshal(bytes, &j)
	if err != nil {
		fmt.Println("Error parsing JSON:", err)
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
		fmt.Println("Error creating file:", err)
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
// 3. CONCURRENT PROCESSES (Distributed Memory and Unbuffered Channels)
// ============================================================================

// DataThread: manages the isolated buffer while blocking invalid requests (too full or too empty)
func DataThread(insertChan <-chan ClimbData, removeReqChan <-chan WorkerRequest, numWorkers int) {
	const CAPACITY = 15 // Must be <= n/2
	buffer := make([]ClimbData, 0, CAPACITY)
	mainFinished := false
	workersToStop := numWorkers

	for {
		if workersToStop == 0 {
			break // Stops when all workers have received the stop signal
		}

		if len(buffer) == 0 {
			if mainFinished {
				// The main process is done and the buffer is empty: send the termination signal to workers
				req := <-removeReqChan
				req.ReplyChan <- ClimbData{SegmentName: "EOF"}
				workersToStop--
			} else {
				// The buffer is empty, accept only insertion messages
				item, ok := <-insertChan
				if !ok {
					mainFinished = true
					insertChan = nil
				} else {
					buffer = append(buffer, item)
					time.Sleep(1 * time.Millisecond) // Simulates a fill/empty opportunity
				}
			}
		} else if len(buffer) == CAPACITY {
			// The buffer is full, accept only removal requests from workers
			req := <-removeReqChan
			req.ReplyChan <- buffer[0]
			buffer = buffer[1:]
		} else {
			// The buffer is neither empty nor full: use select to serve whichever arrives first
			select {
			case item, ok := <-insertChan:
				if !ok {
					mainFinished = true
					insertChan = nil // Disables this branch of the select
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

// ResultThread: manages the ordered local array and receives results and stats
func ResultThread(resultChan <-chan ClimbResult, statsChan <-chan WorkerStats, numWorkers int, mainResultChan chan<- FinalData) {
	var results []ClimbResult
	var stats []WorkerStats
	finishedWorkers := 0

	for {
		if finishedWorkers == numWorkers {
			// SORT RESULTS AT THE END (descending)
			sort.Slice(results, func(i, j int) bool {
				return results[i].NormalizedPower > results[j].NormalizedPower
			})

			// Send everything to the main process once all workers have finished
			mainResultChan <- FinalData{Results: results, Stats: stats}
			break
		}

		select {
		case res := <-resultChan:
			// Simple append, very fast, and does not block the channel
			results = append(results, res)

		case stat := <-statsChan:
			stats = append(stats, stat)
			finishedWorkers++
		}
	}
}

// WorkerThread: processes data items and sends computed results
func WorkerThread(id int, removeReqChan chan<- WorkerRequest, resultChan chan<- ClimbResult, statsChan chan<- WorkerStats) {
	processedCount := 0
	passedCount := 0
	replyChan := make(chan ClimbData) // Private unbuffered response channel

	for {
		// Send the request to the data manager
		removeReqChan <- WorkerRequest{ReplyChan: replyChan}
		item := <-replyChan // Wait for the response

		if item.SegmentName == "EOF" {
			break // Stop when there is no more data
		}

		processedCount++
		logMsg("[WORKER %d] Took item: %s", id, item.SegmentName)
		np := computeNormalizedPower(item)

		if np >= 300.0 {
			passedCount++
			resultChan <- ClimbResult{OriginalData: item, NormalizedPower: np}
		}
	}

	// At the end of the loop, send statistics to the result manager
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

	const NUM_WORKERS = 4 // From 2 to n/4

	// Unbuffered channels for communication
	insertChan := make(chan ClimbData)
	removeReqChan := make(chan WorkerRequest)
	resultChan := make(chan ClimbResult)
	statsChan := make(chan WorkerStats)
	mainResultChan := make(chan FinalData)

	// Start managers
	go DataThread(insertChan, removeReqChan, NUM_WORKERS)
	go ResultThread(resultChan, statsChan, NUM_WORKERS, mainResultChan)

	// Start workers
	for i := 0; i < NUM_WORKERS; i++ {
		go WorkerThread(i, removeReqChan, resultChan, statsChan)
	}

	logMsg("[MAIN] Starting data insertion...")
	// The main process sends all items one by one
	for _, item := range inputData {
		insertChan <- item
		logMsg("[MAIN] Sent item: %s", item.SegmentName)
	}

	// Close the channel to signal the DataThread that insertion is complete
	close(insertChan)
	logMsg("[MAIN] All items sent. Waiting for results...")
	// Receive the final consolidation from the ResultThread (blocking operation)
	finalData := <-mainResultChan

	// Write to file
	writeResultFile("IFU-3_MarianiF_L2_rez.txt", finalData)
	logMsg("Done. Output file generated.")
}
