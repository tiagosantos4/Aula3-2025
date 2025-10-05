#!/bin/bash
# This script runs multiple applications in the background.
# It waits for all of them to finish before exiting.
./build/app A 5 &
./build/app B 10 &
./build/app C 4 &
./build/app D 2 &
./build/app E 3 &
./build/app F 15 &