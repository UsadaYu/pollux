#ifndef POLLUX_RATIONAL_H
#define POLLUX_RATIONAL_H

/**
 * @brief Rational number (pair of numerator and
 *  denominator).
 */
typedef struct {
  /* Numerator. */
  int num;
  /* Denominator */
  int den;
} pollux_rational;

#endif  // POLLUX_RATIONAL_H
