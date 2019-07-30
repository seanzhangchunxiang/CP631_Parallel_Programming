/**********************************************************************************************
**  This program uses Sieve of Eratosthenes algorithm to find out the 5 biggest distances of
**  the consecutive prime numbers in range [0, 1000000000].
**
**  It is possible that multiple threads will modify same entry of sieve array, however, that
**  is fine for this algorithm (as all modifications will do same thing causing no errors).
**
**  This file was written by Chunxiang Zhang and Vishnu Sukumaran for CP631 course project
**  of computer science master program in Wilfrid Laurier University in July 2019.
**
**  v1.0   Chunxiang Zhang & Vishnu Sukumaran
**  v1.1   Chunxiang Zhang
**
**********************************************************************************************/

/* In course server, the code can run success fully by the command:
**  mpicc -O2 CP631_Final_MPI.c -o CP631_Final_MPI.x
**
** Then, the code can be run by the command:
**  mpirun -np 24 ./CP631_Final_MPI.x
**
** If in the server with small memory space, run the command below to prevent segfaults:
** ulimit -s unlimited
**********************************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include "mpi.h"
#include <sys/time.h>


/********************************************************************/
/***                                      local definition                                                 ******/
/********************************************************************/
#define    MAX_NUMBER            (1000000000)
#define    NEEDED_PRIME_NUM      (5)


/* Make sure the following definition satisfy the condition:
** (CPU_CALC_END * CPU_CALC_END) > MAX_NUMBER
** CPU runs the sieve arithmetic for the range [2, CPU_CALC_END)
** and GPU runs the remain part [CPU_CALC_END, MAX_NUMBER]    */
#define    CPU_CALC_END          (32000)

typedef struct
{
    int smallPrime;
    int largePrime;
    int distance;
} primeInfo;


/********************************************************************/
/***                                Static Databases/Variables                                       *****/
/********************************************************************/
/* The biggest 5 distances between continuous prime number in sorted list.
** The largest distance will be saved at the first one primeList[0].
** Here, 6 items are defined for simplify the calculation in loop and the sixth is also be
** used to exchange data between processes.  */
primeInfo primeList[NEEDED_PRIME_NUM+1];
int foundPrimeNum =0;        /* The number of found prime number. Range: 0 ~ 5 */

/*********************************************************************
** This function is written for inserting the new large distance information to structure
** primeList[].
*********************************************************************/
void InsertLargeDistance(int newDistance, int smallPrime, int largePrime)
{
    int j;

    /* This 'for' loop moves the items for new large distance */
    for (j=foundPrimeNum; j>=0; --j)
    {
        /* Save the new result to the sorted place */
        /* Note: 6 items are defined in array primeList to avoid overrun */
        if ( (0 == j) || (newDistance <= primeList[j - 1].distance))
        {
            primeList[j].smallPrime = smallPrime;
            primeList[j].largePrime = largePrime;
            primeList[j].distance = newDistance;
            break;
        }
        else if (NEEDED_PRIME_NUM != j)
        {
            /* Move the item */
            primeList[j].smallPrime = primeList[j-1].smallPrime;
            primeList[j].largePrime = primeList[j-1].largePrime;
            primeList[j].distance = primeList[j-1].distance;
        }
    }

    if (foundPrimeNum < NEEDED_PRIME_NUM)
    {
        foundPrimeNum++;
    }
}

/*********************************************************************
** This function is written for deleting the largest distance information from structure
** primeList[].
*********************************************************************/
void DeleteLargestDistance()
{
    int j;

    if (foundPrimeNum > 0)
    {
        foundPrimeNum--;

        /* Delete the largest one, then move other items */
        for (j = 0; j < foundPrimeNum; ++j)
        {
            primeList[j].smallPrime = primeList[j+1].smallPrime;
            primeList[j].largePrime = primeList[j+1].largePrime;
            primeList[j].distance = primeList[j+1].distance;
        }

        /* Clear the old data */
        primeList[foundPrimeNum].smallPrime = 0;
        primeList[foundPrimeNum].largePrime = 0;
        primeList[foundPrimeNum].distance = 0;
    }
}

int main(int argc, char **argv)
{
    int DIM=MAX_NUMBER;
    unsigned char* sieve;
    int i;
    int j;
    int numprimes;
    struct timeval  startTime; /* Record the start time */
    struct timeval  currentTime;  /* Record the current time */

    int lastPrime = 2;               /* Record of last prime */
    int currDistance;

    int my_rank;
	int rank_has_largest;
    int num_processors;
    int start, end, numInProc;
    int memError = 0;
    int allMemError = 0;
    int buffIndex;
    int firstPrimeInProc;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processors);

    if (1 == num_processors)
    {
        printf("This program needs to be run with multiple processes. \n");
        MPI_Finalize();
        return 0;
    }

    numInProc = (MAX_NUMBER - CPU_CALC_END)/num_processors;

    /* All processes will run Seive algorithm for the range [0, 32000] */
    start = CPU_CALC_END + numInProc*my_rank;
    end = start+numInProc;

    /* Let's cover all the range. Special handle for first and last process */
    if (my_rank == (num_processors-1))
    {
        end = MAX_NUMBER;
    }

    /* All processes handle the same numbers except for the first and last one
    ** There are two parts  in the memory:
    ** [0, CPU_CALC_END] for common base prime numbers
    ** [CPU_CALC_END, MAX_NUMBER] for that process area                    */
    numInProc = CPU_CALC_END  + end - start + 1;

    /* Now, the memory needs to be allocated in every process. */
    sieve = (unsigned char*)malloc(sizeof(unsigned char)*(numInProc));

    if (NULL == sieve)
    {
        memError = 1;
    }
    MPI_Allreduce(&memError, &allMemError, 1, MPI_INT,  MPI_SUM, MPI_COMM_WORLD);

    /* If one process fails to allocate the memory, all the process should free the allocated
    ** memory and quit the program. The process 0 needs to print the errors
    ** message before exiting the program. */
    if (0 != allMemError)
    {
        if (NULL != sieve)
        {
            free(sieve);
        }

        MPI_Finalize();

        if(0 == my_rank)
        {
            printf("Failed to allocate the memory!\n");
        }
        return 0;
    }

    for (i=2; i<numInProc; i++)
    {
        sieve[i]=1; //initialize
    }

    if (0 == my_rank)
    {
        gettimeofday(&startTime, NULL);
    }

     /* CPU_CALC_END*CPU_CALC_END is larger than the maximum value,
    ** so that it's enough to CPU_CALC_END in the following algorithm   */
    for (i=2; i<numInProc; i++)
    {
        if(sieve[i]==0)
        {
            continue;
        }

        if (i <= CPU_CALC_END)
        {
            /* The sieve algorithm is done for two parts: [2, CPU_CALC_END] and [start, end] */
            for (j=i+i; j<=end; j=j+i)
            {
                /* Enter 'if' when handling with the process's own range [start, end] */
                if ((CPU_CALC_END < j) && (j < start))
                {
                    /* Skip other processes' parts and enter its own range */
                    j = (start / i)*i;

                    if (j < start)
                    {
                        j+=i;
                    }
                }

                if (j > CPU_CALC_END)
                {
                    /* This area is different. Every process has it's own data. */
                    buffIndex = j - start + CPU_CALC_END;
                }
                else
                {
                    /* This is common area. All the processes have the same result. */
                    buffIndex = j;
                }
                sieve[buffIndex]=0;
            }
		}

        /* Note: the variables 'i' and 'lastPrime' are all subtracted CPU_CALC_END,
        ** but the distance is correct.                                                            */
        currDistance = i - lastPrime;

        /* The cross border case */
        if ((i>CPU_CALC_END) && (lastPrime <= CPU_CALC_END))
        {
            /* The process 0 will continue calculate the distance, while other processes write down first prime */
            if (0 != my_rank)
            {
                firstPrimeInProc = i;
				lastPrime = i;
                currDistance = 0;
				printf("Process %d found first prime %d\n", my_rank, firstPrimeInProc);
				continue;
            }
        }
        /* The current distance is larger than the smallest record distance. Save it. */
        if ((foundPrimeNum < NEEDED_PRIME_NUM) || (currDistance >= primeList[foundPrimeNum-1].distance))
        {
			if (lastPrime <= CPU_CALC_END)
			{
				/* Only first process handle the range [0, CPU_CALC_END] */
				if  (0 == my_rank)
				{
                    InsertLargeDistance(currDistance, lastPrime, i);
				}
			}
			else
			{
				InsertLargeDistance(currDistance, lastPrime-CPU_CALC_END+start, i-CPU_CALC_END+start);
			}
        }
        lastPrime = i;
    }
    /* So far, all distances inside the range have been found out. Let's find the distance
    ** between the range in different processes . All processes except for first process
    ** send the first prime number to previous process. */
    if(0 == my_rank)
    {
        // Process 0 only receive the prime from process 1
        MPI_Recv(&primeList[NEEDED_PRIME_NUM].largePrime, 1, MPI_INT, my_rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else if (my_rank == (num_processors-1))
    {
        MPI_Send(&firstPrimeInProc, 1, MPI_INT, my_rank-1, 0, MPI_COMM_WORLD);
    }
    else
    {
        if (0 == (my_rank%2))
        {
            MPI_Recv(&primeList[NEEDED_PRIME_NUM].largePrime, 1, MPI_INT, my_rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(&firstPrimeInProc, 1, MPI_INT, my_rank-1, 0, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Send(&firstPrimeInProc, 1, MPI_INT, my_rank-1, 0, MPI_COMM_WORLD);
            MPI_Recv(&primeList[NEEDED_PRIME_NUM].largePrime, 1, MPI_INT, my_rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
	
    /* The last process doesn't need to calculate the cross border distance */
    if (my_rank < (num_processors-1))
    {
        currDistance = primeList[NEEDED_PRIME_NUM].largePrime - lastPrime;
        /* The current distance is larger than the smallest record distance. Save it. */
        if (currDistance >= primeList[foundPrimeNum].distance)
        {
            InsertLargeDistance(currDistance, lastPrime, primeList[NEEDED_PRIME_NUM].largePrime);
        }
    }

    for(i=0;i<NEEDED_PRIME_NUM;i++)
    {
        j = 0;
        MPI_Allreduce(&primeList[i].distance, &primeList[NEEDED_PRIME_NUM].distance, 1, MPI_INT,  MPI_MAX, MPI_COMM_WORLD);
		
		if (primeList[i].distance == primeList[NEEDED_PRIME_NUM].distance)
		{
			/* Current process has the largest value */
		    MPI_Allreduce(&my_rank, &rank_has_largest, 1, MPI_INT,  MPI_MAX, MPI_COMM_WORLD);
			printf("Largest distance in %d process\n", my_rank);
		}
		else
		{
			MPI_Allreduce(&j, &rank_has_largest, 1, MPI_INT,  MPI_MAX, MPI_COMM_WORLD);
		}

        if (my_rank == rank_has_largest)
		{
			/* Current process has the largest value */
			primeList[NEEDED_PRIME_NUM].smallPrime = primeList[i].smallPrime;
			primeList[NEEDED_PRIME_NUM].largePrime = primeList[i].largePrime;
            MPI_Bcast(&primeList[NEEDED_PRIME_NUM].smallPrime, 1, MPI_INT, rank_has_largest, MPI_COMM_WORLD);
		    MPI_Bcast(&primeList[NEEDED_PRIME_NUM].largePrime, 1, MPI_INT, rank_has_largest, MPI_COMM_WORLD);
		}
		else
		{
            MPI_Bcast(&primeList[NEEDED_PRIME_NUM].smallPrime, 1, MPI_INT, rank_has_largest, MPI_COMM_WORLD);
		    MPI_Bcast(&primeList[NEEDED_PRIME_NUM].largePrime, 1, MPI_INT, rank_has_largest, MPI_COMM_WORLD);
		    InsertLargeDistance(primeList[NEEDED_PRIME_NUM].distance, primeList[NEEDED_PRIME_NUM].smallPrime, primeList[NEEDED_PRIME_NUM].largePrime);
		}
    }

    /* Process 0 print out the information */
    if (0 == my_rank)
    {
        gettimeofday(&currentTime, NULL);

        printf("Now, print the 5 biggest distances between two continue prime numbers.\n");
        for(i=0;i<NEEDED_PRIME_NUM;i++)
        {
            printf("Between continue prime number (%d) and (%d), the distance is (%d). \n", primeList[i].smallPrime, primeList[i].largePrime, primeList[i].distance);
        }
        printf ("Total time taken by CPU:  %f seconds\n",
                 (double) (currentTime.tv_usec - startTime.tv_usec) / 1000000 +
                 (double) (currentTime.tv_sec - startTime.tv_sec));
    }
    free(sieve);
    /* Finalize the parallel process */
    MPI_Finalize();
    return 0;
}
