//
// File Name : prime.h
// Author    : Yang, Byung-Sun
//
// Description
//      This is to find a prime number for rehashing of a closed hash table.
//
#ifndef __PRIME_H__
#define __PRIME_H__

extern int           num_of_primes;
extern unsigned int  primes[];

inline static
unsigned int
get_next_prime( unsigned int  val )
{
    int    low, high, mid;
    
    mid = 0;
    low = 1;
    high = num_of_primes - 2;

    while ( low <= high ) {
        mid = ( low + high ) / 2;
        if ( primes[ mid ] > val ) {
            high = mid - 1;
        } else if ( primes[ mid ] < val ) {
            low = mid + 1;
        } else {
            return primes[ mid ];
        }
    }

    if ( high <= 1 || low >= num_of_primes - 2 ) {
        return val - 1;
    }

    if ( primes[ mid ] > val ) {
        return ( ( primes[ mid ] - val > val - primes[ mid - 1 ] ) ?
                 primes[ mid - 1 ] : primes[ mid ] );
    } else {
        return ( ( val - primes[ mid ] >= primes[ mid + 1 ] - val ) ?
                 primes[ mid + 1 ] : primes[ mid ] );
    }
}

#endif __PRIME_H__
