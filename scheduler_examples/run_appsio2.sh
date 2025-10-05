#!/bin/bash
# This script runs multiple applications in the background.
# It waits for all of them to finish before exiting.
./build/app-io A-6.csv &
./build/app-io B-6.csv &
./build/app-io C-6.csv &
