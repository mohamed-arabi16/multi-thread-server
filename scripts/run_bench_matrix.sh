#!/usr/bin/env bash

# Check if bench.sh is executable
if [ ! -x "./bench.sh" ]; then
    echo "Error: ./bench.sh is not executable. Run chmod +x bench.sh"
    exit 1
fi

RESULTS_DIR="results"
RESULTS_FILE="$RESULTS_DIR/results.csv"

# Create results directory if it doesn't exist
mkdir -p "$RESULTS_DIR"

# Write header if file doesn't exist
if [ ! -f "$RESULTS_FILE" ]; then
    echo "policy,threads,queue_size,N,PAR,throughput,avg_latency,min_latency,max_latency,wall_time" > "$RESULTS_FILE"
fi

# Define matrix
POLICIES=("fifo" "sff")
THREAD_COUNTS=(1 2 4 8)
QUEUE_SIZES=(1 4 16 64)

# Define benchmark params
N=2000
PAR=10

URL="http://localhost:8080"
FILES=("small.html" "medium.html" "large.html")

# Function to run experiment
run_experiment() {
    local policy=$1
    local threads=$2
    local qsize=$3

    echo "Running experiment: Policy=$policy, Threads=$threads, QueueSize=$qsize"

    # Start server
    ./server --policy "$policy" -t "$threads" -q "$qsize" > /dev/null 2>&1 &
    SERVER_PID=$!

    # Wait for startup
    sleep 2

    # Run benchmark
    # Capture output to temporary file to parse
    OUTPUT=$(./bench.sh "$N" "$PAR" "$URL" "${FILES[@]}")

    avg_lat=$(echo "$OUTPUT" | grep "Average Latency:" | awk '{print $3}' | sed 's/s//')
    min_lat=$(echo "$OUTPUT" | grep "Min Latency:" | awk '{print $3}' | sed 's/s//')
    max_lat=$(echo "$OUTPUT" | grep "Max Latency:" | awk '{print $3}' | sed 's/s//')
    throughput=$(echo "$OUTPUT" | grep "Throughput:" | awk '{print $2}')
    wall_time=$(echo "$OUTPUT" | grep "Wall Time:" | awk '{print $3}' | sed 's/s//')

    if [ -z "$throughput" ] || [ "$throughput" == "req/sec" ]; then
        throughput="0"
    fi
    if [ -z "$wall_time" ]; then
        wall_time="0"
    fi

    # Record results
    # policy, threads, queue_size, N, PAR, throughput, avg_latency, min_latency, max_latency, wall_time
    echo "$policy,$threads,$qsize,$N,$PAR,$throughput,$avg_lat,$min_lat,$max_lat,$wall_time" >> "$RESULTS_FILE"

    # Stop server
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null

    # Wait a bit to ensure port is freed
    sleep 1
}

# Run matrix
for policy in "${POLICIES[@]}"; do
    for threads in "${THREAD_COUNTS[@]}"; do
        for qsize in "${QUEUE_SIZES[@]}"; do
            run_experiment "$policy" "$threads" "$qsize"
        done
    done
done

echo "Benchmark matrix complete. Results saved to $RESULTS_FILE"
