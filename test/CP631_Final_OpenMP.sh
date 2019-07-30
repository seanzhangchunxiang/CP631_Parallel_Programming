#!/bin/bash
#SBATCH --time=00:05:00
#SBATCH --account=mcs
OMP_NUM_THREADS=24 ./CP631_Final_OpenMP.x > CP631_Final_OpenMP_test_result.txt
