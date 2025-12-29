#!/usr/bin/env bash

# Usage:
#   ./bench.sh <N> <PAR> <URL> <FILES...>
#
# Example:
#   ./bench.sh 200 20 http://localhost:8080 small.html medium.html large.html
#
# Run once with FIFO server, once with Smallest File First (SFF) server.

if [ $# -lt 4 ]; then
    echo "Usage: $0 <N> <PAR> <URL> <FILES...>"
    exit 1
fi

N=$1
PAR=$2
URL=$3
shift 3
FILES=("$@")

# Extract port from the URL (for display only)
PORT=$(echo "$URL" | sed -E 's|.*:([0-9]+).*|\1|')

TMPSEQ=$(mktemp)
TMPRESULT=$(mktemp)

cleanup() {
    rm -f "$TMPSEQ" "$TMPRESULT" "$TMPRESULT.calc" 2>/dev/null
}
trap cleanup EXIT

now_ms() {
    echo $(( $(date +%s) * 1000 ))
}


### --------------------------------------------------------
### STEP 1: Generate randomized workload
### --------------------------------------------------------
echo "Generating workload of $N requests..."
> "$TMPSEQ"
for ((i=1; i<=N; i++)); do
    pick=$(( RANDOM % ${#FILES[@]} ))
    echo "${FILES[$pick]}" >> "$TMPSEQ"
done

### --------------------------------------------------------
### STEP 2: Run benchmark
### --------------------------------------------------------
echo ""
echo "Testing server at: $URL (port $PORT)"
echo "Parallelism: $PAR"
echo "Starting..."
echo ""

> "$TMPRESULT"

start=$(now_ms)

i=0
while read -r file; do
    curl --max-time 120 --connect-timeout 60 -s -o /dev/null \
        -w "%{time_pretransfer} %{time_starttransfer} %{time_starttransfer}\n" \
        "$URL/$file" >> "$TMPRESULT" &
    i=$((i+1))

    if (( i % PAR == 0 )); then
        wait
    fi
done < "$TMPSEQ"

wait
end=$(now_ms)

wall_ms=$(echo "$end - $start" | bc -l)
wall=$(echo "scale=6; $wall_ms / 1000" | bc)

### --------------------------------------------------------
### STEP 3: Compute metrics
### --------------------------------------------------------

awk '{wait=$2-$1; print wait, $3}' "$TMPRESULT" > "$TMPRESULT.calc"

avg_wait=$( awk '{s+=$1; c++} END{printf("%.6f", s/c)}' "$TMPRESULT.calc" )
avg_lat=$(  awk '{s+=$2; c++} END{printf("%.6f", s/c)}' "$TMPRESULT.calc" )
min_lat=$(  awk 'NR==1{m=$2} $2<m{m=$2} END{printf("%.6f", m)}' "$TMPRESULT.calc" )
max_lat=$(  awk '$2>m{m=$2} END{printf("%.6f", m)}' "$TMPRESULT.calc" )

throughput=$(echo "$N / $wall" | bc -l)

### --------------------------------------------------------
### STEP 4: Print results to screen
### --------------------------------------------------------

echo "============================================================"
echo "                  BENCHMARK RESULTS"
echo "============================================================"
echo "URL:             $URL"
echo "Port:            $PORT"
echo "Requests:        $N"
echo "Parallelism:     $PAR"
echo ""
echo "Average Wait:     ${avg_wait}s"
echo "Average Latency:  ${avg_lat}s"
echo "Min Latency:      ${min_lat}s"
echo "Max Latency:      ${max_lat}s"
echo ""
echo "Throughput:       ${throughput} req/sec"
echo "Wall Time:        ${wall}s"
echo "============================================================"
