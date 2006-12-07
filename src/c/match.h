/*****************************************************************************
 * \file score_peptide_spectrum
 * AUTHOR: Chris Park
 * CREATE DATE: 10/13 2006
 * DESCRIPTION: Object for given a peptide and a spectrum, generate a perliminary score(ex, Sp)
 *
 * REVISION: 
 ****************************************************************************/
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include "carp.h"
#include "parse_arguments.h"
#include "spectrum.h"
#include "spectrum_collection.h"
#include "ion.h"
#include "ion_series.h"
#include "crux-utils.h"
#include "objects.h"
#include "parameter.h"
#include "scorer.h"

/**
 * \returns a new memory allocated match
 */
MATCH_T* new_match(void);


/**
 * free the memory allocated match
 * must provide the match iterator to use the correct method to free peptide contained in match
 */
void free_match(
  MATCH_T* match, ///< the match to free -in
  MATCH_ITERATOR_T* match_iterator ///< the working match iterator -in
  );

/**
 * create a new memory allocated match iterator
 * creates a new the generate_peptides_iterator inside the match_iterator
 *\returns a new memory allocated match iterator
 */
MATCH_ITERATOR_T* new_match_iterator(
  SPECTRUM_T* spectrum ///< the spectrum to match peptides -in
  )

/**
 * sets the match_iterator mode.
 * If not already scored in iterator, creates the scorer for the correct mode and claculates the scores for,
 * each match object, then sort the match structs so that return in decreasing score order.
 * MUST, use this method to set match_iterator ready for a given score type before iterating through scores
 * \returns TRUE if match iterator is successfully set to the correct mode
 */
BOOLEAN_T set_mode_match_iterator(
  MATCH_ITERATOR_T* match_iterator, ///< the match iterator to set -out
  SCORE_TYPE_T match_mode ///< the mode to set (MATCH_SP, MATCH_XCORR) -in
  );

/**
 * Does the match_iterator have another match struct to return?
 * MUST set the iterator to correct mode before calling this method
 *\returns TRUE, if match iter has a next match, else False
 */
BOOLEAN_T mode_match_iterator_has_next(
  MATCH_ITERATOR_T* match_iterator, ///< the working  match iterator -in
  SCORE_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
  );

/**
 * return the next match struct!
 * MUST set the iterator to correct mode before initialially calling this method
 *\returns the match in decreasing score order for the match_mode(SCORER_TYPE_T)
 */
MATCH_T* mode_match_iterator_next(
  MATCH_ITERATOR_T* match_iterator, ///< the working match iterator -in
  SCORE_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
  );

/**
 * free the memory allocated iterator
 */
void free_match_iterator(
  MATCH_ITERATOR_T* match_iterator ///< the match iterator to free
  );

/**
 * print the information of the match
 */
void print_match(
  MATCH_T* match, ///< the match to print -in  
  FILE* file, ///< output stream -out
  BOOLEAN_T output_sequence, ///< should I output peptide sequence -in
  SCORE_TYPE_T output_mode  ///< the output mode -in
);

