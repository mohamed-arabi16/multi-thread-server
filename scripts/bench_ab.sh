#!/bin/bash
# Benchmark script for the multi-threaded server
# This script uses ApacheBench (ab) to benchmark the server with different file sizes and concurrency levels

# Adjust the URL and port to match your server settings
SERVER_URL="http://localhost:8080"

# Number of requests
REQUESTS=1000
# Concurrency level
CONCURRENCY=100

# Benchmark small file
ab -n $REQUESTS -c $CONCURRENCY "$SERVER_URL/small.html"

# Benchmark medium file
ab -n $REQUESTS -c $CONCURRENCY "$SERVER_URL/medium.html"

# Benchmark large file
ab -n $REQUESTS -c $CONCURRENCY "$SERVER_URL/large.html"
