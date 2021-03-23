#include <stdint.h>
#include "real.h" 

#define f 16384

//convert integer n to fixed point
struct real int_to_real(int n) {
  struct real  new ;
  new.value = n * f ;
  return new ;
}

//convert fixed point x to integer (rounding towards zero)
int real_truncate(struct real x){
  return x.value / f ;
}

//convert fixed point x to integer (rounding to nearest)
int real_round(struct real x){
  int temp;
  if(x.value >= 0) temp = (x.value + f / 2) / f ;
  else temp = (x.value - f / 2) / f ;
  return temp;
}

// returns real x +  real y
struct real add_real_real(struct real x, struct real y){
    x.value += y.value ;
    return x;
}

// returns real x - real y
struct real sub_real_real(struct real x, struct real y){
    x.value -= y.value ;
    return x;
}

// returns real x +  int n
struct real add_real_int(struct real x, int n){
    x.value += (n*f) ;
    return x;
}

// returns real x - int n
struct real sub_real_int(struct real x, int n){
    x.value -= (n*f) ;
    return x;
}

// returns real x * real y
  struct real mul_real_real(struct real x, struct real y ){
  x.value = ((int64_t) x.value ) * y.value / f ;
  return x;
 }

// returns real x * int n
struct real mul_real_int(struct real x, int n){
    x.value *= n ;
    return x ;
}

// returns real x /real y
struct real div_real_real(struct real x, struct real y){
  x.value = ((int64_t) x.value ) * f / y.value ;
  return x;
}

// returns real x / int n
struct real div_real_int(struct real x, int n) {
  x.value = x.value / n ;
  return x;
}
