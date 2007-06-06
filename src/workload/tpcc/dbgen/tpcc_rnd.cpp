/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file tpcc_rnd.h
 *
 *  @brief Implementation of random generation functions for TPC-C
 *
 *  @author Ippokratis Pandis (ipandis)
 */


#include <stdlib.h>
#include <stdio.h>                                                
#include <string.h>

#include "workload/tpcc/dbgen/tpcc_conf.h"
#include "workload/tpcc/dbgen/tpcc_misc.h"


static char tbl_cust[CUSTOMERS_PER_DISTRICT];

static char alnum[] =
   "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char *last_name_parts[] =
{
   "BAR",
   "OUGHT",
   "ABLE",
   "PRI",
   "PRES",
   "ESE",
   "ANTI",
   "CALLY",
   "ATION",
   "EING"
};

/** @fn rand_integer
 *
 *  @desc Create a uniform random numeric value of type integer, of random
 *  value between lo and hi. Number is NOT placed in BUFFER, and IS
 *  simply RETURNED.
 */

int rand_integer ( int val_lo, int val_hi ) {
  return((random()%(val_hi-val_lo+1))+val_lo);
}


// fn: seed_1_3000
void seed_1_3000( void ) {
  for (int i = 0; i < CUSTOMERS_PER_DISTRICT; i++) {
    tbl_cust[i] = 0;
  }
}

// fn: random_1_3000
int random_1_3000( void ) {
  static int i;
  static int x;

  x = rand_integer(0, CUSTOMERS_PER_DISTRICT - 1);

  for (i = 0; i < CUSTOMERS_PER_DISTRICT; i++)
  {
    if (tbl_cust[x] == 0)
    {
      tbl_cust[x] = 1;
      return(x+1);
    } else {
      x++;
    }
    if (x == CUSTOMERS_PER_DISTRICT)
      x=0;
  }

  printf("\nfatal error in random_1_3000 \n");
  abort();
}


// fn: initialize_random
void initialize_random(void) {
  int t = current_time();                                     

  srand(t);                                                   
  srandom(t);                                                 
}


/** @fn: create_random_a_string
 *
 *  @brief create a random alphanumeric string, of random length between lo and
 *  hi and place them in designated buffer. Routine returns the actual
 *  length.
 *
 *  @param lo - end of acceptable length range
 *  @param hi - end of acceptable length range
 *  
 *  @output actual length
 *  @output random alphanumeric string
 */

int create_random_a_string( char *out_buffer, int length_lo, int length_hi ) {
  int i, actual_length ;

  actual_length = rand_integer( length_lo, length_hi ) ;

  for (i = 0; i < actual_length; i++ ) {
    out_buffer[i] = alnum[rand_integer( 0, 61 )] ;
  }
  out_buffer[actual_length] = '\0' ;

  return (actual_length);
}


/** @fn: create_random_n_string
 *
 *  @brief:  create a random numeric string, of random length between lo and
 *  hi and place them in designated buffer. Routine returns the actual
 *  length.
 *
 *  @param lo - end of acceptable length range
 *  @param hi - end of acceptable length range
 *
 *  @output actual length
 *  @output random numeric string
 */
int create_random_n_string( char *out_buffer, int length_lo, int length_hi ) {
  int i, actual_length ;

  actual_length = rand_integer( length_lo, length_hi ) ;

  for (i = 0; i < actual_length; i++ )
  {
    out_buffer[i] = (char)rand_integer( 48,57 ) ;
  }
  out_buffer[actual_length] = '\0' ;

  return (actual_length);
}

/** @fn: NUrand_val */
int NUrand_val ( int A, int x, int y, int C ) {
  return((((rand_integer(0,A)|rand_integer(x,y))+C)%(y-x+1))+x);
}

/** @fn: create_a_string_with_original
 * 
 *  @brief: create a random alphanumeric string, of random length between lo and
 *  hi and place them in designated buffer. Routine returns the actual
 *  length. The word "ORIGINAL" is placed at a random location in the buffer at
 *  random, for a given percent of the records. 
 *
 *  @note: Cannot use on strings of length less than 8. Lower limit must be > 8
 */
int create_a_string_with_original( char *out_buffer, int length_lo,
                                     int length_hi, int percent_to_set )
{
  int actual_length, start_pos ;

  actual_length = create_random_a_string( out_buffer, length_lo, length_hi ) ;

  if ( rand_integer( 1, 100 ) <= percent_to_set )
  {
    start_pos = rand_integer( 0, actual_length-8 ) ;
    strncpy(out_buffer+start_pos,"ORIGINAL",8) ;
  }

  return (actual_length);
}


/** @fn: create_random_last_name
 *
 *  @brief: create_random_last_name generates a random number from 0 to 999
 *    inclusive. a random name is generated by associating a random string
 *    with each digit of the generated number. the three strings are
 *    concatenated to generate the name
 */
int create_random_last_name(char *out_buffer, int cust_num) {
 int random_num;

 if (cust_num == 0)
   random_num = NUrand_val( A_C_LAST, 0, 999, C_C_LAST_LOAD );    //@d261133mte
 else
   random_num = cust_num - 1;

 strcpy(out_buffer, last_name_parts[random_num / 100]);
 random_num %= 100;
 strcat(out_buffer, last_name_parts[random_num / 10]);
 random_num %= 10;
 strcat(out_buffer, last_name_parts[random_num]);

 return(strlen(out_buffer));
}
