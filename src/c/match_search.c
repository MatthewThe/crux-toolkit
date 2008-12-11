/**
 * \file match_search.c
 * BASED ON: original_match_search.c
 * DATE: Aug 19, 2008
 * AUTHOR: Barbara Frewen
 * DESCRIPTION: Main file for crux-search-for-matches.  Given an ms2
 * file and a fasta file or index, compare all spectra to peptides in
 * the fasta file/index and return high scoring matches.  Peptides are
 * determined by parameters for length, mass, mass tolerance, cleavages,
 * modifications. Score first by a preliminary method, keep only the
 * top ranking matches, score those with a second method and re-rank
 * by second score.  Output in binary csm file format or text sqt file
 * format. 
 */
/*
 * Here is the outline for how the new search should work

   for each spectrum
     for each charge state
      for each peptide modification
        create a peptide iterator
        for each peptide
         score peptide/spectra
      if passes criteria, print results and move on
      else next peptide modification  
 */
#include "carp.h"
#include "parameter.h"
#include "spectrum_collection.h"
#include "match_collection.h"

#define NUM_SEARCH_OPTIONS 15
#define NUM_SEARCH_ARGS 2
#define PARAM_ESTIMATION_SAMPLE_COUNT 500

/* Private functions */
int prepare_protein_input(char* input_file, 
                          INDEX_T** index, 
                          DATABASE_T** database);
void open_output_files(FILE*** binary_filehandle_array, 
                       FILE** sqt_filehandle,
                       FILE** decoy_sqt_filehandle);
BOOLEAN_T is_search_complete(MATCH_COLLECTION_T* matches, 
                             int mods_per_peptide);

int main(int argc, char** argv){

  /* Verbosity level for set-up/command line reading */
  set_verbosity_level(CARP_ERROR);

  /* Define optional command line arguments */
  int num_options = NUM_SEARCH_OPTIONS;
  char* option_list[NUM_SEARCH_OPTIONS] = {
    "verbosity",
    "parameter-file",
    "overwrite",
    "use-index",
    "prelim-score-type",
    "score-type",
    "compute-p-values",
    "spectrum-min-mass",
    "spectrum-max-mass",
    "spectrum-charge",
    "match-output-folder",
    "output-mode",
    "sqt-output-file",
    "decoy-sqt-output-file",
    "number-decoy-set"
  };

  /* Define required command line arguments */
  int num_arguments = NUM_SEARCH_ARGS;
  char* argument_list[NUM_SEARCH_ARGS] = {"ms2 file", "protein input"};

  /* Initialize parameter.c and set default values*/
  initialize_parameters();

  /* Define optional and required arguments */
  select_cmd_line_options(option_list, num_options);
  select_cmd_line_arguments(argument_list, num_arguments);

  /* Parse the command line, including optional params file
     Includes syntax, type, and bounds checking, dies on error */
  parse_cmd_line_into_params_hash(argc, argv, "crux-search-for-matches");

  /* Set verbosity */
  verbosity = get_int_parameter("verbosity");
  set_verbosity_level(verbosity);

  /* Set seed for random number generation */
  if(strcmp(get_string_parameter_pointer("seed"), "time")== 0){
    time_t seconds; // use current time to seed
    time(&seconds); // Get value from sys clock and set seconds variable.
    srand((unsigned int) seconds); // Convert seconds to a unsigned int
  }
  else{
    srand((unsigned int)atoi(get_string_parameter_pointer("seed")));
  }
  
  carp(CARP_INFO, "Beginning crux-search-for-matches");

  /* Get input: ms2 file */
  char* ms2_file = get_string_parameter_pointer("ms2 file");

  // open ms2 file
  SPECTRUM_COLLECTION_T* spectra = new_spectrum_collection(ms2_file);

  // parse the ms2 file for spectra
  carp(CARP_INFO, "Reading in ms2 file %s", ms2_file);
  if(!parse_spectrum_collection(spectra)){
    carp(CARP_FATAL, "Failed to parse ms2 file: %s", ms2_file);
    free_spectrum_collection(spectra);
    exit(1);
  }
  
  carp(CARP_DEBUG, "There were %i spectra found in the ms2 file",
       get_spectrum_collection_num_spectra(spectra));

  /* Get input: protein file */
  char* input_file = get_string_parameter_pointer("protein input");

  /* Prepare input, fasta or index */
  INDEX_T* index = NULL;
  DATABASE_T* database = NULL;
  int num_proteins = prepare_protein_input(input_file, &index, &database); 

  carp(CARP_DEBUG, "Found %i proteins", num_proteins);
  
  /* Prepare output files */

  FILE** psm_file_array = NULL; //file handle array
  FILE* sqt_file = NULL;
  FILE* decoy_sqt_file  = NULL;

  open_output_files(&psm_file_array, &sqt_file, &decoy_sqt_file);

  //print headers
  serialize_headers(psm_file_array);
  print_sqt_header(sqt_file, "target", num_proteins);
  print_sqt_header(decoy_sqt_file, "decoy", num_proteins);

  /* Perform search: loop over spectra*/

  // create spectrum iterator
  FILTERED_SPECTRUM_CHARGE_ITERATOR_T* spectrum_iterator = 
    new_filtered_spectrum_charge_iterator(spectra);

  /*
  // get search parameters for match_collection
  int max_rank_preliminary = get_int_parameter("max-rank-preliminary");
  SCORER_TYPE_T prelim_score = get_scorer_type_parameter("prelim-score-type");
  SCORER_TYPE_T main_score = get_scorer_type_parameter("score-type");
  */
  BOOLEAN_T compute_pvalues = get_boolean_parameter("compute-p-values");
  int sample_count = (compute_pvalues) ? PARAM_ESTIMATION_SAMPLE_COUNT : 0;

  // flags and counters for loop
  int spectrum_searches_counter = 0; //for psm file header, spec*charges
  int mod_idx = 0;
  int num_decoys = get_int_parameter("number-decoy-set");

  // get list of mods
  PEPTIDE_MOD_T** peptide_mods = NULL;
  int num_peptide_mods = generate_peptide_mod_list( &peptide_mods );
  carp(CARP_DEBUG, "Got %d peptide mods", num_peptide_mods);
  // for estimating params for p-values, randomly select a total of 
  //    sample_count matches, a constant fraction from each peptide mod
  int sample_per_pep_mod = (int) sample_count / num_peptide_mods;

  // for each spectrum
  while(filtered_spectrum_charge_iterator_has_next(spectrum_iterator)){
    int charge = 0;
    SPECTRUM_T* spectrum = 
      filtered_spectrum_charge_iterator_next(spectrum_iterator, &charge);
    double mass = get_spectrum_neutral_mass(spectrum, charge);
    //double mass = get_spectrum_singly_charged_mass(spectrum, charge);

    carp(CARP_DETAILED_INFO, 
         "Searching spectrum number %i, charge %i, search number %i",
         get_spectrum_first_scan(spectrum), charge,
         spectrum_searches_counter+1 );

    // with just the target database decide how many peptide mods to use
    // create an empty match collection 
    MATCH_COLLECTION_T* match_collection = 
      new_empty_match_collection( FALSE ); // is decoy

    // assess scores after all pmods with x amods have been searched
    int cur_aa_mods = 0;

    // for each peptide mod
    for(mod_idx=0; mod_idx<num_peptide_mods; mod_idx++){
      // get peptide mod
      PEPTIDE_MOD_T* peptide_mod = peptide_mods[mod_idx];

      // is it time to assess matches?
      int this_aa_mods = peptide_mod_get_num_aa_mods(peptide_mod);
      if( this_aa_mods > cur_aa_mods ){
        carp(CARP_DEBUG, "Finished searching %i mods", cur_aa_mods);
        BOOLEAN_T passes = is_search_complete(match_collection, cur_aa_mods);
        if( passes ){
          carp(CARP_DETAILED_DEBUG, 
               "Ending search with %i modifications per peptide", cur_aa_mods);
          break;
        }// else, search with more mods
        cur_aa_mods = this_aa_mods;
      }

      // get peptide iterator
      MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator = 
        new_modified_peptides_iterator_from_mass(mass,
                                                 peptide_mod,
                                                 index,
                                                 database);
      // score peptides
      int added = add_matches(match_collection, spectrum, 
                              charge, peptide_iterator,
                              sample_per_pep_mod );
      carp(CARP_DEBUG, "Added %i matches", added);

      free_modified_peptides_iterator(peptide_iterator);

    }//next peptide mod

    // in case we searched all mods, do we need to assess again?

    // are there any matches?
    if( get_match_collection_match_total(match_collection) == 0 ){
      // don't print and don't search decoys
      carp(CARP_WARNING, "No matches found for spectrum %i, charge %i",
           get_spectrum_first_scan(spectrum), charge);
      free_match_collection(match_collection);
      continue; // next spectrum
    }
    
    // calculate p-values
    if( compute_pvalues == TRUE ){
      carp(CARP_DEBUG, "Estimating Weibull parameters.");
      int estimate_sample_size = 500;  // get this from ??
      estimate_weibull_parameters(match_collection, 
                                  XCORR, // should be parameter "score-type"
                                  estimate_sample_size, 
                                  spectrum, 
                                  charge);

      carp(CARP_DEBUG, "Calculating p-values.");
      compute_p_values(match_collection);
    }

    // print matches
    carp(CARP_DEBUG, "About to print target matches");
    print_matches(match_collection, spectrum, FALSE,// is decoy
                  psm_file_array[0], sqt_file, decoy_sqt_file);

    fprintf(stderr, "about to free match collections");
    // ?? does this free all the matches, all the spectra and all the peptides?
    free_match_collection(match_collection);

    // now score same number of mods for decoys
    int max_mods = mod_idx;

    // for num_decoys
    int decoy_idx = 0;
    for(decoy_idx = 0; decoy_idx < num_decoys; decoy_idx++ ){
      carp(CARP_DETAILED_DEBUG, "Searching decoy %i", decoy_idx+1);

      // create an empty match collection 
      MATCH_COLLECTION_T* match_collection = 
        new_empty_match_collection( TRUE ); // is decoy

      for(mod_idx = 0; mod_idx < max_mods; mod_idx++){
        /*
        // create an empty match collection 
          MATCH_COLLECTION_T* match_collection = 
          new_empty_match_collection( TRUE ); // is decoy
        */

        // get peptide mod
        PEPTIDE_MOD_T* peptide_mod = peptide_mods[mod_idx];

        // get peptide iterator
        MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator = 
          new_modified_peptides_iterator_from_mass(mass,
                                                   peptide_mod,
                                                   index,
                                                   database);
        // score peptides
        int added = add_matches(match_collection, spectrum, 
                                charge, peptide_iterator,
                                0);// no sampling for param estimation
        carp(CARP_DEBUG, "Added %i matches", added);
        
        free_modified_peptides_iterator(peptide_iterator);
        
      }// last mod

      // print matches
      // only print first decoy to sqt
      FILE* tmp_decoy_sqt_file = decoy_sqt_file;
      if( decoy_idx > 0 ){ tmp_decoy_sqt_file = NULL; }
      carp(CARP_DEBUG, "About to print decoy matches");
      print_matches(match_collection, spectrum, TRUE,// is decoy
                    psm_file_array[1+decoy_idx], sqt_file, tmp_decoy_sqt_file);

      free_match_collection(match_collection);

    }// last decoy

    spectrum_searches_counter++;

    // clean up
    //    free_match_collection(match_collection);
  }// next spectrum

  // fix headers in csm files
  int file_idx;
  for(file_idx=0; file_idx < num_decoys + 1; file_idx++){
    carp(CARP_DEBUG, "Changing csm header to have %i spectrum searches",
         spectrum_searches_counter);
    serialize_total_number_of_spectra(spectrum_searches_counter,
                                      psm_file_array[file_idx]);
  }
  // clean up memory

  carp(CARP_INFO, "Finished crux-search-for-matches");
  exit(0);
}// end main




/* Private function definitions */

int prepare_protein_input(char* input_file, 
                          INDEX_T** index, 
                          DATABASE_T** database){

  int num_proteins = 0;
  BOOLEAN_T use_index = get_boolean_parameter("use-index");

  if (use_index == TRUE){
    carp(CARP_INFO, "Preparing protein index %s", input_file);
    *index = new_index_from_disk(input_file);

    if (index == NULL){
      carp(CARP_FATAL, "Could not create index from disk for %s", input_file);
      exit(1);
    }
    num_proteins = get_index_num_proteins(*index);

  } else {
    carp(CARP_INFO, "Preparing protein fasta file %s", input_file);
    *database = new_database(input_file, FALSE);         
    if( database == NULL ){
      carp(CARP_FATAL, "Could not read fasta file %s", input_file);
      exit(1);
    } 

    parse_database(*database);
    num_proteins = get_database_num_proteins(*database);
  }
  return num_proteins;
}

/**
 * \brief A private function for crux-search-for-matches to prepare
 * binary psm and text sqt files.
 *
 * Reads the --overwrite and --output-mode values from
 * parameter.c. Opens psm file(s) if requested, setting a given
 * pointer to the array of filehandles.  Opens sqt file(s) if
 * requested, setting the given pointers to each file handle.  If
 * binary files not requested, creates an array of NULL pointers.  If
 * sqt files not requested, sets given pointers to NULL. 
 *
 * \returns void.  Sets given arguments to newly created filehandles.
 */
void open_output_files(
  FILE*** psm_file_array, ///< put binary psm filehandles here -out
  FILE** sqt_file,        ///< put text sqt filehandle here -out
  FILE** decoy_sqt_file)  ///< put decoy sqt filehandle here -out
{
  char* match_output_folder = get_string_parameter_pointer(
                                                    "match-output-folder");
  MATCH_SEARCH_OUTPUT_MODE_T output_type = get_output_type_parameter(
                                                    "output-mode");
  BOOLEAN_T overwrite = get_boolean_parameter("overwrite");
  carp(CARP_DEBUG, "The output type is %d (binary, sqt, all)" \
       " and overwrite is '%d'", (int)output_type, (int)overwrite);


  // create binary psm files (allocate memory, even if not used)
  *psm_file_array = create_psm_files();

  if( output_type != BINARY_OUTPUT ){ //ie sqt or all

    //create sqt file handles
    carp(CARP_DEBUG, "Opening sqt files");
    char* sqt_filename = get_string_parameter_pointer("sqt-output-file");
    *sqt_file = create_file_in_path(sqt_filename, 
                                    match_output_folder, 
                                    overwrite);
    char* decoy_sqt_filename = get_string_parameter_pointer(
                                                    "decoy-sqt-output-file");
    if( get_int_parameter("number-decoy-set") > 0 ){
      *decoy_sqt_file = create_file_in_path(decoy_sqt_filename,
                                            match_output_folder,
                                            overwrite);
    }

    if(sqt_file == NULL || decoy_sqt_file == NULL){
      carp(CARP_DEBUG, "sqt file or decoy is null");
    }

  }

  carp(CARP_DEBUG, "Finished opening output files");
}

/**
 * \brief Look at matches and search parameters to determine if a
 * sufficient number PSMs have been found.  Returns TRUE if the
 * maximum number of modifications per peptide have been considered.
 * In the future, implement and option and test for a minimum score.
 * \returns TRUE if no more PSMs need be searched.
 */
BOOLEAN_T is_search_complete(MATCH_COLLECTION_T* matches, 
                             int mods_per_peptide){


  if( matches == NULL ){
    return FALSE;
  }

  // keep searching if no limits on how many mods per peptide
  if( get_int_parameter("max-mods") == MAX_PEPTIDE_LENGTH ){
    return FALSE;
  }
  // stop searching if at max mods per peptide
  if( mods_per_peptide == get_int_parameter("max-mods") ){ 
    return TRUE;
  }

  // test for minimun score found

  return FALSE;
  
}
