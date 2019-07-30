#!/bin/bash
#SBATCH --time=00:05:00
#SBATCH --account=mcs
mpirun -np 24 -mca btl ^openib ./CP631_Final_MPI.x > CP631_Final_MPI_test_result.txt