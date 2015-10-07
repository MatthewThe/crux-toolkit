#include "XLinkIonSeriesCache.h"

using namespace std;

vector<vector<IonSeries*> > XLinkIonSeriesCache::target_xlinkable_ion_series_;

vector<vector<IonSeries*> > XLinkIonSeriesCache::decoy_xlinkable_ion_series_;
vector<IonConstraint*> XLinkIonSeriesCache::xcorr_ion_constraint_;


IonSeries* XLinkIonSeriesCache::getXLinkablePeptideIonSeries(
  XLinkablePeptide& xpep,
  int charge
  ) {

  IonSeries* ans = NULL;
  int xpep_idx = xpep.getIndex();

  if (xpep_idx == -1) {
    //carp(CARP_DEBUG, "Unindexed xlinkable peptide. Returning NULL");
    return NULL;
  } else {

    bool decoy = xpep.isDecoy();
    //carp(CARP_INFO, "decoy %i pep_idx %i charge %i", decoy, xpep_idx, charge);
    vector<vector<IonSeries*> >* ion_cache = &target_xlinkable_ion_series_;
    if (decoy) {
      ion_cache = &decoy_xlinkable_ion_series_;
      //carp(CARP_INFO, "Getting decoy cache");
    }

    int charge_idx = charge-1;
    //carp(CARP_INFO, "getting pepidx:%i",xpep_idx);
    while(ion_cache->size() <= xpep_idx) {
      //carp(CARP_INFO, "Adding vector<IonSeries*>():%d", ion_cache->size());
      ion_cache->push_back(vector<IonSeries*>());
    }
  
    vector<IonSeries*>& level1 = (*ion_cache)[xpep_idx];

    //carp(CARP_INFO, "getting charge:%i", charge);
    while(level1.size() <= charge_idx) {
      //carp(CARP_INFO, "Adding charge:%d",(level1.size()+1));
      level1.push_back(NULL);
    }

    ans = level1[charge_idx];
    if (ans == NULL) {
      //carp(CARP_INFO, "creating ion series");
      ans = new IonSeries(getXCorrIonConstraint(charge), charge);
      ans->update(xpep.getSequence(), xpep.getModifiedSequencePtr());
      ans->predictIons();
      level1[charge_idx] = ans;
    } else {
      //carp(CARP_INFO, "using cached ions");
    }
  }
  return ans;

}

IonConstraint* XLinkIonSeriesCache::getXCorrIonConstraint(
  int charge
  ) {

  int charge_idx = charge - 1;

  while(xcorr_ion_constraint_.size() <= charge_idx) {
    xcorr_ion_constraint_.push_back(IonConstraint::newIonConstraintSmart(XCORR, (xcorr_ion_constraint_.size()+1)));
  }
  //carp(CARP_INFO, "returning ion_constraint");
  return(xcorr_ion_constraint_[charge_idx]);

}


 
void XLinkIonSeriesCache::finalize() {
  for (size_t xpep_idx = 0;xpep_idx <  target_xlinkable_ion_series_.size(); xpep_idx++) {

    vector<IonSeries*> &level1 = target_xlinkable_ion_series_[xpep_idx];
    for (size_t charge_idx=0;charge_idx < level1.size(); charge_idx++) {
      if (level1[charge_idx]) {
	IonSeries::freeIonSeries(level1[charge_idx]);
      }
    }
  }

  for (size_t xpep_idx = 0;xpep_idx < decoy_xlinkable_ion_series_.size(); xpep_idx++) {
    vector<IonSeries*>& level1 = decoy_xlinkable_ion_series_[xpep_idx];
    for (size_t charge_idx=0;charge_idx < level1.size(); charge_idx++) {
      if (level1[charge_idx]) {
	IonSeries::freeIonSeries(level1[charge_idx]);
      }
    }
  }

  for (size_t charge_idx=0;charge_idx < xcorr_ion_constraint_.size();charge_idx++) {
    IonConstraint::free(xcorr_ion_constraint_[charge_idx]);
  }
  
}