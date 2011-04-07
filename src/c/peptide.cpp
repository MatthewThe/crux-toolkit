/*************************************************************************//**
 * \file peptide.cpp
 * \brief Object for representing a single peptide.
 ****************************************************************************/
#include "peptide.h"
#include <string.h>

#include <set>

#include "MatchFileReader.h"

using namespace std;

/*
  TABLE OF CONTENTS
  Global variables
  Private data types
  Private functions
  Public functions
    Allocators/deallocators
    Getters and Setters
      mass-related
      source-related
      sequence-related
      getters requiring calculation

    Comparisons for sorting // MOVEME
    Printing / parsing      // MOVEME
      text
      binary

    Iterators
      residue iterator
      source iterator

      
 */


/**
 * static global variable
 * determines if the peptide src are created by link lists or array
 * if TRUE, peptides are implented with link list peptide src, else array
 */
static BOOLEAN_T PEPTIDE_SRC_USE_LINK_LIST;

/* Private data types */

/**
 * \struct peptide
 * \brief A subsequence of a protein.
 */
struct peptide {
  unsigned char length; ///< The length of the peptide
  FLOAT_T peptide_mass;   ///< The peptide's mass.
  PEPTIDE_SRC_T* peptide_src; ///< a linklist of peptide_src   
  MODIFIED_AA_T* modified_seq; ///< peptide sequence with modifications
  MODIFIED_AA_T* decoy_modified_seq; ///< randomized peptide sequence
};

// the struct to be printed and read; possible fix for adding a field to the peptide
struct PRINT_PEPTIDE_T {
  unsigned char length; ///< The length of the peptide
  FLOAT_T peptide_mass;   ///< The peptide's mass.
  PEPTIDE_SRC_T* peptide_src; ///< a linklist of peptide_src   
  MODIFIED_AA_T* modified_seq; ///< peptide sequence with modifications
};


/**
 * \struct residue_iterator
 * \brief Object to iterate over the residues in a peptide, starting at the
 * first residue of the peptide, and proceeding in order.
 */
struct residue_iterator {
  PEPTIDE_T*  peptide; ///< The peptide whose residues to iterate over.
  char*   sequence;    ///< The peptide sequence
  int     residue_idx; ///< The index of the current peak
};

/**
 * \struct peptide_src_iterator
 * \brief Object to iterate over the peptide_srcs linklist in a peptide
 */
struct peptide_src_iterator{
  PEPTIDE_T*  peptide; ///< The peptide whose peptide_srcs to iterate over.
  PEPTIDE_SRC_T* current; ///< the current peptide_srcs
};

/* Private functions */

/* Public functions--Allocators/Deallocators */

/**
 * \returns An (empty) peptide object.
 */
PEPTIDE_T* allocate_peptide(void){
  PEPTIDE_T* peptide = (PEPTIDE_T*)mycalloc(1, sizeof(PEPTIDE_T));
  peptide->peptide_src = NULL;
  peptide->modified_seq = NULL;
  peptide->decoy_modified_seq = NULL;
  return peptide;
}

/**
 *\returns the protein struct size, value of sizeof function
 */
int get_peptide_sizeof(){
  return sizeof(PEPTIDE_T);
}

// FIXME association part might be need to change
/**
 * \returns A new peptide object, populated with the user specified parameters.
 */
PEPTIDE_T* new_peptide(
  unsigned char length,     ///< The length of the peptide -in
  FLOAT_T peptide_mass,       ///< The neutral mass of the peptide -in
  Protein* parent_protein, ///< the parent_protein of this peptide -in
  int start_idx ///< the start index of this peptide in the protein sequence -in
  //PEPTIDE_TYPE_T peptide_type ///<  The type of peptides(TRYPTIC, C_TRYPTIC, N_TRYPTIC, NOT_TRYPTIC, ANY_TRYPTIC) -in
  )
{
  PEPTIDE_T* peptide = allocate_peptide();
  set_peptide_length(peptide, length);
  set_peptide_peptide_mass(peptide, peptide_mass);
// FIXME: find the level of digest for this specific protein
  peptide->peptide_src = 
    //    new_peptide_src(peptide_type, parent_protein, start_idx );
    new_peptide_src(NON_SPECIFIC_DIGEST, parent_protein, start_idx );
  peptide->modified_seq = NULL;
  peptide->decoy_modified_seq = NULL;
  
  return peptide;
}
  
/**
 * \brief Allocates a new peptide giving it the values of the source
 * peptide.
 * \returns A newly allocated peptide identical to the source.
 */
PEPTIDE_T* copy_peptide(
  PEPTIDE_T* src ///< source peptide -in
){
 
  if( src == NULL ){
    return NULL;
  }
  PEPTIDE_T* new_peptide = allocate_peptide();
  new_peptide->length = src->length;
  new_peptide->peptide_mass = src->peptide_mass;

  if( PEPTIDE_SRC_USE_LINK_LIST ){
    new_peptide->peptide_src = allocate_peptide_src();
    copy_peptide_src(src->peptide_src, new_peptide->peptide_src);
  }else{ // use array
    // if you don't allocate this correctly, it doesn't get freed correctly
    // first count the number of peptide srcs
    PEPTIDE_SRC_T* cur_src = src->peptide_src;
    int src_count = 0;
    while(cur_src != NULL){
      src_count++;
      cur_src = get_peptide_src_next_association(cur_src);
    }
    new_peptide->peptide_src = new_peptide_src_array(src_count); //alloc mem
    copy_peptide_src_array(src->peptide_src, 
                           new_peptide->peptide_src,
                           src_count);
  }
  if( src->modified_seq == NULL ){
    new_peptide->modified_seq = NULL;
  }else{
    new_peptide->modified_seq = copy_mod_aa_seq(src->modified_seq,src->length);
  }
  if( src->decoy_modified_seq == NULL ){
    new_peptide->decoy_modified_seq = NULL;
  }else{
    new_peptide->decoy_modified_seq = copy_mod_aa_seq(src->decoy_modified_seq,
                                                      src->length);
  }
  //PEPTIDE_SRC_T* new_association;

  //  set_peptide_length(dest, get_peptide_length(src));
  //  set_peptide_peptide_mass(dest, get_peptide_peptide_mass(src));

  // copy all of the peptide_src in the peptide
  //  new_association = allocate_peptide_src();
  //  copy_peptide_src(src->peptide_src, new_association);
  //  set_peptide_peptide_src(dest, new_association);

  return new_peptide;
}

/**
 * Merge two identical peptides, copy all peptide_src into one of the peptide
 * peptide_dest, peptide_bye must have at least one peptide src
 * frees the peptide_bye, once the peptide_src are re-linked to the peptide_dest
 * Assumes that both peptides use linklist implemenation for peptide_src
 * \returns TRUE if merge is successful else FALSE
 */
BOOLEAN_T merge_peptides(
  PEPTIDE_T* peptide_dest, ///< the peptide to merge into  -out
  PEPTIDE_T* peptide_bye ///< the peptide to be merged  -in
  )
{
  PEPTIDE_SRC_T* current_src = peptide_dest->peptide_src;
  PEPTIDE_SRC_T* next_src = get_peptide_src_next_association(current_src);
  
  // does all peptides have at least one peptide_src?
  if(current_src == NULL || peptide_bye->peptide_src == NULL){
    carp(CARP_ERROR, "failed to merge two peptides");
    return FALSE;
  }

  // find the end of the peptide src link list..
  while(next_src != NULL){
    current_src = next_src;
    next_src =  get_peptide_src_next_association(current_src);
  }
  set_peptide_src_next_association(current_src, peptide_bye->peptide_src);
  free(peptide_bye);
  return TRUE;
}

/**
 * Merges two identical peptides by adding the peptide_src of the
 * second to the first.  The second peptide remains unchanged.
 * Does not comfirm identity of peptides.
 * \returns TRUE if merge is successfull.
 */
BOOLEAN_T merge_peptides_copy_src(PEPTIDE_T* peptide_dest,
                                  PEPTIDE_T* peptide_giver){

  if( peptide_dest == NULL || peptide_giver == NULL ){
    carp(CARP_FATAL, "Cannot merge NULL peptides.");
  }

  // find the last peptide src for destination
  PEPTIDE_SRC_T* dest_src = peptide_dest->peptide_src;
  PEPTIDE_SRC_T* dest_next = get_peptide_src_next_association(dest_src);

  while( dest_next != NULL ){
    dest_src = dest_next;
    dest_next = get_peptide_src_next_association(dest_src);
  }

  // copy the giver peptide_src's to the dest (allocate first src)
  PEPTIDE_SRC_T* temp_src = allocate_peptide_src();
  PEPTIDE_SRC_T* giver_src = peptide_giver->peptide_src;

  copy_peptide_src(giver_src, temp_src);
  set_peptide_src_next_association(dest_src, temp_src);

  return TRUE;
}

/**
 * Frees an allocated peptide object.
 * Depending on peptide_src implementation determines how to free srcs
 * This decision is made by global variable PEPTIDE_SRC_USE_LINK_LIST
 */
void free_peptide(
  PEPTIDE_T* peptide ///< peptide to free -in
  )
{

  if( peptide == NULL ){
    return;
  }
  if( peptide->peptide_src ){
    // check which implementation peptide_src uses
    if(!PEPTIDE_SRC_USE_LINK_LIST){
      // array implementation
      free(peptide->peptide_src);    
    }
    else{
      // link list implementation
      free_peptide_src(peptide->peptide_src);
    }
  }

  if(peptide->modified_seq){
    free(peptide->modified_seq);;
  }
  if(peptide->decoy_modified_seq){
    free(peptide->decoy_modified_seq);;
  }
  free(peptide);
}

/* Public functions--Getters and Setters */

/**
 * sets the peptide src implementation in the peptide object
 * This should be set only once and not be altered
 */
void set_peptide_src_implementation(
  BOOLEAN_T use_link_list ///< does the peptide use link list peptide src
  )
{  
  PEPTIDE_SRC_USE_LINK_LIST = use_link_list; 
}
                            
/* mass-related getters and setters */

/**
 * sets the peptide mass
 */
void set_peptide_peptide_mass( 
  PEPTIDE_T* peptide,  ///< the peptide to set -out
  FLOAT_T peptide_mass  ///< the mass of the peptide - in
  )
{
  peptide->peptide_mass = peptide_mass;
}

/**
 * \returns the peptide mass
 */

/*inline*/ FLOAT_T get_peptide_peptide_mass( 
  PEPTIDE_T* peptide  ///< the peptide to query the mass -in
  )
{
  return peptide->peptide_mass;
}


/** 
 * \returns The mass of the peptide if it had charge "charge"
 */
FLOAT_T get_peptide_charged_mass(
 PEPTIDE_T* peptide, ///< the query peptide -in
 int charge ///< charge of peptide -in
 )
{
  return get_peptide_mz(peptide, charge) * charge;
}

/** 
 * \returns The m/z of the peptide if it had charge "charge"
 */
FLOAT_T get_peptide_mz(
    PEPTIDE_T* peptide, ///< the query peptide -in
    int charge ///< the charge of peptide -in
    )
{
  return ((get_peptide_peptide_mass(peptide) + MASS_H * charge)/ charge);
}

/* source-related getters and setters */

/**
 * sets the peptide_src field in the peptide
 * this method should be ONLY used when the peptide has no existing list of peptide_src
 * use add_peptide_peptide_src method to add to existing list
 * must pass on a heap allocated peptide_src object
 * does not copy in the object, just the pointer to the object.
 */
void set_peptide_peptide_src(
  PEPTIDE_T* peptide,  ///< the peptide to set -out                                             
  PEPTIDE_SRC_T* new_association ///< new peptide_src -in
  )
{
  peptide->peptide_src = new_association;
}

/**
 * this method adds the new_association to the end of the existing peptide's 
 * linklist of peptide_srcs
 * if no prior existing list, adds it at the front
 * must pass on a heap allocated peptide_src object
 * does not copy in the object, just the pointer to the object.
 */
void add_peptide_peptide_src(
  PEPTIDE_T* peptide,  ///< the peptide to set -out
  PEPTIDE_SRC_T* new_association ///< new peptide_src -in
  )
{
  PEPTIDE_SRC_T* add_association = peptide->peptide_src;
  PEPTIDE_SRC_ITERATOR_T* iterator = NULL;
  
  // is the peptide src list empty?
  if(add_association == NULL){
    peptide->peptide_src = new_association;
    return;
  }

  // create peptide src iterator
  iterator = new_peptide_src_iterator(peptide);

  // find the last peptide_src object in the list
  while(peptide_src_iterator_has_next(iterator)){
    add_association = peptide_src_iterator_next(iterator);
  }
  
  set_peptide_src_next_association(add_association, new_association);
  free_peptide_src_iterator(iterator);
}

/**
 * this method adds the peptide src array to an EMPTY peptide
 * only used in index.c, when the peptide src count for  peptide is known
 * Any existing peptide_src will lose it's reference
 */
void add_peptide_peptide_src_array(
  PEPTIDE_T* peptide,  ///< the peptide to set -out
  PEPTIDE_SRC_T* peptide_src_array ///< new peptide_src -in
  )
{
  // should be empty peptide src list
  peptide->peptide_src = peptide_src_array;

}

// TODO: why do we need both of these?
/**
 * returns a pointer to the peptide_protein_association field of the peptide
 */
PEPTIDE_SRC_T* get_peptide_peptide_src(
  PEPTIDE_T* peptide  ///< the peptide to query the peptide_peptide_src -in
  )
{
  return peptide->peptide_src;
}

/**
 * get the peptide->first peptide_src->parent protein->database
 */
DATABASE_T* get_peptide_first_src_database(
  PEPTIDE_T* peptide ///< working peptide -in
  )
{
  return get_peptide_src_parent_protein(peptide->peptide_src)->getDatabase();
}

// set by peptide_src?
/**
 * returns a pointer to the peptide's first parent protein field of the peptide
 */
Protein* get_peptide_parent_protein(
  PEPTIDE_T* peptide  ///< the peptide to query the parent_protein -in
  )
{
  return get_peptide_src_parent_protein(peptide->peptide_src);
}

/**
 * sets the sequence length of the peptide
 * length maximum of 255
 */
void set_peptide_length( 
  PEPTIDE_T* peptide,  ///< the peptide to set the length -out
  unsigned char length  ///< the length of sequence -in
  )
{
  peptide->length = length;
}

/* sequence-related getters and setters */
/**
 *\returns the sequence length of the peptide
 */
unsigned char get_peptide_length( 
  PEPTIDE_T* peptide  ///< the peptide to query the length -in
  )
{
  return peptide->length;
}

/**
 * \brief Check whether a given sequence is equal to a given peptide.
 * \returns A Boolean indicating equality or not.
 */
static BOOLEAN_T equal_peptides(
 char* peptide_sequence, ///< peptide sequence -in
 PEPTIDE_T* peptide_object ///< peptide object -in
 )
{
  char* parent_sequence = 
    get_peptide_src_parent_protein(peptide_object->peptide_src)->
    getSequencePointer();
  int start_idx = get_peptide_src_start_idx(peptide_object->peptide_src);

  int result = strncmp(peptide_sequence, 
                       &(parent_sequence[start_idx-1]), 
                       peptide_object->length);

  // Return TRUE if strncmp returns 0.
  return((BOOLEAN_T)(!result));
}


/**
 * \brief Get a string representation of the peptide sequence with no
 * added modification symbols.
 * Returns decoy sequence for decoy peptides.
 * \returns The newly-allocated sequence of peptide
 */
char* get_peptide_sequence(
 PEPTIDE_T* peptide ///< peptide to query sequence -in
 )
{
  if(peptide == NULL){
    carp(CARP_ERROR, "Cannot get sequence from NULL peptide.");
    return NULL;
  }
  if(peptide->peptide_src == NULL){
    carp(CARP_ERROR, "Cannot get sequence from peptide with no peptide src.");
    return NULL;
  }

  char* seq_copy = NULL;

  if(peptide->decoy_modified_seq != NULL){
    seq_copy = modified_aa_to_unmodified_string(peptide->decoy_modified_seq, 
                                                peptide->length);
  } else {
    seq_copy = get_peptide_unshuffled_sequence(peptide);
  }
 
  return seq_copy; 
}

/**
 * \brief Get a string representation of the target (unshuffled)
 * peptide sequence with no added modification symbols.
 * For target peptides, returns the same as get_peptide_sequence.
 * \returns The newly-allocated sequence of peptide
 */
char* get_peptide_unshuffled_sequence(
 PEPTIDE_T* peptide ///< peptide to query sequence -in
 )
{
  if(peptide == NULL){
    carp(CARP_ERROR, "Cannot get sequence from NULL peptide.");
    return NULL;
  }
  if(peptide->peptide_src == NULL){
    carp(CARP_ERROR, "Cannot get sequence from peptide with no peptide src.");
    return NULL;
  }

  char* parent_sequence = 
    get_peptide_src_parent_protein(peptide->peptide_src)->getSequencePointer();
  int start_idx = get_peptide_src_start_idx(peptide->peptide_src);

  char* copy_sequence = copy_string_part(&parent_sequence[start_idx-1],
                                         peptide->length);
 
  return copy_sequence; 
}

/**
 * \brief Get a pointer to the peptide sequence that is NOT null
 * terminated.
 * USE WITH CAUTION.  Pointer is to the parent protein sequence and
 * thus is not null-terminated until the end of the protein.  Parent
 * protein is taken from the first protein source.
 * 
 * \returns A pointer to an existing peptide sequence.
 */
char* get_peptide_sequence_pointer(
  PEPTIDE_T* peptide ///< peptide to query sequence -in
  )
{
  if(peptide->peptide_src == NULL){
    carp(CARP_FATAL, "ERROR: no peptide_src to retrieve peptide sequence pointer\n");
  }
  char* parent_sequence = 
    get_peptide_src_parent_protein(peptide->peptide_src)->getSequencePointer();
  int start_idx = get_peptide_src_start_idx(peptide->peptide_src);

  char* pointer_peptide_sequence = &parent_sequence[start_idx-1];
  
  return pointer_peptide_sequence;
}

/**
 * \returns The sequence of peptide as used in sqt files, namely with
 * each flanking AA and any modifications 
 * 
 * Format is [AA|-].[peptide_sequence].[AA|-] where AA is a flanking
 * amino acid and - indicates this is the end of the protein sequence
 * Gets flanking AAs from the first peptide_src, thus must have at
 * least one peptide src 
 *
 * \returns A newly allocated char* with formated peptide sequence
 */
char* get_peptide_sequence_sqt(
 PEPTIDE_T* peptide ///< peptide to query sequence -in
 )
{
  if(peptide == NULL || peptide->peptide_src == NULL){
    carp(CARP_ERROR, "Cannot get sequence from NULL peptide or peptide src.");
    return NULL;
    //    carp(CARP_FATAL, "ERROR: no peptide_src to retrieve peptide sequence\n");
  }
  
  char* seq = get_peptide_sequence_from_peptide_src_sqt(peptide, 
                                                        peptide->peptide_src);

  return seq;
}

/**
 * \brief Formats the sequence of the peptide from a particular
 * peptide_src.
 *
 * Is called by get_peptide_sequence_sqt()
 * Format is "X.peptide_sequence.X", where "X" is a flanking amino acid.
 * "X", is printed as "-" if there is no flanking sequence.  Includes
 * any modifications.
 * Goes to the first peptide_src to gain sequence, thus must have at
 * least one peptide src 
 *
 * \returns A newly allocated string with the sqt-formated peptide sequence.
 */
char* get_peptide_sequence_from_peptide_src_sqt(
 PEPTIDE_T* peptide, ///< peptide to query sequence -in
 PEPTIDE_SRC_T* peptide_src ///< peptide_src -in 
 )
{
  char* copy_sequence = NULL;
  Protein* protein = get_peptide_src_parent_protein(peptide_src);
  // get peptide start idx of protein in prarent protein
  int start_idx = get_peptide_src_start_idx(peptide_src);
  // parent protein length
  int protein_length = protein->getLength();

  char* parent_sequence = 
    protein->getSequencePointer();

  // get modified petpide sequence
  char* mod_pep_seq = get_peptide_modified_sequence_with_symbols(peptide);
  int mod_pep_len = strlen(mod_pep_seq);

  // allocate peptide memory
  //  copy_sequence = (char*)mycalloc(peptide->length+5, sizeof(char));
  copy_sequence = (char*)mycalloc(mod_pep_len+5, sizeof(char));

  // Default template is "X.peptide.X", where "X" are flanking amino acids
  copy_sequence[0] = '-';
  copy_sequence[1] = '.';
  copy_sequence[mod_pep_len+2] = '.';
  copy_sequence[mod_pep_len+3] = '-';
  copy_sequence[mod_pep_len+4] = '\0';

  // copy over the peptide sequences
  //strncpy(&copy_sequence[2], &parent_sequence[start_idx-1], peptide->length);
  strncpy(&copy_sequence[2], mod_pep_seq, mod_pep_len);
  
  // is there an AA before?
  if(start_idx != 1){
    copy_sequence[0] = parent_sequence[start_idx-2];
  }
  // is there an AA after?
  if((start_idx + peptide->length - 1) < protein_length){
    copy_sequence[peptide->length+3] = parent_sequence[start_idx+peptide->length-1];
  }
  
  free(mod_pep_seq);
  // yeah return!!
  return copy_sequence; 
}

/**
 * \brief Return a char for the amino acid c-terminal to the peptide
 * in the peptide src at the given index.
 *
 * \returns A char (A-Z) or - if peptide is the first in the protein.
 */
char get_peptide_c_term_flanking_aa(
 PEPTIDE_T* peptide   ///< peptide of interest
 ){
  if( peptide == NULL ){
    carp(CARP_ERROR, "Cannot get flanking amino acid from null peptide");
    return '\0';
  }

  // get protein seq
  Protein* protein = get_peptide_src_parent_protein(peptide->peptide_src);
  char* protein_seq = protein->getSequencePointer();

  // get peptide start idx, protein index starts at 1
  int start_index = get_peptide_src_start_idx(peptide->peptide_src);

  char aa = '-';
  // if not at beginning, return char
  if( start_index > 1 ){
    aa = protein_seq[start_index - 2]; // -1 for 1-based shift
                                       // -1 for aa before start
  } 
  return aa;
}

/**
 * \brief Return a char for the amino acid n-terminal to the peptide
 * in the peptide src at the given index.
 *
 * \returns A char (A-Z) or - if peptide is the last in the protein.
 */
char get_peptide_n_term_flanking_aa(
 PEPTIDE_T* peptide   ///< peptide of interest
 )
{
  if( peptide == NULL ){
    carp(CARP_ERROR, "Cannot get flanking amino acid from null peptide");
    return '\0';
  }

  // get protein seq and length
  Protein* protein = get_peptide_src_parent_protein(peptide->peptide_src);
  char* protein_seq = protein->getSequencePointer();
  int protein_length = protein->getLength();

  // get peptide end idx, protein index starts at 1
  int start_index = get_peptide_src_start_idx(peptide->peptide_src);
  int end_index = start_index + peptide->length - 1;

  char aa = '-';
  // if not at end, return char
  if( end_index < protein_length ){
    aa = protein_seq[end_index]; // -1 for 1-based shift, +1 for aa after end
  } 
  return aa;
}

/**
 * \brief Add a modification to a peptide.
 *
 * Adds the modified sequence to the peptide and changes the peptide
 * mass based on the mass change in the peptide_mod.
 * \returns void
 */
void set_peptide_mod(PEPTIDE_T* peptide,     ///< peptide to be modified
                     MODIFIED_AA_T* mod_seq, ///< modified seq to add
                     PEPTIDE_MOD_T* pep_mod  ///< mod that made the seq
){
  if( peptide == NULL || mod_seq == NULL || pep_mod == NULL ){
    carp(CARP_ERROR, "Cannot modify peptide.  Peptide, mod, or seq is NULL.");
    return;
  }

  // comment me to fix files
  // check that peptide doesn't already have a mod?
  peptide->modified_seq = mod_seq;// should this be a copy instead??
  // change mass
  peptide->peptide_mass += peptide_mod_get_mass_change(pep_mod);

}

void set_peptide_decoy_modified_seq(PEPTIDE_T* peptide, MODIFIED_AA_T* mod_seq) {
  peptide->decoy_modified_seq=mod_seq;
}

BOOLEAN_T get_peptide_is_modified(PEPTIDE_T* peptide) {
  return peptide->modified_seq != NULL;
}



/**
 * \brief Get the modified peptide sequence
 *
 * If the peptide has no modifications, create a sequence of
 * MODIFIED_AA_T's in which none of them are actually modified.
 * \returns A newly allocated copy of the sequence of MODIFIED_AA_Ts.
 */
MODIFIED_AA_T* get_peptide_modified_aa_sequence(PEPTIDE_T* peptide){
  if( peptide == NULL ){
    carp(CARP_ERROR, "Cannot get modified sequence from NULL peptide.");
    return NULL;
  }

  MODIFIED_AA_T* seq_copy = NULL;
  
  if( peptide->modified_seq != NULL ){ 
    carp(CARP_DETAILED_DEBUG, "Getting modified seq from peptide.");
    seq_copy = copy_mod_aa_seq(peptide->modified_seq,
                               peptide->length);

  } else {// create one from char seq
    carp(CARP_DETAILED_DEBUG, "mod seq NOT cached");
    char* seq = get_peptide_sequence(peptide);
    convert_to_mod_aa_seq(seq, &seq_copy);
    free(seq);
  }
  
  return seq_copy;
}

/**
 * \brief Get the modified aa sequence in string form.
 *
 * If the peptide has no modifications, returns same string as
 * get_peptide_sequence.  If modified, adds the mod symbols to the string.
 * \returns A newly allocated string of the peptide sequence including
 * any modifications.
 */
char* get_peptide_modified_sequence_with_symbols(
 PEPTIDE_T* peptide
 ){
  char* seq_string = NULL;

  if( peptide->decoy_modified_seq ){
    seq_string = 
      modified_aa_string_to_string_with_symbols(peptide->decoy_modified_seq, 
                                              peptide->length); 
  } else if( peptide->modified_seq == NULL ){
    seq_string = get_peptide_sequence(peptide);
  }else{
    seq_string = 
      modified_aa_string_to_string_with_symbols(peptide->modified_seq,
                                                peptide->length);
  }
  
  return seq_string;
}

/**
 * \brief Get the modified aa sequence in string form.
 *
 * If the peptide has no modifications, returns same string as
 * get_peptide_sequence.  If modified, adds in brackets the masses of
 * all modifications.  If merge_masses is true, prints the sum of all
 * modifications for a residue.  If false, prints all masses in a
 * comma separated list.
 * \returns A newly allocated string of the peptide sequence including
 * any modifications.
 */
char* get_peptide_modified_sequence_with_masses(
 PEPTIDE_T* peptide,
 BOOLEAN_T merge_masses
 ){
  char* seq_string = NULL;

  if( peptide->decoy_modified_seq ){
    seq_string = 
      modified_aa_string_to_string_with_masses(peptide->decoy_modified_seq, 
                                               peptide->length,
                                               merge_masses); 
  } else if( peptide->modified_seq == NULL ){
    seq_string = get_peptide_sequence(peptide);
  }else{
    seq_string = 
      modified_aa_string_to_string_with_masses(peptide->modified_seq,
                                               peptide->length,
                                               merge_masses);
  }
  
  return seq_string;
}

/**
 * \brief Get the target sequence of the peptide encoded as char*
 * including modification symbols (e.g. *,#).
 *
 * If the peptide is not a decoy, returns the same sequence as
 * get_peptide_modified_sequence.  If the peptide has no
 * modifications, returns same string as get_peptide_sequence.  If
 * modified, adds the mod symbols to the string. 
 * \returns A newly allocated string of the peptide's unshuffled
 * (target) sequence including any modifications.
 */
char* get_peptide_unshuffled_modified_sequence(
 PEPTIDE_T* peptide
 ){
  char* seq_string = NULL;
  if( peptide->modified_seq == NULL ){
    seq_string = get_peptide_sequence(peptide);
  }else{
    seq_string = 
      modified_aa_string_to_string_with_symbols(peptide->modified_seq,
                                                peptide->length);
  }
  
  return seq_string;
}

/* getters requiring calculation */

/**
 * \brief Count the number of modified amino acids in the
 * peptide. This number is distnct from the number of aamods in the
 * peptide mod since one amino acid can have more than one
 * modification on it.
 * \returns The number of amino acids in the peptide that have at
 * least one modification.
 */
int count_peptide_modified_aas(PEPTIDE_T* peptide){
  if( peptide == NULL ){
    return 0;
  }

  if( peptide->modified_seq == NULL ){
    return 0;
  }

  int count = 0;
  int aa_idx = 0;
  for(aa_idx=0; aa_idx < peptide->length; aa_idx++){
    //    carp(CARP_DETAILED_INFO, "modified aa %s and mask is %hu", modified_aa_to_string( peptide->modified_seq[aa_idx]), (GET_MOD_MASK & modified_aa_to_string( peptide->modified_seq[aa_idx])));
    if( GET_MOD_MASK & peptide->modified_seq[aa_idx] ){
      count++;
    }
  }

  return count;
}

/**
 * \returns The mass of the given peptide as determined by the aa sequence.
 */
FLOAT_T calc_sequence_mass(
  const char* peptide, ///< the query peptide -in
  MASS_TYPE_T mass_type ///< isotopic mass type (AVERAGE, MONO) -in
  )
{
  FLOAT_T peptide_mass = 0;
  int idx = 0;
  char amino;
  while(peptide[idx] != '\0'){
    amino = peptide[idx++];
    peptide_mass += get_mass_amino_acid(amino, mass_type);
  }
  if(mass_type == AVERAGE){
    return peptide_mass + MASS_H2O_AVERAGE;
  }
  return peptide_mass + MASS_H2O_MONO;
}

/**
 * This appears to be the same as calc_sequence_mass??
 * \returns The mass of the given peptide.
 */
FLOAT_T calc_peptide_mass(
  PEPTIDE_T* peptide, ///< the query peptide -in
  MASS_TYPE_T mass_type ///< isotopic mass type (AVERAGE, MONO) -in
  )
{
  FLOAT_T peptide_mass = 0;
  RESIDUE_ITERATOR_T * residue_iterator = new_residue_iterator(peptide);
  
  while(residue_iterator_has_next(residue_iterator)){
    peptide_mass += get_mass_amino_acid(residue_iterator_next(residue_iterator), mass_type);
  }
  free_residue_iterator(residue_iterator);

  if(mass_type == AVERAGE){
    return peptide_mass + MASS_H2O_AVERAGE;
  }
  return peptide_mass + MASS_H2O_MONO;
}

FLOAT_T calc_modified_peptide_mass(
  PEPTIDE_T* peptide, ///< the query peptide -in
  MASS_TYPE_T mass_type ///< isotopic mass type (AVERAGE, MONO) -in
  ) {

  if (peptide->modified_seq == NULL) {
    carp(CARP_DETAILED_DEBUG,"Peptide not modified return sequence mass");
    return calc_peptide_mass(peptide, mass_type);
  }

  AA_MOD_T** mod_list = NULL;
  int total_mods = get_all_aa_mod_list(&mod_list);

  FLOAT_T peptide_mass = 0;
  for (int idx = 0; idx < get_peptide_length(peptide); idx++) {
    MODIFIED_AA_T modified_aa = peptide->modified_seq[idx];
    char aa = modified_aa_to_char(modified_aa);
    peptide_mass += get_mass_amino_acid(aa, mass_type);

    for (int mod_idx = 0;mod_idx < total_mods;mod_idx++) {
      if ( is_aa_modified(modified_aa, mod_list[mod_idx])) {
        peptide_mass += aa_mod_get_mass_change(mod_list[mod_idx]);
      }
    }
  }

  if (mass_type == AVERAGE) {
    return peptide_mass + MASS_H2O_AVERAGE;
  } else {
    return peptide_mass + MASS_H2O_MONO;
  }
  
}


static FLOAT_T krokhin_index['Z'-'A'] = {
  0.8, 0.0, -0.8, -0.5, 0.0, 10.5, -0.9, -1.3, 8.4, 0.0, 
  -1.9, 9.6, 5.8, -1.2, 0.0, 0.2, -0.9, -1.3, -0.8, 0.4,
  0.0, 5.0, 11.0, 0.0, 4.0};

/*
 * Calculates the peptide hydrophobicity, as in Krokhin (2004).
 */
FLOAT_T calc_krokhin_hydrophobicity(
  PEPTIDE_T* peptide ///< the query peptide -in
)
{
  FLOAT_T krokhin = 0.0;
  RESIDUE_ITERATOR_T * residue_iterator = new_residue_iterator(peptide);
  while(residue_iterator_has_next(residue_iterator)){
    char c = residue_iterator_next(residue_iterator)-'A';
    krokhin += krokhin_index[(int)c];
  }
  free_residue_iterator(residue_iterator);

  return krokhin;
}

/**
 * Examines the peptide sequence and counts how many tryptic missed
 * cleavage sites exist. 
 *\returns the number of missed cleavage sites in the peptide
 */
int get_peptide_missed_cleavage_sites(
  PEPTIDE_T* peptide  ///< the peptide to query -in
  )
{
  int missed_count = 0;
  int aa_idx = 0;
  char* sequence = get_peptide_sequence_pointer(peptide);

  // count the missed cleavage sites
  for(; aa_idx < peptide->length-1; ++aa_idx){
    if(sequence[aa_idx] == 'K' ||
       sequence[aa_idx] == 'R'){
      
      // skip one that are followed by a P
      if(sequence[aa_idx+1] == 'P'){
        continue;
      }
      //TODO if modification prevents cleavage, then skip it.
      
      else{
        ++missed_count;
      }      
    } 
  }
  
  return missed_count;
}

int get_peptide_missed_cleavage_sites(
  PEPTIDE_T* peptide, ///< the peptide to query -in
  std::set<int> skip //skip these amino acid indices.
  ) {

  int missed_count = 0;
  char* sequence = get_peptide_sequence_pointer(peptide);

  AA_MOD_T** mod_list = NULL;
  int total_mods = 0;
  if (peptide->modified_seq) {
   
    total_mods = get_all_aa_mod_list(&mod_list);
    
  }

  // count the missed cleavage sites
  for(int aa_idx=0; aa_idx < peptide->length-1; ++aa_idx){
    
    if (skip.find(aa_idx) != skip.end()) {
      continue;
    }

    bool cleavage_prevented = true;
    if(sequence[aa_idx] == 'K' ||
       sequence[aa_idx] == 'R'){

      cleavage_prevented = false;
      // skip one that are followed by a P
      if(sequence[aa_idx+1] == 'P'){
        cleavage_prevented = true;
      }

      if (peptide->modified_seq) {
        int mod_idx = 0;
        while (!cleavage_prevented && mod_idx < total_mods) {
          if (aa_mod_get_prevents_cleavage(mod_list[mod_idx])) {
            if (is_aa_modified(peptide->modified_seq[aa_idx], mod_list[mod_idx])) {
              cleavage_prevented = true;
            }
          }
          mod_idx++;
        }
      }
    }

    if (!cleavage_prevented) {
      ++missed_count;
    }
  }
  
  return missed_count;

}

/**
 * \brief Find the distance from the c-terminus of the source protein
 * to the c-terminus of the peptide (seq[0]).  
 * In the case of multiple source proteins, return the smallest
 * distance.
 * \returns The distance from the protein c-terminus.
 */
int get_peptide_c_distance(PEPTIDE_T* peptide){

  int min_index = MAX_PROTEIN_SEQ_LENGTH;
  PEPTIDE_SRC_T* cur_src = peptide->peptide_src;
  while( cur_src != NULL ){
    int index = get_peptide_src_start_idx(cur_src);
    if( index < min_index ){
      min_index = index;
    }
    cur_src = get_peptide_src_next_association(cur_src);
  }
  return min_index - 1;
}

/**
 * \brief Find the distance from the n-terminus of the source protein
 * to the n-terminus of the peptide.
 * In the case of multiple source proteins, return the smallest
 * distance.
 * \returns The distance from the protein c-terminus.
 */
int get_peptide_n_distance(PEPTIDE_T* peptide){

  int min_index = MAX_PROTEIN_SEQ_LENGTH;
  int peptide_length = get_peptide_length(peptide);
  PEPTIDE_SRC_T* cur_src = peptide->peptide_src;

  while( cur_src != NULL ){
    // get protein length
    int protein_length = get_peptide_src_parent_protein(cur_src)->getLength();
    // get index of end
    int start_index = get_peptide_src_start_idx(cur_src);

    int cidx = protein_length - (start_index + peptide_length - 1);
    if( cidx < min_index){
      min_index = cidx;
    }
    cur_src = get_peptide_src_next_association(cur_src);
  }
  return min_index;


}

/**
 * Creates a heap allocated hash_value for the peptide that should
 * uniquely identify the peptide
 *\returns the string of "<first src protein idx><start idx><length>"
 */
char* get_peptide_hash_value( 
  PEPTIDE_T*  peptide ///< The peptide whose residues to iterate over.
  )
{
  char* hash_value = NULL;
  int peptide_length_space = get_number_digits(peptide->length);
  unsigned int protein_idx = 
    get_peptide_src_parent_protein(peptide->peptide_src)->getProteinIdx();
  int protein_idx_space = get_number_digits(protein_idx);
  int peptide_start_idx = get_peptide_src_start_idx(peptide->peptide_src);
  int peptide_start_idx_space = get_number_digits(peptide_start_idx);
  int status;
  int space = peptide_length_space + protein_idx_space + peptide_start_idx_space + 1;

  // allocate space for three integers
  hash_value = (char*)mycalloc(space,sizeof(char));
  
  // copy over the itegers
  status = snprintf(hash_value, 
                    space,
                    "%d%d%d", 
                   protein_idx, 
                   peptide_start_idx, 
                   peptide->length);
  
  if(status != (space-1)){
    carp(CARP_ERROR, "failed to create peptide hash value");
  }
  return hash_value;
}

/**
 * Change the given target peptide into a decoy by randomizing its sequence.
 * Uses settings in parameter.c to decide between shuffling and
 * reversing the sequence.  Any modifications that exist will be
 * maintained on the same amino acids whose position will move.  If
 * the peptide is already a decoy, replaces the existing decoy
 * sequence. 
 */
void transform_peptide_to_decoy(PEPTIDE_T* peptide){
  if(peptide == NULL){
    carp(CARP_ERROR, "Can't turn NULL peptide into decoy.");
    return;
  }

  BOOLEAN_T reverse_seq = get_boolean_parameter("reverse-sequence");

  // delete any existing decoy sequence
  if(peptide->decoy_modified_seq){ 
    free(peptide->decoy_modified_seq); 
  }
  // if the peptide is already modified, shuffle the modified sequence
  if(peptide->modified_seq){
    MODIFIED_AA_T* new_seq = NULL;
    if( reverse_seq ){
      new_seq = generate_reversed_mod_sequence(peptide);
    } else {
      new_seq = generate_shuffled_mod_sequence(peptide);
    }
    peptide->decoy_modified_seq = new_seq;

  } else {// shuffle the unmodified sequence
    char* new_seq = NULL;
    if( reverse_seq ){
      new_seq = generate_reversed_sequence(peptide);
    } else {
      new_seq = generate_shuffled_sequence(peptide);
    }
    convert_to_mod_aa_seq(new_seq, &(peptide->decoy_modified_seq));
    free(new_seq);
  }
}

/**
 * \brief Return a randomly shuffled version of the given peptide's
 * sequence, leaving the terminal amino acids in place.  Ensures that
 * the shuffled version is not the same as the given peptide.
 * 
 * \returns A newly-allocated char array with the shuffled sequence.
 */
static const int MAX_SHUFFLES = 5; // Don't bother trying to shuffle more than this.
char* generate_shuffled_sequence(
  PEPTIDE_T* peptide ///< The peptide to shuffle -in 
  )
{
  // Allocate a copy of the peptide.
  char* sequence = get_peptide_sequence(peptide);
  int length = peptide->length;

  // Shuffle from left to right, using the Knuth algorithm for shuffling.
  int num_shuffles = 0;
  do {

    // Don't move the n-term and c-term amino acids
    int start_idx = 1;
    int end_idx = length - 2;

    while(start_idx < end_idx){
      int switch_idx = get_random_number_interval(start_idx, end_idx);
      char temp_char = sequence[start_idx];
      sequence[start_idx] = sequence[switch_idx];
      sequence[switch_idx] = temp_char;
      ++start_idx;
    }
    num_shuffles++;
  } while (equal_peptides(sequence, peptide) && (num_shuffles < MAX_SHUFFLES));

  return sequence;
}

/**
 * \brief Return a reversed version of the given peptide's sequence as
 * an array of char (A-Z).  Leave the first and last residue
 * unchanged.  If the reversed sequence is identical to the target,
 * shuffle the sequence instead.
 *
 * \returns A newly-allocated char array of the reversed sequence.
 */
char* generate_reversed_sequence(
  PEPTIDE_T* peptide ///< The peptide to shuffle -in 
  ){

  char* sequence = get_peptide_sequence(peptide);
  int length = peptide->length;
  int start_idx = 1;       // leave first ...
  int end_idx = length -2; // ...and last residue in place
  char temp_char = 0;

  while(start_idx < end_idx){
    temp_char = sequence[end_idx];
    sequence[end_idx] = sequence[start_idx];
    sequence[start_idx] = temp_char;
    start_idx++;
    end_idx--;
  }

  // check to see if the reversed sequence is the same as original
  if( strncmp(sequence, get_peptide_sequence_pointer(peptide), length) == 0 ){
    carp(CARP_DETAILED_INFO, 
         "Peptide %s is a palindrome and will be shuffled instead of reversed.",
         sequence);
    free(sequence);
    sequence = generate_shuffled_sequence(peptide);
  }

  return sequence;
}

/**
 * \brief Return a randomly shuffled version of the given peptide's 
 * sequence as an array of MODIIFIED_AA_T.  Based on the peptide type,
 * will leave the end(s) unchanged to preserve the tryptic property.
 * 
 *\returns A newly-allcoated MODIFIED_AA_T array of the shuffled sequence.
 */
MODIFIED_AA_T* generate_shuffled_mod_sequence(
  PEPTIDE_T* peptide  ///< The peptide sequence to shuffle -in
  //PEPTIDE_TYPE_T peptide_type
  ///< tryptic status to enforce on the shuffled sequence
  // not currently used
  )
{
  //TODO (BF 6-Apr-09): should we warn if seq is len 3 and won't change?
  MODIFIED_AA_T* sequence = get_peptide_modified_aa_sequence(peptide);
  int length = peptide->length;
  int start_idx = 0;
  int end_idx = length - 1;
  int switch_idx = 0;
  MODIFIED_AA_T temp_aa = 0;

  // Do not move the first and last residue, regardless of enzyme
  ++start_idx;
  --end_idx;

  // shuffle from left to right, using the Knuth algorithm for shuffling.
  while(start_idx < end_idx){
    switch_idx = get_random_number_interval(start_idx, end_idx);
    temp_aa = sequence[start_idx];
    sequence[start_idx] = sequence[switch_idx];
    sequence[switch_idx] = temp_aa;
    ++start_idx;
  }

  return sequence;
}

/**
 * \brief Return a reversed version of the given peptide's sequence as
 * an array of MODIFIED_AA_T.  Leave the first and last residue
 * unchanged.  If the reversed sequence is identical to the target,
 * shuffle the sequence instead.
 *
 * \returns A newly-allocated MODIFIED_AA_T array of the reversed sequence.
 */
MODIFIED_AA_T* generate_reversed_mod_sequence(
  PEPTIDE_T* peptide ///< The peptide to shuffle -in 
  ){
  MODIFIED_AA_T* sequence = get_peptide_modified_aa_sequence(peptide);
  int length = peptide->length;
  int start_idx = 0;
  int end_idx = length - 1;
  MODIFIED_AA_T temp_aa = 0;

  // first check to see if it will yield a different seq when reversed
  if( modified_aa_seq_is_palindrome(sequence, length) == TRUE){
    return generate_shuffled_mod_sequence(peptide);
  }

  // Do not move the first and last residue, regardless of enzyme
  ++start_idx;
  --end_idx;

  // reverse  
  while(start_idx < end_idx){
    temp_aa = sequence[start_idx];
    sequence[start_idx] = sequence[end_idx];
    sequence[end_idx] = temp_aa;
    ++start_idx;
    end_idx--;
  }

  return sequence;
}


/* Comparisons for sorting */

/**
 * Compare peptide sequence
 * \returns TRUE if peptide sequence is identical else FALSE
 */
BOOLEAN_T compare_peptide_sequence(
  PEPTIDE_T* peptide_one,  ///< the peptide sequence to compare  -out
  PEPTIDE_T* peptide_two  ///< the peptide sequence to compare  -out
  )
{
  // is Mass and Length identical
  if(compare_float(peptide_one->peptide_mass, peptide_two->peptide_mass) != 0 ||
     peptide_one->length != peptide_two->length){
    return FALSE;
  }
  else{
    int current_idx = 0;
    char* start_one = get_peptide_src_sequence_pointer(peptide_one->peptide_src);
    char* start_two = get_peptide_src_sequence_pointer(peptide_two->peptide_src);
    
    while(current_idx < peptide_one->length){
      if(start_one[current_idx] != start_two[current_idx]){
        return FALSE;
      }
      ++current_idx;
    } 
  }
  return TRUE;
}


/**
 * compares two peptides with the lexical sort type
 * for qsort
 * /returns 1 if peptide_one has lower priority, 0 if equal, -1 if greater priority
 */
int compare_peptide_lexical_qsort(
  PEPTIDE_T** peptide_one, ///< peptide to compare one -in
  PEPTIDE_T** peptide_two ///< peptide to compare two -in
  )
{
  // convert the protein to heavy if needed
  /* uncomment if needed, to use light heavy protein
  protein_to_heavy(get_peptide_parent_protein(peptide_one));
  protein_to_heavy(get_peptide_parent_protein(peptide_two));
  */
  PEPTIDE_T* peptide_1 = *peptide_one;
  PEPTIDE_T* peptide_2 = *peptide_two;
  char* peptide_one_sequence = get_peptide_sequence_pointer(peptide_1);
  char* peptide_two_sequence = get_peptide_sequence_pointer(peptide_2);
  int peptide_one_length = peptide_1->length;
  int peptide_two_length = peptide_2->length;
  int current_idx = 0;
  
  // check if all alphabetically identical
  while(current_idx < peptide_one_length &&
        current_idx < peptide_two_length){
    if(peptide_one_sequence[current_idx] > peptide_two_sequence[current_idx]){        
      return 1;
    }
    else if(peptide_one_sequence[current_idx] < peptide_two_sequence[current_idx]){
      return -1;
    }
    ++current_idx;
  }
  
  // alphabetically identical, check if same length
  if(peptide_one_length > peptide_two_length){
    return 1;
  }
  else if(peptide_one_length < peptide_two_length){
    return -1;
  }
  else{
    return 0;
  }
}

/**
 * compares two peptides with the mass sort type
 * if peptide mass is identical sort by lexicographical order
 * used for qsort function
 * /returns 1 if peptide_one has lower priority, 0 if equal, -1 if greater priority
 */
int compare_peptide_mass_qsort(
  PEPTIDE_T** peptide_one, ///< peptide to compare one -in
  PEPTIDE_T** peptide_two ///< peptide to compare two -in
  )
{
  // determine mass order
  int result = compare_float((*peptide_one)->peptide_mass, 
                             (*peptide_two)->peptide_mass);

  // if identical mass, sort in lexical order
  if(result == 0){
    return compare_peptide_lexical_qsort(peptide_one, peptide_two);
  }
  else{// if not identical
    return result;
  }
}

/**
 * compares two peptides with the length sort type
 * /returns 1 if peptide_one has lower priority, 0 if equal, -1 if greater priority
 */
int compare_peptide_length_qsort(
  PEPTIDE_T** peptide_one, ///< peptide to compare one -in
  PEPTIDE_T** peptide_two ///< peptide to compare two -in
  )
{
  int peptide_one_length = (*peptide_one)->length;
  int peptide_two_length = (*peptide_two)->length;
  
  // alphabetically identical, check if same length
  if(peptide_one_length > peptide_two_length){
    return 1;
  }
  else if(peptide_one_length < peptide_two_length){
    return -1;
  }
  else{
    return 0;
  }
}

/**
 * Compare peptide mass
 * \returns 0 if peptide mass is identical else 1 if peptide_one is larger, -1 if peptide_two is larger
 */
int compare_peptide_mass(
  PEPTIDE_T* peptide_one,  ///< the peptide mass to compare  -out
  PEPTIDE_T* peptide_two  ///< the peptide mass to compare  -out
  )
{
  return compare_float(peptide_one->peptide_mass, peptide_two->peptide_mass);
}

/* Public functions--Printing / parsing */

/* text writing */

/**
 * \brief Prints a peptide object in text to file.
 *
 * Used by crux-generate-peptides. Prints the peptide once for all
 * peptide_src objects associated with it.  Optional fields are
 * determined by arguments. Tab-delimited format is:
 * mass protein-id peptide-start peptide-length <peptide-trypticity>
 * <peptide-sequence>
 * Peptide start begins with 1.
 */
void print_peptide_in_format(
  PEPTIDE_T* peptide,  ///< the query peptide -in
  BOOLEAN_T flag_out, ///< print peptide sequence? -in
  //BOOLEAN_T trypticity_opt, ///< print trypticity of peptide? -in
  FILE* file  ///< the out put stream -out
  )
{
  Protein* parent = NULL;
  PEPTIDE_SRC_T* next_src = peptide->peptide_src;
  char* id = NULL;
  int start_idx = 0;
  char* sequence = NULL;

  // print mass of the peptide
    fprintf(file, "%.2f", peptide->peptide_mass);

  // obtain peptide sequence
  if(flag_out){
    parent = get_peptide_src_parent_protein(next_src);        
    if( peptide->modified_seq== NULL ){
      sequence = get_peptide_sequence(peptide);
    }else{
      sequence = 
        modified_aa_string_to_string_with_masses(peptide->modified_seq, 
                                                 peptide->length,
                         get_boolean_parameter("display-summed-mod-masses"));
    }
  }

  // iterate over all peptide src
  while(next_src != NULL){
    parent = get_peptide_src_parent_protein(next_src);    
    id = parent->getIdPointer();
    start_idx = get_peptide_src_start_idx(next_src);
    
    fprintf(file, "\t%s\t%d\t%d", id, start_idx, peptide->length);
  
    // print trypticity of peptide??
/*
    if(trypticity_opt){
      // TODO: change this to switch statement with only one get() call
      if(get_peptide_src_peptide_type(next_src) == TRYPTIC){
        fprintf(file, "\t%s", "TRYPTIC");
      }
      else if(get_peptide_src_peptide_type(next_src) == PARTIALLY_TRYPTIC){
        fprintf(file, "\t%s", "PARTIALLY_TRYPTIC");
      }
      else if(get_peptide_src_peptide_type(next_src) == N_TRYPTIC){
        fprintf(file, "\t%s", "N_TRYPTIC");
      }
      else if(get_peptide_src_peptide_type(next_src) == C_TRYPTIC){
        fprintf(file, "\t%s", "C_TRYPTIC");
      }
      else if(get_peptide_src_peptide_type(next_src) == NOT_TRYPTIC){
        fprintf(file, "\t%s", "NOT_TRYPTIC");
      }
      else if(get_peptide_src_peptide_type(next_src) == ANY_TRYPTIC){
        fprintf(file, "\t%s", "ANY_TRYPTIC");
      }
    }
*/
    // print peptide sequence?
    if(flag_out){
      fprintf(file, "\t%s\n", sequence);
    }
    else{
      fprintf(file, "\n");
    }
    next_src = get_peptide_src_next_association(next_src);    
  }

  // free sequence if allocated
  if(flag_out){
    free(sequence);
  }
}

// TODO: this should be merged with other print, flags for optional
// fields and filtering should be taken from parameter.c and format
// adjusted accordingly
/**
 * Prints a peptide object to file.
 * ONLY prints peptide_src that match the peptide_src
 * mass \\t protein-id \\t peptide-start \\t peptide-length <\\t peptide-sequence> \n
 *      \\t protein-id \\t peptide-start \\t peptide-length <\\t peptide-sequence> \n
 * prints in correct format for generate_peptide
 */
void print_filtered_peptide_in_format(
  PEPTIDE_T* peptide,  ///< the query peptide -in
  BOOLEAN_T flag_out, ///< print peptide sequence? -in
  FILE* file  ///< the out put stream -out
  //PEPTIDE_TYPE_T peptide_type ///< the peptide_type of src to print -in
  )
{
  Protein* parent = NULL;
  PEPTIDE_SRC_T* next_src = peptide->peptide_src;
  //char* id = NULL;
  //int start_idx = 0;
  char* sequence = NULL;
  // BOOLEAN_T light = FALSE;

  // print mass of the peptide
  fprintf(file, "%.2f", peptide->peptide_mass);

  // obtain peptide sequence
  if(flag_out){
    parent = get_peptide_src_parent_protein(next_src);
    
    // covnert to heavy protein
/*    
    FIXME, IF use light heavy put back
    if(get_protein_is_light(parent)){
      protein_to_heavy(parent);
      light = TRUE;
    }
*/
    sequence = get_peptide_sequence(peptide);
  }

  // iterate over all peptide src
/*
  while(next_src != NULL){
    if(peptide_type == ANY_TRYPTIC ||
       peptide_type == get_peptide_src_peptide_type(next_src) ||
       (peptide_type == PARTIALLY_TRYPTIC && 
        (get_peptide_src_peptide_type(next_src) == N_TRYPTIC ||
         get_peptide_src_peptide_type(next_src) == C_TRYPTIC)) ){
      
      // if(!light){
      parent = get_peptide_src_parent_protein(next_src);
        
      // covnert to heavy protein
      FIXME, IF use light heavy put back
      if(get_protein_is_light(parent)){
        protein_to_heavy(parent);
        light = TRUE;
      }
        // }
      
      id = get_protein_id_pointer(parent);
      start_idx = get_peptide_src_start_idx(next_src);
      
      fprintf(file, "\t%s\t%d\t%d", id, start_idx, peptide->length);
      
      // print peptide sequence?
      if(flag_out){
        fprintf(file, "\t%s\n", sequence);
      }
      else{
        fprintf(file, "\n");
      }
    
       * uncomment this code if you want to restore a protein to 
       * light after converted to heavy
      // convert back to light
      if(light){
        protein_to_light(parent);
        light = FALSE;
      }
    }
    next_src = get_peptide_src_next_association(next_src);
  }
*/

  // free sequence if allocated
  if(flag_out){
    free(sequence);
  }
}


/* binary read/writing (serialization) */

/**
 * Serialize a peptide in binary to a FILE 
 *
 * The peptide serialization format looks like this:
 *
 * <PEPTIDE_T: peptide struct><int: number of peptide_src>[<int:
 * protein index><DITEST_T: degree of digestion (replaced
 * peptide_type)><int: peptide start 
 * index>]+<int: modified_seq length>[<MODIFIED_AA_T>]+ 
 * The peptide src information (in square brackets) repeats for the
 * number times indicated by the number between the struct and the
 * first peptide src entry.  The protein index is the index of the
 * parent protein in the database DATABASE_T. The number of
 * MODIFIED_AA_T's is given by the int preceeding it.
 *
 * \returns TRUE if serialization is successful, else FALSE
 */
BOOLEAN_T serialize_peptide(
  PEPTIDE_T* peptide, ///< the peptide to serialize -in
  FILE* file,         ///< the binary output file to serialize to -out
  FILE* text_file     ///< the ASCII output file (may be NULL)
  )
{

  PEPTIDE_SRC_ITERATOR_T* iterator = 
    new_peptide_src_iterator(peptide);
  PEPTIDE_SRC_T* peptide_src = NULL;
  int num_src = 0;
  
  char* seq = get_peptide_sequence(peptide);
  carp(CARP_DETAILED_DEBUG, "Serializing peptide %s, len %i, mass %.2f", 
       seq, peptide->length, peptide->peptide_mass);

  // there must be at least one peptide src
  if(!peptide_src_iterator_has_next(iterator)){
    carp(CARP_WARNING, "No peptide source.");
    return FALSE;
  }

  // count the number of sources
  while(peptide_src_iterator_has_next(iterator)){
    peptide_src = peptide_src_iterator_next(iterator);

    ++num_src;
  }
  free_peptide_src_iterator(iterator);
  iterator = new_peptide_src_iterator(peptide);

  // write the peptide struct
  // for compatibility create an old-style struct to write in a block
  PRINT_PEPTIDE_T p;
  p.length = peptide->length;
  p.peptide_mass = peptide->peptide_mass;
  p.peptide_src = peptide->peptide_src;
  p.modified_seq = peptide->modified_seq;
  fwrite(&p, sizeof(PRINT_PEPTIDE_T), 1, file);

  // write peptide src count
  fwrite(&num_src, sizeof(int), 1, file);

  // for each peptide src, serialize
  while(peptide_src_iterator_has_next(iterator)){
    peptide_src = peptide_src_iterator_next(iterator);

    // serialize the peptide src
    serialize_peptide_src(peptide_src, file);
    
    ++num_src;
  }
  free_peptide_src_iterator(iterator);
  
  // write the number of MODIFIED_AA_T's to serialize
  int mod_seq_length = peptide->length + 1;
  if( peptide->modified_seq == NULL ){
    mod_seq_length = 0;
  }
  fwrite(&mod_seq_length, sizeof(int), 1, file);

  // write modified seq
  fwrite(peptide->modified_seq, sizeof(MODIFIED_AA_T), mod_seq_length, file);


  free(seq);
  
  // If a text file was given, print the peptide in ASCII.
  if (text_file != NULL) {
    fprintf(text_file, "%s %.5f\n", 
            get_peptide_modified_sequence_with_symbols(peptide),
	    get_peptide_peptide_mass(peptide));
  }

  return TRUE;
}

/**
 * \brief Read in a peptide from a tab-delimited file and return it.
 *
 * Parses the information for a peptide match from the search
 * file.  Allocates memory for the peptide and all of
 * its peptide_src's.  Requires a database so that the protein can be
 * set for each peptide_src.  Returns NULL if eof or if file format
 * appears incorrect.
 *
 * \returns A newly allocated peptide or NULL
 */
PEPTIDE_T* parse_peptide_tab_delimited(
  MatchFileReader& file, ///< the tab delimited peptide file -in
  DATABASE_T* database,///< the database containing the peptides -in
  BOOLEAN_T use_array  ///< should I use array peptide_src or link list -in  
  ) {

  // the new peptide to be given values in file
  PEPTIDE_T* peptide = allocate_peptide();

  //populate peptide struct.
  string string_sequence = file.getString(SEQUENCE_COL);
  // string length may include mod symbols and be longer than the peptide seq
  peptide->length = convert_to_mod_aa_seq(string_sequence.c_str(),
                                          &peptide->modified_seq);
  peptide->peptide_mass = file.getFloat(PEPTIDE_MASS_COL);
  
  if(!parse_peptide_src_tab_delimited(peptide, file, database, use_array)){
    carp(CARP_ERROR, "Failed to parse peptide src.");
    free(peptide);
    return NULL;
  };

  carp(CARP_DETAILED_DEBUG, "Finished parsing peptide.");

  return peptide;

}

/**
 * \brief Read in a peptide from a binary file and return it.
 *
 * Assumes the peptide has been written to file using
 * serialize_peptide().  Allocates memory for the peptide and all of
 * its peptide_src's.  Requires a database so that the protein can be
 * set for each peptide_src.  Returns NULL if eof or if file format
 * appears incorrect.
 *
 * \returns A newly allocated peptide or NULL
 */
PEPTIDE_T* parse_peptide(
  FILE* file, ///< the serialized peptide file -in
  DATABASE_T* database, ///< the database containing the peptides -in
  BOOLEAN_T use_array  ///< should I use array peptide_src or link list -in  
  )
{  
  carp(CARP_DETAILED_DEBUG, "Parsing peptide");
  
  // the new peptide to be given values in file
  PEPTIDE_T* peptide = allocate_peptide();
  
  // read peptide struct
  // for compatibility create an old-style struct to read in a block
  PRINT_PEPTIDE_T p;
  if(fread(&p, sizeof(PRINT_PEPTIDE_T), 1, file) != 1){
    carp(CARP_DETAILED_DEBUG, "Did not read peptide struct from file");
    // there is no peptide
    free(peptide);
    return NULL;
  }
  // copy values to the peptide  
  peptide->length = p.length;
  peptide->peptide_mass = p.peptide_mass;
  peptide->peptide_src = p.peptide_src ;
  peptide->modified_seq = p.modified_seq ;

  
  if(!parse_peptide_src(peptide, file, database, use_array)){
    carp(CARP_ERROR, "Failed to parse peptide src.");
    free(peptide);
    return NULL;
  };

  // TODO: write parse_peptide_modification()
  // read the length of the modified aa sequence
  int mod_seq_len = -1;
  size_t num_read = fread(&mod_seq_len, sizeof(int), 1, file);
  if( mod_seq_len < 0 || num_read != 1){
    carp(CARP_ERROR, "Did not read the correct length of modified sequence");
    peptide->modified_seq = NULL;
  }

  carp(CARP_DETAILED_DEBUG, "Length of modified sequence is %d", mod_seq_len);
  // allocate memory for and read in modified sequence
  if( mod_seq_len == 0 ){
    peptide->modified_seq = NULL;
  }else{
    assert( mod_seq_len - 1 == peptide->length );
    peptide->modified_seq = 
      (MODIFIED_AA_T*)mycalloc(mod_seq_len, sizeof(MODIFIED_AA_T));
    if( fread(peptide->modified_seq, sizeof(MODIFIED_AA_T), mod_seq_len, file)
        != (size_t)mod_seq_len){
      carp(CARP_ERROR, "Failed to read peptide modified sequence."); 
    }
  }
  
  carp(CARP_DETAILED_DEBUG, "Finished parsing peptide.");
  
  return peptide;
}

/**
 * \brief Read in a peptide from a binary file without reading its
 * peptide_src's.
 *
 * This parsing method is for callers that do not want memory
 * allcoated for every peptide in the file.  Caller allocates memory
 * once, parses peptide, checks values, and returns or keeps looking.
 * To get the peptide_src for this peptide, caller uses 
 * fseek(file, peptide_src_file_location, SEEK_SET);
 * parse_peptide_src(peptide, file, database, use_array);
 *
 * Assumes that the peptide has been written to file using
 * serialize_peptide().  
 * \returns TRUE if peptide was successfully parsed or FALSE if it was
 * not. 
 */
BOOLEAN_T parse_peptide_no_src(
  PEPTIDE_T* peptide, ///< memory already allocated for a peptide 
  FILE* file,       ///< file pointing to a serialized peptide
  long int* peptide_src_file_location)  // use to seek back to peptide_src
{
  if( peptide == NULL || file == NULL ){
    carp(CARP_ERROR, "Cannot parse (NULL) peptide from (NULL) file.");
    return FALSE;
  }

  // read peptide struct
  //  int read = fread(peptide, sizeof(PEPTIDE_T), 1, file);
  // for compatibility prior to storing modified seq
  PRINT_PEPTIDE_T p;
  int read = fread(&p, sizeof(PRINT_PEPTIDE_T), 1, file);
  if( read != 1 ){
    carp(CARP_DETAILED_DEBUG, "read did not find a peptide, returned %i", read);
    return FALSE;
  }
  peptide->length = p.length;
  peptide->peptide_mass = p.peptide_mass;
  peptide->peptide_src = p.peptide_src ;
  peptide->modified_seq = p.modified_seq ;

  carp(CARP_DETAILED_DEBUG, "read peptide len %i, mass %.2f", peptide->length, peptide->peptide_mass);

  // remember where the peptide_src begins
  *peptide_src_file_location = ftell(file);

  // read the number of peptide_src's
  int num_peptide_src = -1;
  read = fread(&num_peptide_src, sizeof(int), 1, file);
  if( num_peptide_src < 1 || read != 1){
    carp(CARP_DETAILED_DEBUG, "Num peptide src is %i and num read is %i", num_peptide_src, read);
    carp(CARP_ERROR, "Peptide must have at least one peptide src.");
    return FALSE;
  }

  // skip past all of the peptide_src's
  fseek(file, num_peptide_src * size_of_serialized_peptide_src(), SEEK_CUR);

  // read in any peptide modifications, starting with length
  int mod_seq_len = -1;
  size_t num_read = fread(&mod_seq_len, sizeof(int), 1, file);
  if( mod_seq_len < 0 || num_read != 1 ){
    carp(CARP_ERROR, "Did not read the correct length of modified sequence");
    peptide->modified_seq = NULL;
  }

  // read in modified sequence
  num_read = fread(peptide->modified_seq, sizeof(MODIFIED_AA_T), 
                   mod_seq_len, file);

  // we didn't ad any peptide_src, make sure it's still NULL
  peptide->peptide_src = NULL;

  return TRUE;
}

/* Public functions--Iterators */

/**
 * Instantiates a new residue_iterator from a peptide.
 * \returns a RESIDUE_ITERATOR_T object.
 */
RESIDUE_ITERATOR_T* new_residue_iterator(
  PEPTIDE_T* peptide ///< peptide sequence to iterate -in
  )
{
  RESIDUE_ITERATOR_T* residue_iterator =
    (RESIDUE_ITERATOR_T*)mycalloc(1, sizeof(RESIDUE_ITERATOR_T));
  
  residue_iterator->peptide =  peptide;
  residue_iterator->residue_idx = 0;
  residue_iterator->sequence = get_peptide_sequence(peptide);
  return residue_iterator;
}        

/**
 * Frees an allocated residue_iterator object.
 */
void free_residue_iterator(
  RESIDUE_ITERATOR_T* residue_iterator ///< free this object -in
  )
{
  free(residue_iterator->sequence);
  free(residue_iterator);
}

/**
 * The basic iterator functions.
 * \returns TRUE if there are additional residues to iterate over, FALSE if not.
 */
BOOLEAN_T residue_iterator_has_next(
  RESIDUE_ITERATOR_T* residue_iterator ///< the query iterator -in
  )
{
  return (residue_iterator->residue_idx < residue_iterator->peptide->length);
}

/**
 * \returns The next residue (a character) in the peptide.
 */
char residue_iterator_next(
  RESIDUE_ITERATOR_T* residue_iterator  ///< the query iterator -in
  )
{
  ++residue_iterator->residue_idx;
  return residue_iterator->sequence[residue_iterator->residue_idx - 1];
}


/**
 * Protein peptide association Iterator
 */

/**
 * Instantiates a new peptide_src_iterator from a peptide.
 * \returns a PEPTIDE_SRC_T object.
 */
PEPTIDE_SRC_ITERATOR_T* new_peptide_src_iterator(
  PEPTIDE_T* peptide ///< peptide's fields to iterate -in
  )
{
  PEPTIDE_SRC_ITERATOR_T* association_iterator =
    (PEPTIDE_SRC_ITERATOR_T*)mycalloc(1,sizeof(PEPTIDE_SRC_ITERATOR_T));
  association_iterator->peptide = peptide;
  association_iterator->current = peptide->peptide_src;
  return association_iterator;
}

/**
 * Frees an allocated peptide_src_iterator object.
 */
void free_peptide_src_iterator(
  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator ///< free this object -in
  )
{
  free(peptide_src_iterator);

}

/**
 * The basic iterator functions.
 * \returns TRUE if there are additional peptide_srcs to iterate over, FALSE if not.
 */
BOOLEAN_T peptide_src_iterator_has_next(
  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator///< the query iterator -in
  )
{
  return !(peptide_src_iterator->current == NULL);
}

/**
 * \returns The next peptide_srcs in the peptide.
 */
PEPTIDE_SRC_T* peptide_src_iterator_next(
  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator///< the query iterator -in
  )
{
  PEPTIDE_SRC_T* previous = peptide_src_iterator->current;
  if(peptide_src_iterator->current != NULL){
    peptide_src_iterator->current = 
      get_peptide_src_next_association(peptide_src_iterator->current);
  }
  else{
    carp(CARP_FATAL, "ERROR: no more peptide_srcs to iterate\n");
  }
  return previous;
}

/**
 * \brief Builds a comma delimited string listing the 
 * protein id(peptide start index) for the sources of 
 * a peptide
 *
 * \returns a string of the protein sources for this peptide
 */
string get_protein_ids_peptide_locations(PEPTIDE_T* peptide) {

  set<string> protein_ids_locations;

  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator = 
    new_peptide_src_iterator(peptide);

  std::ostringstream protein_field_stream;

  if (peptide_src_iterator_has_next(peptide_src_iterator)) {
    while(peptide_src_iterator_has_next(peptide_src_iterator)){
      PEPTIDE_SRC_T* peptide_src = peptide_src_iterator_next(peptide_src_iterator);
      Protein* protein = get_peptide_src_parent_protein(peptide_src);
      char* protein_id = protein->getId();
      int peptide_loc = get_peptide_src_start_idx(peptide_src);
      std::ostringstream protein_loc_stream;
      protein_loc_stream << protein_id << "(" << peptide_loc << ")";
      free(protein_id);
      protein_ids_locations.insert(protein_loc_stream.str());
    }
  }
  free(peptide_src_iterator);

  set<string>::iterator result_iter = protein_ids_locations.begin();
  string protein_field_string = *result_iter;

  while(++result_iter != protein_ids_locations.end()) {
    protein_field_string += "," + *result_iter;
  }

  return protein_field_string;
}

 
/**
 * \brief Builds a comma delimited string listing the protein ids
 * for the sources of a peptide.
 *
 * \returns a pointer to the string. Caller is responsible for freeing memeory.
 * If peptide has no sources returns NULL.
 */
char *get_protein_ids(PEPTIDE_T *peptide) {

  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator = 
    new_peptide_src_iterator(peptide);
  char *protein_field = NULL;

  if (peptide_src_iterator_has_next(peptide_src_iterator)) {

    // Peptide has at least one parent.
    
    PEPTIDE_SRC_T* peptide_src = peptide_src_iterator_next(peptide_src_iterator);
    Protein* protein = get_peptide_src_parent_protein(peptide_src);

    const int allocation_factor = 1;
    char* protein_id = protein->getId();
    size_t protein_id_len = strlen(protein_id);
    size_t protein_field_len = allocation_factor * protein_id_len + 1; // Buffer size
    protein_field = (char*)mymalloc(sizeof(char) * protein_field_len);
    size_t protein_field_free = protein_field_len;  // Remaining free buffer space
    char *protein_field_tail = protein_field;
    *protein_field = 0;

    // First protein id in list doesn't have leading ','

    strncpy(protein_field_tail, protein_id, protein_field_free);
    protein_field_tail += protein_id_len;
    protein_field_free -= protein_id_len;
    free(protein_id);

    // Following proteins in list have leading ','

    while(peptide_src_iterator_has_next(peptide_src_iterator)){
      peptide_src = peptide_src_iterator_next(peptide_src_iterator);
      protein = get_peptide_src_parent_protein(peptide_src);
      protein_id = protein->getId();
      protein_id_len = strlen(protein_id);

      // Allocate more memory if needed, allow space for comma and null
      if (protein_field_free < (protein_id_len + 2)) {
        size_t tail_offset = protein_field_tail - protein_field;
        protein_field = (char*)myrealloc(
          protein_field, 
          sizeof(char) * ((allocation_factor * (protein_id_len + 1)) + protein_field_len)
        );
        protein_field_len += allocation_factor * (protein_id_len + 1);
        protein_field_free += allocation_factor * (protein_id_len + 1);
        protein_field_tail = protein_field + tail_offset;
      }

      *protein_field_tail = ',';
      ++protein_field_tail;
      --protein_field_free;
      strncpy(protein_field_tail, protein_id, protein_field_free);
      protein_field_tail += protein_id_len;
      protein_field_free -= protein_id_len;
      free(protein_id);
    }
  }

  free(peptide_src_iterator);

  return protein_field;
}

/**
 * \brief Builds a comma delimited string listing the flanking amino acids
 * for the sources of a peptide.
 *
 * \returns a pointer to the string. Caller is responsible for freeing memeory.
 * If peptide has no sources returns NULL.
 */
char *get_flanking_aas(PEPTIDE_T *peptide) {

  PEPTIDE_SRC_ITERATOR_T* peptide_src_iterator = 
    new_peptide_src_iterator(peptide);
  char *flanking_field = NULL;

  if (peptide_src_iterator_has_next(peptide_src_iterator)) {

    // Peptide has at least one parent.
    
    PEPTIDE_SRC_T* peptide_src = peptide_src_iterator_next(peptide_src_iterator);
    Protein* protein = get_peptide_src_parent_protein(peptide_src);

    int protein_length = protein->getLength();
    int peptide_length = get_peptide_length(peptide);
    int start_index = get_peptide_src_start_idx(peptide_src);
    int end_index = start_index + peptide_length - 1;
    char* protein_seq = protein->getSequencePointer();
    const int allocation_factor = 1;
    size_t flanking_str_len = 2; // left and right flanking AA
    size_t flanking_field_len = allocation_factor * flanking_str_len + 1;
    flanking_field = (char*)mymalloc(sizeof(char) * flanking_field_len);
    size_t flanking_field_free = flanking_field_len;
    char *flanking_field_tail = flanking_field;

    // First flanking AA in list doesn't have leading ','
    
    // Flanking C
    *flanking_field_tail = (start_index > 1 ? protein_seq[start_index - 2] : '-');;
    ++flanking_field_tail;
    --flanking_field_free;
    // Flanking N
    *flanking_field_tail = (end_index < protein_length ? protein_seq[end_index] : '-');
    ++flanking_field_tail;
    --flanking_field_free;
    // Terminating null
    *flanking_field_tail = 0;

    flanking_str_len = 3; // leadinng ',', left and right flanking AA
    while(peptide_src_iterator_has_next(peptide_src_iterator)){

      peptide_src = peptide_src_iterator_next(peptide_src_iterator);
      protein = get_peptide_src_parent_protein(peptide_src);
      protein_length = protein->getLength();
      peptide_length = get_peptide_length(peptide);
      start_index = get_peptide_src_start_idx(peptide_src);
      end_index = start_index + peptide_length - 1;
      protein_seq = protein->getSequencePointer();

      // Allocate more memory if needed
      if (flanking_field_free < flanking_str_len + 1) {
        size_t tail_offset = flanking_field_tail - flanking_field;
        flanking_field = (char*)myrealloc(
          flanking_field,
          sizeof(char) * (allocation_factor * flanking_str_len + flanking_field_len)
        ); 
        flanking_field_len += (allocation_factor * flanking_str_len);
        flanking_field_free += (allocation_factor * flanking_str_len);
        flanking_field_tail = flanking_field + tail_offset;
      } 
      
      // Following flanking AA in list have leading ','
      *flanking_field_tail = ',';
      ++flanking_field_tail;
      --flanking_field_free;
      // Flanking C
      *flanking_field_tail = (start_index > 1 ? protein_seq[start_index - 2] : '-');
      ++flanking_field_tail;
      --flanking_field_free;
      // Flanking N
      *flanking_field_tail = (end_index < protein_length ? protein_seq[end_index] : '-');
      ++flanking_field_tail;
      --flanking_field_free;
      // Terminating NULL
      *flanking_field_tail = 0;
    }

  }

  free(peptide_src_iterator);

  return flanking_field;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */

