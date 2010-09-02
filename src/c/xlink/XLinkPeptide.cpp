#include "XLinkPeptide.h"
#include "modified_peptides_iterator.h"
#include "ion_series.h"
#include "ion.h"




#include <iostream>
#include <sstream>

using namespace std;

FLOAT_T XLinkPeptide::linker_mass_;
set<PEPTIDE_T*> XLinkPeptide::allocated_peptides_;

XLinkPeptide::XLinkPeptide() : MatchCandidate() {

}

XLinkPeptide::XLinkPeptide(XLinkablePeptide& peptideA,
			   XLinkablePeptide& peptideB,
			   int posA, int posB) : MatchCandidate() {
  //cout<<"XLinkPeptide"<<endl;
  linked_peptides_.push_back(peptideA);
  linked_peptides_.push_back(peptideB);
  link_pos_idx_.push_back(posA);
  link_pos_idx_.push_back(posB);
}

XLinkPeptide::XLinkPeptide(char* peptideA,
			   char* peptideB,
			   int posA, int posB) : MatchCandidate() {

  //cout <<"Creating peptideA"<<endl;
  XLinkablePeptide A(peptideA);
  linked_peptides_.push_back(A);
  //cout <<"Creating peptideB"<<endl;
  XLinkablePeptide B(peptideB);
  linked_peptides_.push_back(B);
  //cout <<"Adding links"<<endl;
  A.addLinkSite(posA);
  link_pos_idx_.push_back(0);
  B.addLinkSite(posB);
  link_pos_idx_.push_back(0);
}

XLinkPeptide::~XLinkPeptide() {
}

void XLinkPeptide::setLinkerMass(FLOAT_T linker_mass) {
  linker_mass_=linker_mass;
}

FLOAT_T XLinkPeptide::getLinkerMass() {
  return linker_mass_;
}

int XLinkPeptide::getLinkPos(int peptide_idx) {
  return linked_peptides_[peptide_idx].getLinkSite(link_pos_idx_[peptide_idx]);
}


void get_min_max_mass(
  FLOAT_T precursor_mz, 
  int charge, 
  FLOAT_T window,
  WINDOW_TYPE_T precursor_window_type,
  FLOAT_T& min_mass, 
  FLOAT_T& max_mass) {

  double mass = (precursor_mz - MASS_PROTON) * (double)charge;
  //cerr <<"mz: "
  //     <<precursor_mz
  //     <<" charge:"
  //     <<charge
  //     <<" mass:"<<mass
  //     <<" window:"<<window<<endl;
  if (precursor_window_type == WINDOW_MASS) {
    //cerr<<"WINDOW_MASS"<<endl;
    min_mass = mass - window;
    max_mass = mass + window;
  } else if (precursor_window_type == WINDOW_MZ) {
    //cerr<<"WINDOW_MZ"<<endl;
    double min_mz = precursor_mz - window;
    double max_mz = precursor_mz + window;
    min_mass = (min_mz - MASS_PROTON) * (double)charge;
    max_mass = (max_mz - MASS_PROTON) * (double)charge;
  } else if (precursor_window_type == WINDOW_PPM) {
    //cerr<<"WINDOW_PPM"<<endl;
    min_mass = mass / (1.0 + window * 1e-6);
    max_mass = mass / (1.0 - window * 1e-6);
  }
  
  //cerr<<"min:"<<min_mass<<" "<<"max: "<<max_mass<<endl;

}

void get_min_max_mass(
  FLOAT_T precursor_mz, 
  int charge, 
  BOOLEAN_T use_decoy_window,
  FLOAT_T& min_mass, 
  FLOAT_T& max_mass) {
  
  if (use_decoy_window) {
    get_min_max_mass(precursor_mz,
		     charge,
		     get_double_parameter("precursor-window-decoy"),
		     get_window_type_parameter("precursor-window-type-decoy"),
		     min_mass,
		     max_mass);
  } else {
    get_min_max_mass(precursor_mz,
		     charge,
		     get_double_parameter("precursor-window"),
		     get_window_type_parameter("precursor-window-type"),
		     min_mass,
		     max_mass);
  }
}


void XLinkPeptide::addLinkablePeptides(double min_mass, double max_mass,
			 INDEX_T* index, DATABASE_T* database,
			 PEPTIDE_MOD_T* peptide_mod, BOOLEAN_T is_decoy, 
			 XLinkBondMap& bondmap, 
			 vector<XLinkablePeptide>& linkable_peptides) {
  
  int max_missed_cleavages=get_int_parameter("max-missed-cleavages");

  //cerr <<"addLinkablePeptides(): start"<<endl;
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator =
    new_modified_peptides_iterator_from_mass_range(
      min_mass, 
      max_mass,
      peptide_mod, 
      is_decoy,
      index, 
      database);

  while (modified_peptides_iterator_has_next(peptide_iterator)) {
    PEPTIDE_T* peptide = modified_peptides_iterator_next(peptide_iterator);
    if (get_peptide_missed_cleavage_sites(peptide) <= max_missed_cleavages) {
      
      vector<int> link_sites;
      XLinkablePeptide::findLinkSites(peptide, bondmap, link_sites);

      if (link_sites.size() > 0) {
        XLinkablePeptide xlinkable_peptide(peptide, link_sites);
        linkable_peptides.push_back(xlinkable_peptide);
        XLink::addAllocatedPeptide(peptide);
      } else {
        free_peptide(peptide);
      }
    } else {
      free_peptide(peptide);
    }
  }
  free_modified_peptides_iterator(peptide_iterator);
  //cerr <<"addLinkablePeptides(): done."<<endl;
}

void XLinkPeptide::addCandidates(FLOAT_T precursor_mz, int charge,
			    XLinkBondMap& bondmap,
			    INDEX_T* index, DATABASE_T* database,
			    PEPTIDE_MOD_T** peptide_mods,
			    int num_peptide_mods,
			    MatchCandidateVector& candidates,
			    BOOLEAN_T use_decoy_window) {

  //get all linkable peptides up to mass-linkermass.

  //cerr <<"XLinkPeptide::addCandidates() : start."<<endl;

  vector<XLinkablePeptide> linkable_peptides;

  //find max, min mass.
  FLOAT_T max_mass;
  FLOAT_T min_mass;

  get_min_max_mass(precursor_mz, charge, use_decoy_window, min_mass, max_mass);

  
  //iterate through each modification, 
  //get all peptides that are linkable up to the max_mass-linker_mass.
  // assess scores after all pmods with x amods have been searched
  int cur_aa_mods = 0;

  // for each peptide mod
  for(int mod_idx=0; mod_idx<num_peptide_mods; mod_idx++){
    // get peptide mod
    PEPTIDE_MOD_T* peptide_mod = peptide_mods[mod_idx];

    // is it time to assess matches?
    int this_aa_mods = peptide_mod_get_num_aa_mods(peptide_mod);
    
    if( this_aa_mods > cur_aa_mods ){
      carp(CARP_INFO, "Finished searching %i mods", cur_aa_mods);
      /*
	TODO - do we need this?
      BOOLEAN_T passes = is_search_complete(match_collection, cur_aa_mods);
      if( passes ){
        carp(CARP_DETAILED_DEBUG, 
             "Ending search with %i modifications per peptide", cur_aa_mods);
        break;
      }// else, search with more mods
      */
      cur_aa_mods = this_aa_mods;
    }

    addLinkablePeptides(0, max_mass-linker_mass_, index, database,
			peptide_mod, FALSE, bondmap, linkable_peptides);
    
    
  }//next peptide mod

  if (linkable_peptides.size() == 0) {
    carp(CARP_INFO, "No linkable peptides found!");
    return;
  }

  //sort by increasing mass.
  sort(linkable_peptides.begin(),
       linkable_peptides.end(), 
       compareXLinkablePeptideMass);
  /*
  for (unsigned int idx=0;idx<linkable_peptides.size();idx++) {
    char* seq = get_peptide_sequence(linkable_peptides[idx].getPeptide());
    cerr <<linkable_peptides[idx].getMass()<<" "<<seq<<endl;
    free(seq);
  }
  */
  unsigned int first_idx = 0;
  unsigned int last_idx = linkable_peptides.size()-1;
  
  while(first_idx < linkable_peptides.size()-2) {
    //cerr<<"first:"<<first_idx;
    FLOAT_T first_mass = linkable_peptides[first_idx].getMass() + linker_mass_;
    //cerr<<" mass:"<<first_mass<<endl;
    last_idx = linkable_peptides.size()-1;
    //cerr<<"last:"<<last_idx<<endl;
    FLOAT_T current_mass = first_mass + 
      linkable_peptides[last_idx].getMass();
    //cerr<<"current_mass:"<<current_mass<<endl;
    while (first_idx < last_idx && current_mass > max_mass) {
      last_idx--;
      current_mass = first_mass + linkable_peptides[last_idx].getMass();
    }

    if (first_idx >= last_idx) break;  //we are done.

    while (first_idx < last_idx && current_mass >= min_mass) {
      //cerr<<"Adding links for peptides:"<<first_idx<<":"<<last_idx<<endl;
      XLinkablePeptide& pep1 = linkable_peptides[first_idx];
      XLinkablePeptide& pep2 = linkable_peptides[last_idx];
      //make sure they are not the same peptide, this may happen with modifications.
      if (get_peptide_sequence_pointer(pep1.getPeptide()) == 
	  get_peptide_sequence_pointer(pep2.getPeptide())) {
	last_idx--;
	continue;
      }

      //for every linkable site, generate the candidate if it is legal.
      for (unsigned int link1_idx=0;link1_idx < pep1.numLinkSites(); link1_idx++) {
	for (unsigned int link2_idx=0;link2_idx < pep2.numLinkSites();link2_idx++) {
	  //cerr<<"link1_idx:"<<link1_idx<<endl;
	  //cerr<<"link2_idx:"<<link2_idx<<endl;
	  int link1_site = pep1.getLinkSite(link1_idx);
	  //cerr<<"link1_site:"<<link1_site<<endl;
	  int link2_site = pep2.getLinkSite(link2_idx);
	  //cerr<<"link2_site:"<<link2_site<<endl;
	  //cerr<<"Testing link:"<<endl;
	  if (bondmap.canLink(pep1, pep2, link1_site, link2_site)) {
	    //create the candidate
	    MatchCandidate* newCandidate = 
	      new XLinkPeptide(pep1, pep2, link1_idx, link2_idx);
	    candidates.add(newCandidate);
	    //cerr<<"Adding candidate:"<<newCandidate -> getSequenceString()<<
	    //	" "<<newCandidate->getMass()<<" "<<min_mass<<" "<<max_mass<<endl;
	  }
	}
      } /* for link1_idx */
      last_idx--;
      current_mass = first_mass + linkable_peptides[last_idx].getMass();
    }

    //start with the next candidate.
    first_idx++;
  }

  //cerr <<"XLinkPeptide::addCandidates: done"<<endl;
}

MATCHCANDIDATE_TYPE_T XLinkPeptide::getCandidateType() {
  return XLINK_CANDIDATE;
}

string XLinkPeptide::getSequenceString() {
  string seq1 = linked_peptides_[0].getModifiedSequenceString();
  
  string seq2 = linked_peptides_[1].getModifiedSequenceString();

  ostringstream oss;
  oss << seq1 << "," << 
    seq2 << " (" <<
    (getLinkPos(0)+1) << "," <<
    (getLinkPos(1)+1) << ")";
  string svalue = oss.str();

  return svalue;
}

FLOAT_T XLinkPeptide::calcMass(MASS_TYPE_T mass_type) {
  return linked_peptides_[0].getMass(mass_type) + 
    linked_peptides_[1].getMass(mass_type) + 
    linker_mass_;
}

MatchCandidate* XLinkPeptide::shuffle() {

  XLinkPeptide* decoy = new XLinkPeptide();
  decoy->linked_peptides_.push_back(linked_peptides_[0].shuffle());
  decoy->linked_peptides_.push_back(linked_peptides_[1].shuffle());
  decoy->link_pos_idx_.push_back(link_pos_idx_[0]);
  decoy->link_pos_idx_.push_back(link_pos_idx_[1]);

  return (MatchCandidate*)decoy;


}

void XLinkPeptide::predictIons(ION_SERIES_T* ion_series, int charge) {
  //cerr << "Inside predictIons"<<endl;
  MASS_TYPE_T fragment_mass_type = get_mass_type_parameter("fragment-mass"); 
  //cerr << "Predicting "<<getSequenceString()<<" +"<<charge<<endl;
  //predict the ion_series of the first peptide.
  char* seq1 = linked_peptides_[0].getSequence();
  MODIFIED_AA_T* mod_seq1 = linked_peptides_[0].getModifiedSequence();

  //carp(CARP_INFO,"predictIONS %s",seq1);

  set_ion_series_charge(ion_series, charge);
  update_ion_series(ion_series, seq1, mod_seq1);
  predict_ions(ion_series);

  //iterate through all of the ions, if the ion contains a link, then
  //add the mass of peptide2 + linker_mass.

  ION_ITERATOR_T* ion_iter =
    new_ion_iterator(ion_series);
  while (ion_iterator_has_next(ion_iter)) {
    ION_T* ion = ion_iterator_next(ion_iter);

    unsigned int cleavage_idx = get_ion_cleavage_idx(ion);

    if (is_forward_ion_type(ion)) {
      if (cleavage_idx > (unsigned int)getLinkPos(0)) {
	FLOAT_T mass = get_ion_mass_from_mass_z(ion);
	mass += linked_peptides_[1].getMass(fragment_mass_type) + linker_mass_;
	set_ion_mass_z_from_mass(ion, mass);
      }
    } else {
      if (cleavage_idx >= (strlen(seq1) - (unsigned int)getLinkPos(0))) {
	FLOAT_T mass = get_ion_mass_from_mass_z(ion);
	mass += linked_peptides_[1].getMass(fragment_mass_type) + linker_mass_;
	set_ion_mass_z_from_mass(ion, mass);
      }
    }
  }

  free_ion_iterator(ion_iter);    


  //predict the ion_series of the second peptide.
  ION_CONSTRAINT_T* ion_constraint = 
    get_ion_series_ion_constraint(ion_series);
  ION_SERIES_T* ion_series2 = 
      new_ion_series_generic(ion_constraint, charge);
  
  char* seq2 = linked_peptides_[1].getSequence();

  //carp(CARP_INFO,"seq2:%s",seq2);

  MODIFIED_AA_T* mod_seq2 = 
    linked_peptides_[1].getModifiedSequence();
  set_ion_series_charge(ion_series2, charge);
  update_ion_series(ion_series2, seq2, mod_seq2);
  predict_ions(ion_series2);
  
  ion_iter = new_ion_iterator(ion_series2);
  
  //modify the necessary ions and add to the ion_series.
  while(ion_iterator_has_next(ion_iter)) {
    ION_T* ion = ion_iterator_next(ion_iter);

    unsigned int cleavage_idx = get_ion_cleavage_idx(ion);
    if (is_forward_ion_type(ion)) {
      if (cleavage_idx > (unsigned int)getLinkPos(1)) {
	FLOAT_T mass = get_ion_mass_from_mass_z(ion);
	mass += linked_peptides_[0].getMass(fragment_mass_type) + linker_mass_;
	set_ion_mass_z_from_mass(ion, mass);
      }
    } else {
      if (cleavage_idx >= (strlen(seq2)-(unsigned int)getLinkPos(1))) {
	FLOAT_T mass = get_ion_mass_from_mass_z(ion);
	mass += linked_peptides_[0].getMass(fragment_mass_type) + linker_mass_;
	set_ion_mass_z_from_mass(ion, mass);
      }
    }
    add_ion_to_ion_series(ion_series, ion);
  }
  //carp(CARP_INFO,"free(seq1)");
  free(seq1);
  //carp(CARP_INFO,"free(seq2)");
  free(seq2);
  //carp(CARP_INFO,"free(mod_seq1)");
  free(mod_seq1);
  //carp(CARP_INFO,"free(mod_seq2)");
  free(mod_seq2);
  
  free_ion_series(ion_series2, FALSE);

  free_ion_iterator(ion_iter);

  //carp(CARP_INFO,"Number of ions:%d",get_ion_series_num_ions(ion_series));
  
}

string XLinkPeptide::getIonSequence(ION_T* ion) {

  int peptide_idx = 0;

  string ion_sequence = get_ion_peptide_sequence(ion);

  if (ion_sequence == linked_peptides_[0].getSequence()) {
    peptide_idx = 0;
  } else {
    peptide_idx = 1;
  }

  unsigned int cleavage_idx = get_ion_cleavage_idx(ion);

  bool is_linked = false;

  if (is_forward_ion_type(ion)) {
    is_linked = (cleavage_idx > (unsigned int)getLinkPos(peptide_idx)); 
  } else {
    is_linked = (cleavage_idx >= (ion_sequence.length() - getLinkPos(peptide_idx)));
  }

  string subseq;
  if (is_forward_ion_type(ion)) {
    subseq = ion_sequence.substr(0, cleavage_idx);
  } else {
    subseq = ion_sequence.substr(ion_sequence.length() - cleavage_idx, ion_sequence.length());
  }

  if (!is_linked) {
    return subseq;
  } else {
    string ans;
    if (peptide_idx == 0) {
      char* seq2 = linked_peptides_[1].getSequence();
      ans = subseq + string(",") + string(seq2);
      free(seq2);
    } else {
      char* seq1 = linked_peptides_[0].getSequence();
      ans = string(seq1) + string(",") + subseq;
      free(seq1);
    }
    return ans;
  }
}

PEPTIDE_T* XLinkPeptide::getPeptide(int peptide_idx) {
  return linked_peptides_[peptide_idx].getPeptide();
}
