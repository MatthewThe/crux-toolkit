#ifndef BARISTA_H
#define BARISTA_H

#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include <assert.h>
#include <cstdio>
#include "CruxApplication.h"
#include "DataSet.h"
#include "ProtScores.h"
#include "PSMScores.h"
#include "PepScores.h"
#include "NeuralNet.h"
#include "SQTParser.h"
using namespace std;


class Barista : public CruxApplication
{
 public:
 Barista() : seed(0), selectionfdr(0.01), num_features(0), num_hu(3), mu(0.05), weightDecay(0.0), alpha(0.3),
    max_peptides(0), max_psms_in_prot(0), net_clones(0),nepochs(15), max_fdr(0), max_fdr_psm(0),
    max_fdr_pep(0), verbose(0){}
  ~Barista(){delete[] net_clones; net_clones = 0;}
  int set_command_line_options(int argc, char *argv[]);
  void setup_for_training(int trn_to_tst);
  int run();
  int run_tries();
  int run_tries_multi_task();
  void train_net(double selectionfdr, int interval);
  void train_net_multi_task(double selectionfdr, int interval);
  double get_protein_score(int protind);
  void calc_gradients(int protind, int label);
  double train_hinge(int protind, int label);
  double train_hinge_psm(int psmind, int label);

  int getOverFDRProt(ProtScores &set, NeuralNet &n, double fdr);
  double get_protein_score(int protind, NeuralNet &n);
  int getOverFDRProtMax(ProtScores &set, NeuralNet &n, double fdr);
  double get_protein_score_max(int protind, NeuralNet &n);
  double get_protein_score_parsimonious(int protind, NeuralNet &n);
  int getOverFDRProtParsimonious(ProtScores &set, NeuralNet &n, double fdr);

  void write_results_prot(string &out_dir, int fdr);
  void report_all_results();
  void get_pep_seq(string &pep, string &seq, string &n, string &c);
  void write_results_prot_xml(ofstream &os);
  void write_results_peptides_xml(ofstream &os);
  void write_results_psm_xml(ofstream &os);
  void report_all_results_xml();
  void report_prot_fdr_counts(vector<double> &qvals, ofstream &of);
  void report_psm_fdr_counts(vector<double> &qvals, ofstream &of);
  void report_pep_fdr_counts(vector<double> &qvals, ofstream &of);
  void report_all_fdr_counts();
  void clean_up();

  int getOverFDRPSM(PSMScores &set, NeuralNet &n, double fdr);
  double get_peptide_score(int pepind, NeuralNet &n);
  int getOverFDRPep(PepScores &set, NeuralNet &n, double fdr);

  inline void set_input_dir(string input_dir) {in_dir = input_dir; d.set_input_dir(input_dir);}
  inline void set_output_dir(string output_dir){out_dir = output_dir;}

  /* CruxApplication Methods */
  virtual int main(int argc, char** argv);
  virtual std::string getName();
  virtual std::string getDescription();


  double check_gradients_hinge_one_net(int protind, int label);
  double check_gradients_hinge_clones(int protind, int label);
 protected:
  int verbose;
  Dataset d;
  string in_dir;
  string out_dir;
  int seed;
  double selectionfdr;
  int nepochs;

  NeuralNet net;
  int num_hu;
  double mu;
  double weightDecay;
  double alpha;
  int num_features;

  int max_psms_in_prot;
  NeuralNet *net_clones;
  NeuralNet max_net_prot;
  int max_fdr;
  ProtScores trainset, thresholdset, testset;

  int max_peptides;
  vector<int> max_psm_inds;
  vector<double> max_psm_scores;
  
  //for parsimony counts
  vector<int> used_peptides;
  vector<int> pepind_to_max_psmind;

  PSMScores psmtrainset, psmtestset;
  NeuralNet max_net_psm;
  int max_fdr_psm;
  
  PepScores peptrainset,peptestset;
  NeuralNet max_net_pep;
  int max_fdr_pep;
  
};


#endif
