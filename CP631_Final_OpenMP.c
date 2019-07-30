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
**
**********************************************************************************************/

/* In course server, the code can run success fully by the command:
**  gcc -fopenmp -O2 CP631_Final_OpenMP.c -o CP631_Final_OpenMP.x
**
** Then, the code can be run by the command:
**  OMP_NUM_THREADS=24 ./CP631_Final_OpenMP.x
**
** If in the server with small memory space, run the command below to prevent segfaults:
** ulimit -s unlimited
**
**********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <omp.h>
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

/*********************************************************************
** This function is written for inserting the new large distance information to structure
** primeList[].
*********************************************************************/
void InsertRcdTobuff(primeInfo* buff, int* foundPrmInThd, int newDistance, int smallPrime, int largePrime)
{
    int j;

    /* This 'for' loop moves the items for new large distance */
    for (j=*foundPrmInThd; j>=0; --j)
    {
        /* Save the new result to the sorted place */
        /* Note: 6 items are defined in array primeList to avoid overrun */
        if ( (0 == j) || (newDistance <= buff[j - 1].distance))
        {
            buff[j].smallPrime = smallPrime;
            buff[j].largePrime = largePrime;
            buff[j].distance = newDistance;
            break;
        }
        else if (NEEDED_PRIME_NUM != j)
        {
            /* Move the item */
            buff[j].smallPrime = buff[j-1].smallPrime;
            buff[j].largePrime = buff[j-1].largePrime;
            buff[j].distance = buff[j-1].distance;
        }
    }

    if (*foundPrmInThd < NEEDED_PRIME_NUM)
    {
        (*foundPrmInThd)++;
    }
}

int main(int argc, char **argv)
{
    unsigned char* sieve;
    int i;
    int j;
    int numprimes;
    struct timeval  startTime; /* Record the start time */
    struct timeval  currentTime;  /* Record the current time */

    int lastPrime = 2;               /* Record of last prime */
    int currDistance;

    int start, end, numInThd;            /* The start, end and range of thread */
    int buffIndex;
    primeInfo*  threadResult;
    /* Save the found prime in range [2, CPU_CALC_END]. The length is estimated: 1-1/2-1/3 = 1/6 */
    int primeByCPU[CPU_CALC_END/6];
    int foundByCPU = 0;
    int threadResSize;
    int num_thread;

    /* Get number of threads */
#pragma omp parallel
    {
		/* The function omp_get_num_threads() can get correct value in omp mode */
        num_thread = omp_get_num_threads();
	}

    /* Allocate the memory for all threads. */
    sieve = (unsigned char*)malloc(sizeof(unsigned char)* MAX_NUMBER);

    threadResSize = sizeof(primeInfo) * num_thread * (NEEDED_PRIME_NUM+1);
    threadResult = (primeInfo*)malloc(threadResSize);

    if ((NULL == sieve) || (NULL == threadResult))
    {
        if (NULL != sieve)
        {
            free(sieve);
        }

        if (NULL != threadResult)
        {
            free(threadResult);
        }
		
		printf("Failed to allocate the memory.\n");
        return 0;
    }

    memset(threadResult, 0, threadResSize);

    for (i=2; i<MAX_NUMBER; i++)
    {
        sieve[i]=1; //initialize
    }

    gettimeofday(&startTime, NULL);

    /* Find out all the prime number in the range [2, CPU_CALC_END] */
    for (i=2; i<CPU_CALC_END; i++)
    {
        if(0 == sieve[i])
        {
            continue;
        }

        for (j=i+i;j<=CPU_CALC_END;j=j+i)
        {
            sieve[j]=0;
        }

        primeByCPU[foundByCPU++] = i;
    }

#pragma omp parallel firstprivate(i, j, buffIndex, lastPrime, start, end)
    {
        int foundPrimeInThread = 0;
        int ID = omp_get_thread_num();
        primeInfo* threadCurrRes = &threadResult[ID * NEEDED_PRIME_NUM];
        int firstPrimeInthreadc = 0;
        int currentPrime;

        /* All threads will run Seive algorithm for the range [0, 32000] */
        numInThd = (MAX_NUMBER - CPU_CALC_END) / num_thread;
        start = CPU_CALC_END + numInThd*ID;
        end = start+numInThd;

        /* Let's cover all the range. Special handle for first and last process */
        if (ID == (num_thread-1))
        {
            end = MAX_NUMBER;
        }

printf("foundByCPU(%d), start(%d), end(%d),ID(%d)!\n", foundByCPU, start, end, ID);
        /* The following loop will run sieve algorithm for the thread range   */
        for (i=0; i<foundByCPU; i++)
        {
            currentPrime = primeByCPU[i];
			/* The following line is important setting. Don't change it unless you are sure */
			j = (start + currentPrime - 1) / currentPrime * currentPrime;

              /* The sieve algorithm is done for the thread range [start, end] */
            for (; j<=end; j+=currentPrime)
            {
                /* This area is different. Every thread has it's own data. */
                sieve[j]=0;
            }
        }

        if (0 == ID)
        {
            /* Only the first thread in first process keep the lastPrime value */
            start = 2;
        }

        /* Find out all the prime numbers and save the largest distance in array */
        for (i=start; i<end; i++)
        {
            if(sieve[i]==0)
            {
                continue;
            }

            if (0 == firstPrimeInthreadc)
            {
                firstPrimeInthreadc = i;
                /* Save the first prime in the thread for future use */
                threadResult[num_thread * NEEDED_PRIME_NUM + ID].smallPrime = i;
              printf("firstPrimeInthreadc(%d), ID(%d)!\n", firstPrimeInthreadc, ID);
            }
            else
            {
                currDistance = i - lastPrime;

                if((foundPrimeInThread < NEEDED_PRIME_NUM) || (currDistance > threadCurrRes[foundPrimeInThread -1].distance))
                {
                    InsertRcdTobuff(threadCurrRes, &foundPrimeInThread ,currDistance, lastPrime, i);
                }
            }

            lastPrime = i;
        }

        threadResult[num_thread * NEEDED_PRIME_NUM + ID].largePrime = lastPrime;
    } // end of #pragma

    /* Let's put largest 5 distances to prime buffer */
    for (i=0; i<num_thread * NEEDED_PRIME_NUM; i++)
    {
		//printf("buffer (%d), distance(%d), small prime(%d), large prime(%d) foundPrimeNum(%d)\n", i, threadResult[i].distance, threadResult[i].smallPrime, threadResult[i].largePrime, foundPrimeNum);
        if ((foundPrimeNum < NEEDED_PRIME_NUM) || (threadResult[i].distance > primeList[NEEDED_PRIME_NUM-1].distance))
        {
            InsertLargeDistance(threadResult[i].distance, threadResult[i].smallPrime, threadResult[i].largePrime);
			//printf("buffer (%d), distance(%d), small prime(%d), large prime(%d)\n", foundPrimeNum-1, primeList[foundPrimeNum-1].distance, primeList[foundPrimeNum-1].smallPrime, primeList[foundPrimeNum-1].largePrime);
        }
    }

    /* Handle the border distance between threads */
    for (i=0; i<num_thread-1; i++)
    {
        j = i + num_thread * NEEDED_PRIME_NUM;
        currDistance = threadResult[j+1].smallPrime - threadResult[j].largePrime;
        if (currDistance > primeList[NEEDED_PRIME_NUM-1].distance)
        {
            InsertLargeDistance(currDistance, threadResult[j].largePrime, threadResult[j+1].smallPrime);
        }
    }

    gettimeofday(&currentTime, NULL);

    printf("Now, print the %d biggest distances between two continue prime numbers.\n", NEEDED_PRIME_NUM);
    for(i=0;i<NEEDED_PRIME_NUM;i++)
    {
        printf("Between continue prime number (%d) and (%d), the distance is (%d). \n", primeList[i].smallPrime, primeList[i].largePrime, primeList[i].distance);
    }
    printf ("Total time taken by CPU:  %f seconds\n",
             (double) (currentTime.tv_usec - startTime.tv_usec) / 1000000 +
             (double) (currentTime.tv_sec - startTime.tv_sec));

    free(sieve);
    free(threadResult);

    return 0;
}
