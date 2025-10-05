#!/bin/bash
# This script runs multiple applications in the background.
# It waits for all of them to finish before exiting.
./build/app A 10 &
./build/app B 15 &
./build/app C 20 &
