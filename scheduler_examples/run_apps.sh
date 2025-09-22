#!/bin/bash
# This script runs multiple applications in the background.
# It waits for all of them to finish before exiting.
./app outlook 10 &
./app firefox 15 &
./app powerpoint 20 &
