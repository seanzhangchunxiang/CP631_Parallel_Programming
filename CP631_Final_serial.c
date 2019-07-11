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
**  v1.0   Chunxiang Zhang
**
**********************************************************************************************/

/* In course server, the code can run success fully by the command:
** gcc -O2 CP631_Final_serial.c -o CP631_Final_serial.x
**
** Then, the code can be run by the command:  
**  ./CP631_Final_serial.x
**
** If in the server with small memory space, run the command below to prevent segfaults:
** ulimit -s unlimited
**********************************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include <sys/time.h>


/*********************************************************************************************/
/***                                      local definition                        ************/
/*********************************************************************************************/
#define    MAX_NUMBER            (1000000000)
#define    NEEDED_PRIME_NUM      (5)

typedef struct
{
    int smallPrime;
    int largePrime;
    int distance;
} primeInfo;


/**********************************************************************************************/
/***                                Static Databases/Variables                            *****/
/**********************************************************************************************/


int main()
{
    int DIM=MAX_NUMBER;
    unsigned char* sieve;
    int i;
    int j;
    int numprimes;
    struct timeval  startTime; /* Record the start time */
    struct timeval  currentTime;  /* Record the current time */

    int foundPrimeNum =0;        /* The number of found prime number. Range: 0 ~ 5 */
    int recSmallDist = 0;           /* Smallest distance in the 5 recorded distance */
    int lastPrime = 2;               /* Record of last prime */
    int currDistance;

    /* The biggest 5 distances between continuous prime number in sorted list.
    ** The largest distance will be saved at the first one primeList[0].
    ** Here, 6 items are defined for simplify the calculation in loop.  */
    primeInfo primeList[NEEDED_PRIME_NUM+1];

    sieve = (unsigned char*)malloc(sizeof(unsigned char)*DIM);

    for (i=2; i<DIM; i++)
    {
        sieve[i]=1; //initialize
    }

    gettimeofday(&startTime, NULL);

    for (i=2; i<DIM; i++)
    {
        if(sieve[i]==0)
        {
            continue;
        }

        for (j=i+i;j<DIM;j=j+i)
        {
            sieve[j]=0;
        }

        currDistance = i - lastPrime;

        /* The current distance is larger than the smallest record distance. Save it. */
        if (currDistance >= recSmallDist)
        {
            for (j=foundPrimeNum; j>=0; j--)
            {
                /* Save the new result to the sorted place */
                /* Note: 6 items are defined in array primeList to avoid overrun */
                if ( (0 == j) || (currDistance <= primeList[j - 1].distance))
                {
                    primeList[j].smallPrime = lastPrime;
                    primeList[j].largePrime = i;
                    primeList[j].distance = currDistance;
                    break;
                }
                else
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

            /* Update the current smallest distance */
            recSmallDist = primeList[foundPrimeNum - 1].distance;
        }

        lastPrime = i;
    }

    gettimeofday(&currentTime, NULL);

    printf("Now, print the 5 biggest distances between two continue prime numbers.\n");
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
