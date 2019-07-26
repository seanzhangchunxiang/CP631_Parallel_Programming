#include "cuda.h" /* CUDA runtime API */
#include "cstdio"
#include "math.h"
#include <sys/time.h>


/*****************************************************************************/
/***                      local definition                        ************/
/*****************************************************************************/
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

/*****************************************************************************/
/***                    Static Databases/Variables                       *****/
/*****************************************************************************/
/* The biggest NEEDED_PRIME_NUM distances between continuous prime number in
** sorted list. The largest distance will be saved at the first one
** primeList[0]. Here, one more item is defined for simplify the calculation
** in loop.                                                                  */
primeInfo primeList[NEEDED_PRIME_NUM+1];
/* The number of found prime number. Range: 0 ~ NEEDED_PRIME_NUM */
int foundPrimeNum;


/* Function insertNewDistance(int distance, int smallerPrime, int largePrime)
*******************************************************************************
* Function description: getPrimeCUDA() is used to insert the found new large
* distance.
*
* Inputs:
*   distance: the new distance between smaller prime and larger prime
*   smallerPrime: small prime
*   largerPrime: large prime
*
* Output:
*   Save the data to array primeList[] and update variable 'foundPrimeNum'
*
* Return:
*   None
*
******************************************************************************/
void insertNewDistance(int distance, int smallPrime, int largePrime)
{
    int j;

    for (j=foundPrimeNum; j>=0; j--)
    {
        /* Save the new result to the sorted place */
        /* Note: 6 items are defined in array primeList to avoid overrun */
        if ( (0 == j) || (distance <= primeList[j - 1].distance))
        {
            primeList[j].smallPrime = smallPrime;
            primeList[j].largePrime = largePrime;
            primeList[j].distance = distance;
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

    /* Maximum 5 largest distances are kept. Update the number of distances */
    if (foundPrimeNum < NEEDED_PRIME_NUM)
    {
        foundPrimeNum++;
    }
}

/* Function getPrimeCUDA()
*******************************************************************************
* Function description: getPrimeCUDA() is used to find out all the primes.
*
* Inputs:
*   dev: the pointer to whole buffer in device [0, 1000000000]
*   prm: the array where the prime numbers in [2, CPU_CALC_END] found were save.
*   limit: the prime numbers found by CPU (cuda limit)
*
* Output:
*   The result is saved in the memory through pointer dev
*
* Return:
*   None
*
******************************************************************************/
__global__  void getPrimeCUDA(unsigned char* dev, int* prm, int limit)
{
    int j;
    int testPrime;
    int x;

    x = blockIdx.x * blockDim.x + threadIdx.x;

    if (x < limit)
	{
        testPrime = prm[x];
        for (j=testPrime+testPrime; j<MAX_NUMBER; j+=testPrime)
        {
            dev[j]=0;
		}
	}
}


int main()
{
    unsigned char* sieve;
    unsigned char* devA;
    /* Save the found prime and pass them to cuda. The length is estimated: 1-1/2-1/3 = 1/6 */
	int primeByCPU[CPU_CALC_END/6];
    int* devPrimes;

	int foundByCPU = 0;
    int i;
    int j;
    int blockSize, nBlocks;
    int totalSize;
    struct timeval  startTime; /* Record the start time */
    struct timeval  currentTime;  /* Record the current time */

    int recSmallDist = 0;           /* Smallest distance in the 5 recorded distance */
    int lastPrime = 2;               /* Record of last prime */
    int currDistance;

    /* Verify setting for searching primes between [2, MAX_NUMBER]. */
    if ((CPU_CALC_END * CPU_CALC_END) <= MAX_NUMBER)
    {
       printf("The CPU_CALC_END * CPU_CALC_END is too small.\n", CPU_CALC_END);

       j = 2;
       while ((j*j) < MAX_NUMBER)
       {
          j++;
       }
       printf("Please change definition of  from (%d) to value no less than (%d).\n", CPU_CALC_END, j);
       return 0;
    }

    totalSize = sizeof(unsigned char)*MAX_NUMBER;
    sieve = (unsigned char*)malloc(totalSize);

    /* allocate arrays on device */
    cudaMalloc((void **) &devA, totalSize);

    for (i=2; i<MAX_NUMBER; i++)
    {
        sieve[i]=1; //initialize
    }

    gettimeofday(&startTime, NULL);

    for (i=2; i<CPU_CALC_END; i++)
    {
        if(0 == sieve[i])
        {
            continue;
        }

        for (j=i+i;j<CPU_CALC_END;j=j+i)
        {
            sieve[j]=0;
        }

        primeByCPU[foundByCPU++] = i;

        currDistance = i - lastPrime;

        /* The current distance is larger than the smallest record distance. Save it. */
        if (currDistance >= recSmallDist)
        {
            insertNewDistance(currDistance, lastPrime, i);

            /* Update the current smallest distance */
            recSmallDist = primeList[foundPrimeNum - 1].distance;
        }

        lastPrime = i;
    }

    /* allocate arrays on device */
    cudaMalloc((void **) &devPrimes, foundByCPU * sizeof(int));

    /* copy arrays to device memory (synchronous) */
    cudaMemcpy(devA, sieve, totalSize, cudaMemcpyHostToDevice);

    /* copy arrays to device memory (synchronous) */
    cudaMemcpy(devPrimes, primeByCPU, foundByCPU * sizeof(int), cudaMemcpyHostToDevice);
    /* guarantee synchronization */
    cudaDeviceSynchronize();

    blockSize = 512;
    nBlocks = foundByCPU / blockSize;
	
	if (0 !=(foundByCPU % blockSize))
	{
	    nBlocks++;
	}

    /* execute kernel (asynchronous!) */
    getPrimeCUDA<<<nBlocks, blockSize>>>(devA, devPrimes, foundByCPU);

    /* retrieve results from device (synchronous) */
    cudaMemcpy(&sieve[CPU_CALC_END], &devA[CPU_CALC_END], totalSize-CPU_CALC_END, cudaMemcpyDeviceToHost);
	cudaMemcpy(primeByCPU, devPrimes, foundByCPU * sizeof(int), cudaMemcpyDeviceToHost);

    /* guarantee synchronization */
    cudaDeviceSynchronize();
	
    for (i=CPU_CALC_END; i<MAX_NUMBER; i++)
    {
        if(sieve[i]==0)
        {
            continue;
        }

        currDistance = i - lastPrime;

        /* The current distance is larger than the smallest record distance. Save it. */
        if (currDistance >= recSmallDist)
        {
            insertNewDistance(currDistance, lastPrime, i);

            /* Update the current smallest distance */
            recSmallDist = primeList[foundPrimeNum - 1].distance;
        }

        lastPrime = i;
    }

    gettimeofday(&currentTime, NULL);
    printf("Largest prime number is %d. Total prime blocks (%d)\n", lastPrime, nBlocks);
    printf("Now, print the %d biggest distances between two continue prime numbers.\n", NEEDED_PRIME_NUM);
    for(i=0;i<NEEDED_PRIME_NUM;i++)
    {
        printf("Between continue prime number (%d) and (%d), the distance is (%d). \n", primeList[i].smallPrime, primeList[i].largePrime, primeList[i].distance);
    }
    printf ("Total time taken by CPU:  %f seconds\n",
             (double) (currentTime.tv_usec - startTime.tv_usec) / 1000000 +
             (double) (currentTime.tv_sec - startTime.tv_sec));
    free(sieve);

    return 0;
}
