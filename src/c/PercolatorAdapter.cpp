/**
 * \file PercolatorAdapter.cpp
 * $Revision$
 * \brief Converts Percolator result objects to Crux result objects.
 */

#include "PercolatorAdapter.h"
#include "DelimitedFile.h"

#include<map>
#include<string>

using namespace std;

/**
 * Constructor for PercolatorAdapter. 
 */
PercolatorAdapter::PercolatorAdapter() : Caller() {
  carp(CARP_DEBUG, "PercolatorAdapter::PercolatorAdapter");
  collection_ = new ProteinMatchCollection();
  decoy_collection_ = new ProteinMatchCollection();
}

/**
 * Destructor for PercolatorAdapter
 */
PercolatorAdapter::~PercolatorAdapter() {
  carp(CARP_DEBUG, "PercolatorAdapter::~PercolatorAdapter");

  // delete match collections created by this adapter
  int collectionsDeleted = 0;
  for (vector<MatchCollection*>::iterator iter = match_collections_made_.begin();
       iter != match_collections_made_.end();
       ++iter) {
    delete *iter;
    ++collectionsDeleted;
  }
  // delete proteins created by this adapter
  int proteinsDeleted = 0;
  for (vector<PostProcessProtein*>::iterator iter = proteins_made_.begin();
       iter != proteins_made_.end();
       ++iter) {
    delete *iter;
    ++proteinsDeleted;
  }

  //delete collection_;  //TODO find out why this crashes
  carp(CARP_DEBUG, "PercolatorAdapter::~PercolatorAdapter - done. %d "
       "MatchCollections deleted.", collectionsDeleted);
}

/**
  * Calls Percolator's overridden Caller::writeXML_PSMs() and then
  * Collects all of the psm results
  */
void PercolatorAdapter::writeXML_PSMs() {
  carp(CARP_DEBUG, "PercolatorAdapter::writeXML_PSMs");
  Caller::writeXML_PSMs();
  addPsmScores();  
}

/**
 * Calls Percolator's overridden Caller::writeXML_Peptides() and then
 * Collects all of the peptide results from the fullset
 */
void PercolatorAdapter::writeXML_Peptides() {
  carp(CARP_DEBUG, "PercolatorAdapter::writeXML_Peptides");
  Caller::writeXML_Peptides();
  addPeptideScores();
}

/**
 * Calls Percolator's overriden Caller::writeXMLProteins() and then
 * Collects all of the protein results from fido
 */  
void PercolatorAdapter::writeXML_Proteins() {
  carp(CARP_DEBUG, "PercolatorAdapter::writeXML_Proteins");
  Caller::writeXML_Proteins();
  addProteinScores();
}

/**
 * Converts a set of Percolator scores into a Crux MatchCollection
 */
void PercolatorAdapter::psmScoresToMatchCollection(
  MatchCollection** match_collection,  ///< out parameter for targets
  MatchCollection** decoy_match_collection ///< out parameter for decoys
) {

  // Create new MatchCollection object that will be the converted Percolator Scores
  *match_collection = new MatchCollection();
  match_collections_made_.push_back(*match_collection);
  *decoy_match_collection = new MatchCollection();
  match_collections_made_.push_back(*decoy_match_collection);

  // Find out which feature is lnNumSP and get indices of charge state features
  Normalizer* normalizer = Normalizer::getNormalizer();
  double* normSubAll = normalizer->getSub();
  double* normDivAll = normalizer->getDiv();
  double normSub, normDiv;
  FeatureNames& features = DataSet::getFeatureNames();
  string featureNames = features.getFeatureNames();
  vector<string> featureTokens;
  DelimitedFile::tokenize(featureNames, featureTokens);
  int lnNumSPIndex = -1;
  map<int, int> chargeStates; // index of feature -> charge
  for (int i = 0; i < featureTokens.size(); ++i) {
    string& featureName = featureTokens[i];
    transform(featureName.begin(), featureName.end(),
              featureName.begin(), ::tolower);
    if (featureName == "lnnumsp") {
      lnNumSPIndex = i;
      normSub = normSubAll[i];
      normDiv = normDivAll[i];
    } else if (featureName.find("charge") == 0) {
      size_t chargeNum = atoi(featureName.substr(6).c_str());
      chargeStates[i] = chargeNum;
    }
  }

  // Iterate over each ScoreHolder in Scores object
  for (
    vector<ScoreHolder>::iterator score_itr = fullset.begin();
    score_itr != fullset.end();
    score_itr++
    ) {

    bool is_decoy = score_itr->isDecoy();

    PSMDescription* psm = score_itr->pPSM;
    // Try to look up charge state in map
    int charge_state = -1;
    for (map<int, int>::const_iterator i = chargeStates.begin();
         i != chargeStates.end();
         ++i) {
      if (psm->features[i->first] > 0) {
        charge_state = i->second;
        break;
      }
    }
    if (charge_state == -1) {
      // Failed, try to parse charge state from id
      charge_state = parseChargeState(psm->id);
      if (charge_state == -1) {
        carp_once(CARP_WARNING, "Could not determine charge state of PSM");
      }
    }
    Crux::Peptide* peptide = extractPeptide(psm, charge_state, is_decoy);

    SpectrumZState zState;
    zState.setSinglyChargedMass(psm->expMass, charge_state);
    // calcMass/expMass = singly charged mass
    Crux::Spectrum* spectrum = new Crux::Spectrum(
      psm->scan, psm->scan, zState.getMZ(), vector<int>(1, charge_state), ""
    );

    Crux::Match* match = new Crux::Match(peptide, spectrum, zState, is_decoy);
    match->setScore(PERCOLATOR_SCORE, score_itr->score);
    match->setScore(PERCOLATOR_QVALUE, psm->q);
    match->setScore(PERCOLATOR_PEP, psm->pep);

    // Get matches/spectrum
    if (lnNumSPIndex < 0) {
      match->setLnExperimentSize(-1);
    } else {
      // Normalized lnNumSP
      double lnNumSP = psm->getFeatures()[lnNumSPIndex];
      // Unnormalize
      lnNumSP = lnNumSP * normDiv + normSub;
      match->setLnExperimentSize(lnNumSP);
    }

    if (!is_decoy) {
      (*match_collection)->addMatch(match);
    } else {
      (*decoy_match_collection)->addMatch(match);
    }
    match->setPostProcess(true); // so spectra get deleted when match does
    Crux::Match::freeMatch(match); // so match gets deleted when collection does
  }

  (*match_collection)->forceScoredBy(PERCOLATOR_SCORE);
  (*match_collection)->forceScoredBy(PERCOLATOR_QVALUE);
  (*match_collection)->forceScoredBy(PERCOLATOR_PEP);
  (*match_collection)->populateMatchRank(PERCOLATOR_SCORE);

  (*decoy_match_collection)->forceScoredBy(PERCOLATOR_SCORE);
  (*decoy_match_collection)->forceScoredBy(PERCOLATOR_QVALUE);
  (*decoy_match_collection)->forceScoredBy(PERCOLATOR_PEP);
  (*decoy_match_collection)->populateMatchRank(PERCOLATOR_SCORE);

  // sort by q-value
  (*match_collection)->sort(PERCOLATOR_QVALUE);
  (*decoy_match_collection)->sort(PERCOLATOR_QVALUE);

}

/**
 * Adds PSM scores from Percolator objects into a ProteinMatchCollection
 */
void PercolatorAdapter::addPsmScores() {
  MatchCollection* targets;
  MatchCollection* decoys;
  psmScoresToMatchCollection(&targets, &decoys);
  collection_->addMatches(targets);
  decoy_collection_->addMatches(decoys);
}

/**
 * Adds protein scores from Percolator objects into a ProteinMatchCollection
 */
void PercolatorAdapter::addProteinScores() {

  vector<ProteinMatch*> matches;
  vector<ProteinMatch*> decoy_matches;
  map<const string,Protein> protein_scores = protEstimator->getProteins();
  
  for (
    map<const string,Protein>::iterator score_iter = protein_scores.begin();
    score_iter != protein_scores.end();
    score_iter++) {
    
    // Set scores
    ProteinMatch* protein_match;
    if (!score_iter->second.getIsDecoy()) {
      protein_match = collection_->getProteinMatch(score_iter->second.getName());
      matches.push_back(protein_match);
    } else {
      protein_match = decoy_collection_->getProteinMatch(score_iter->second.getName());
      decoy_matches.push_back(protein_match);
    }
      protein_match->setScore(PERCOLATOR_SCORE, -log(score_iter->second.getP()));
      protein_match->setScore(PERCOLATOR_QVALUE, score_iter->second.getQ());
      protein_match->setScore(PERCOLATOR_PEP, score_iter->second.getPEP());
  }

  // set percolator score ranks
  std::sort(matches.begin(), matches.end(),
            PercolatorAdapter::comparePercolatorScores);
  std::sort(decoy_matches.begin(), decoy_matches.end(),
            PercolatorAdapter::comparePercolatorScores);
  int cur_rank = 0;
  for (vector<ProteinMatch*>::iterator iter = matches.begin();
       iter != matches.end();
       ++iter) {
    ProteinMatch* match = *iter;
    match->setRank(PERCOLATOR_SCORE, ++cur_rank);
  }
  cur_rank = 0;
  for (vector<ProteinMatch*>::iterator iter = decoy_matches.begin();
       iter != decoy_matches.end();
       ++iter) {
    ProteinMatch* match = *iter;
    match->setRank(PERCOLATOR_SCORE, ++cur_rank);
  }

}

/**
 * Adds peptide scores from Percolator objects into a ProteinMatchCollection
 */
void PercolatorAdapter::addPeptideScores() {

  carp(CARP_DEBUG, "Setting peptide scores");

  // Iterate over each ScoreHolder in Scores object
  for (
    vector<ScoreHolder>::iterator score_itr = fullset.begin();
    score_itr != fullset.end();
    score_itr++
    ) {

    PSMDescription* psm = score_itr->pPSM;
    string sequence;
    FLOAT_T peptide_mass;
    MODIFIED_AA_T* mod_seq = getModifiedAASequence(psm, sequence, peptide_mass);

    // Set scores
    PeptideMatch* peptide_match;
    if (!score_itr->isDecoy()) {
      peptide_match = collection_->getPeptideMatch(mod_seq);
    } else {
      peptide_match = decoy_collection_->getPeptideMatch(mod_seq);
    }
    if (peptide_match == NULL) {
      carp(CARP_FATAL, "Cannot find peptide %s %i",
                       psm->getPeptide().c_str(), score_itr->isDecoy());
    }
    peptide_match->setScore(PERCOLATOR_SCORE, score_itr->score);
    peptide_match->setScore(PERCOLATOR_QVALUE, psm->q);
    peptide_match->setScore(PERCOLATOR_PEP, psm->pep);

    free(mod_seq);

  }

}
  
/*
 *\returns the ProteinMatchCollection, to be called after Caller::run() is finished
 */
ProteinMatchCollection* PercolatorAdapter::getProteinMatchCollection() {
  return collection_;
}

/*
 *\returns the decoy ProteinMatchCollection, to be called after Caller::run() is finished
 */
ProteinMatchCollection* PercolatorAdapter::getDecoyProteinMatchCollection() {
  return decoy_collection_;
}

/**
 * Given a Percolator psm_id in the form ".*_([0-9]+)_[^_]*",
 * find the charge state (matching group)
 */
int PercolatorAdapter::parseChargeState(
  string psm_id ///< psm to parse charge state from
) {
  size_t charge_begin, charge_end;

  charge_end = psm_id.rfind("_");
  if (charge_end < 0) {
    return -1;
  }

  charge_begin = psm_id.rfind("_", charge_end - 1) + 1;
  if (charge_begin < 0) {
    return -1;
  }

  string charge_str = psm_id.substr(charge_begin, charge_end - charge_begin);
  return atoi(charge_str.c_str());
}

/**
 * Compares two AbstractMatches by Percolator score
 */
bool PercolatorAdapter::comparePercolatorScores(
  AbstractMatch* lhs, ///< first match with Percolator scores to compare
  AbstractMatch* rhs ///< second match with Percolator scores to compare
) {
  if (!lhs->hasScore(PERCOLATOR_SCORE) || !rhs->hasScore(PERCOLATOR_SCORE)) {
    carp(CARP_FATAL, "Could not compare matches by Percolator score.");
  }
  return lhs->getScore(PERCOLATOR_SCORE) < rhs->getScore(PERCOLATOR_SCORE);
}

/**
* \returns a Crux peptide from the PSM
*/
Crux::Peptide* PercolatorAdapter::extractPeptide(
  PSMDescription* psm, ///< psm
  int charge_state, ///< charge state
  bool is_decoy ///< is psm a decoy?
  ) {

  string seq;
  FLOAT_T peptide_mass;
  
  MODIFIED_AA_T* mod_seq = getModifiedAASequence(psm, seq, peptide_mass);

  string full_peptide(psm->getFullPeptide());
  string n_term = "";
  string c_term = "";
  if (!full_peptide.empty()) {
    n_term += full_peptide[0];
    c_term += full_peptide[full_peptide.length() - 1];
  }

  PostProcessProtein* parent_protein = new PostProcessProtein();
  proteins_made_.push_back(parent_protein);
  parent_protein->setId((*psm->proteinIds.begin()).c_str());
  int start_idx = parent_protein->findStart(seq, n_term, c_term);

  Crux::Peptide* peptide = new Crux::Peptide(seq.length(), peptide_mass, parent_protein, start_idx);

  // add other proteins
  bool skip_one = true;
  for (set<string>::iterator iter = psm->proteinIds.begin();
       iter != psm->proteinIds.end();
       ++iter) {
    if (skip_one) {
      skip_one = false;
      continue;
    }
    PostProcessProtein* secondary_protein = new PostProcessProtein();
    proteins_made_.push_back(secondary_protein);
    secondary_protein->setId(iter->c_str());
    int secondary_idx = secondary_protein->findStart(seq, n_term, c_term);
    peptide->addPeptideSrc(
      new PeptideSrc(NON_SPECIFIC_DIGEST, secondary_protein, secondary_idx)
    );
  }

  peptide->setModifiedAASequence(mod_seq, is_decoy);

  free(mod_seq);
  return peptide;
}

/**
 * \returns the modified and unmodified peptide sequence
 * for the psm
 */
MODIFIED_AA_T* PercolatorAdapter::getModifiedAASequence(
  PSMDescription* psm, ///< psm -in
  string& seq, ///< sequence -out
  FLOAT_T& peptide_mass ///< calculated mass of peptide with modifications -out
  ) {

  std::stringstream ss_seq;
  string perc_seq = psm->getPeptide();
  size_t perc_seq_len = perc_seq.length();
  if (perc_seq_len >= 5 &&
      perc_seq[1] == '.' && perc_seq[perc_seq_len - 2] == '.') {
    // Trim off flanking AA if they exist
    perc_seq = perc_seq.substr(2, perc_seq_len - 4);
    perc_seq_len -= 4;
  }
  peptide_mass = 0.0;
  
  if (perc_seq.find("UNIMOD") != string::npos) {
    carp(CARP_FATAL, 
	 "UNIMOD modifications currently not supported:%s", 
	 perc_seq.c_str());
  }


  vector<pair<int, const AA_MOD_T*> > mod_locations_types;
  size_t count = 0;
  for (size_t seq_idx = 0; seq_idx < perc_seq_len; seq_idx++) {
    if (perc_seq.at(seq_idx) == '[') {
      //modification found.
      size_t begin_idx = seq_idx+1;
      size_t end_idx = perc_seq.find(']', begin_idx);
      int mod_location = count-1;
      FLOAT_T delta_mass;

      from_string(delta_mass, perc_seq.substr(begin_idx, end_idx - begin_idx));
      carp(CARP_DEBUG,"seq:%s, i:%i m:%f", perc_seq.c_str(), seq_idx, delta_mass);
      peptide_mass += delta_mass;
      const AA_MOD_T* mod = get_aa_mod_from_mass(delta_mass);
      if (mod == NULL) {
	carp(CARP_FATAL, "Mod not found!");
      }

      mod_locations_types.push_back(pair<int, const AA_MOD_T*>(mod_location, mod));
      seq_idx = end_idx;
    } else {
      ss_seq << perc_seq.at(seq_idx);
      count++;
    }
  }
  seq = ss_seq.str();
  peptide_mass += Crux::Peptide::calcSequenceMass(seq.c_str(),
                  get_mass_type_parameter("isotopic-mass"));

  MODIFIED_AA_T* mod_seq;
  convert_to_mod_aa_seq(seq.c_str(), &mod_seq);
  for (size_t mod_idx = 0 ; mod_idx < mod_locations_types.size(); mod_idx++) {
    size_t seq_idx = mod_locations_types[mod_idx].first;
    const AA_MOD_T* mod = mod_locations_types[mod_idx].second;

    modify_aa(mod_seq+seq_idx, (AA_MOD_T*)mod);
  }
  return mod_seq;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
