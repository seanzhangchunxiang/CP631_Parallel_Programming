#!/bin/bash
#SBATCH --time=00:05:00
#SBATCH --account=mcs
OMP_NUM_THREADS=4 OMP_SCHEDULE=guided OMP_PROC_BIND=true mpirun -np 6 -mca btl ^openib ./CP631_Final_MPI_OpenMP.x > CP631_Final_MPI_OpenMP_test_result.txt
