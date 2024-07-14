#!/usr/bin/env bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

if [[ $# -ne 1 ]]; then
    echo -e "${RED}Usage: $0 <device>${NC}"
    exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
    echo -e "${RED}This script must be run as root${NC}"
    exit 1
fi

device=$1

total_blocks=$(($(blockdev --getsz "${device}") / 8))

# Avoid last block 
total_blocks=$((total_blocks - 1))

random_data=$(mktemp)
read_data=$(mktemp)

total_bytes=0
total_time=0

cleanup() {
    rm -f "${random_data}" "${read_data}"
    if [[ "${total_time}" -gt 0 ]]; then
        throughput=$((total_bytes * 1000 / total_time)) # B/ms
        throughput_mb=$(echo "scale=2; ${throughput} / 1000" | bc) # MB/s
    else
        throughput_mb=0
    fi
    echo -e "Total bytes processed: ${GREEN}${total_bytes}${NC}"
    echo -e "Average throughput: ${GREEN}${throughput_mb} MB/s${NC}"
    exit
}

# Trap INT and TERM signals to cleanup
trap cleanup INT TERM

while true; do
    start_time=$(date +%s%3N)

    number=$(( RANDOM % total_blocks ))

    dd if=/dev/urandom of="${random_data}" bs=4096 count=1 status=none

    dd if="${random_data}" of="${device}" bs=4096 seek="${number}" count=1 status=none
    sync

    dd if="${device}" of="${read_data}" bs=4096 skip="${number}" count=1 status=none

    if ! cmp -s "${random_data}" "${read_data}"; then
        echo -e "${RED}Data verification failed at block ${number}${NC}"
        cleanup
    fi

    echo -e "Block ${number}: ${GREEN}OK${NC}"

    end_time=$(date +%s%3N)
    duration=$((end_time - start_time))

    total_bytes=$((total_bytes + 4096))
    total_time=$((total_time + duration))
done