/*************************************************************************//**
 * \file carp.cpp
 * \brief Object for representing a single protein.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "carp.h"
#include "util/crux-utils.h"
#include "parameter.h"
#include "util/utils.h"
#include "util/CarpStreamBuf.h"

using namespace std;

/**
 * Constants
 */
static int G_verbosity; 
static FILE* log_file = NULL;

HASH_T * messages_;
unsigned int hash_size_ = 1000;

CarpStreamBuf* carp_stream_buffer = NULL;
streambuf* cerr_buf = NULL;

void carp_initialize() {
  /* Re-route stderr to log file. */
  carp_stream_buffer = new CarpStreamBuf();
  cerr_buf = std::cerr.rdbuf();
  std::cerr.rdbuf(carp_stream_buffer);
}

void carp_finalize() {
  /* Recover stderr */
  if (cerr_buf) {
    std::cerr.rdbuf(cerr_buf);
  }
  if (carp_stream_buffer) {
    delete carp_stream_buffer;
  }
}

void set_verbosity_level(int verbosity){
  G_verbosity = verbosity;
}

int get_verbosity_level(void){
  return G_verbosity;
}

/**
 * Open log file for carp messages.
 *
 * Parameters must have been processed before calling this function.
 */
void open_log_file(string log_file_name) {
  string output_dir = get_string_parameter("output-dir");
  bool overwrite = get_boolean_parameter("overwrite");
  log_file_name = prefix_fileroot_to_name(log_file_name);
  log_file = create_file_in_path(log_file_name, output_dir.c_str(), overwrite);

}

/**
 * Print command line to log file.
 *
 * Parameters must have been processed before calling this function.
 */
void log_command_line(int argc, char *argv[]) {
  // Command line arguments were shifted, shift back.
  ++argc;
  --argv;
  if (log_file != NULL) {
    fprintf(log_file, "COMMAND LINE: ");
    int i = 0;
    for (i = 0; i < argc; ++i) {
      fprintf(log_file, "%s%c", argv[i], i < (argc - 1) ? ' ' : '\n');
    }
  }
}

static void carp_print(const char *string) {
  fprintf(stderr, "%s", string);
  if (log_file != NULL) {
    fprintf(log_file, "%s", string);
  }
}

/**
 * Print message to log file.
 *
 * Print severity level and message to log file.
 * The term 'carp' is used because 'log' is already used 
 * by the math library. 
 *
 * Verbosity of CARP_FATAL will cause the 
 * program to exit with status code 1.
 *
 */
void carp( int verbosity, const char* format, ...) {
  if (verbosity <= G_verbosity){
    va_list  argp;

    if (verbosity == CARP_WARNING){
      carp_print("WARNING: ");
    }
    else if (verbosity == CARP_ERROR){
      carp_print("ERROR: ");
    }
    else if (verbosity == CARP_FATAL){
      carp_print("FATAL: ");
    }
    else if (verbosity == CARP_INFO){
      carp_print("INFO: ");
    }
    else if (verbosity == CARP_DETAILED_INFO){
      carp_print("DETAILED INFO: ");
    }
    else if (verbosity == CARP_DEBUG){
      carp_print("DEBUG: ");
    }
    else if (verbosity == CARP_DETAILED_DEBUG){
      carp_print("DETAILED DEBUG: ");
    } 
    else {
      carp_print("UNKNOWN: ");
    }

    va_start(argp, format);
    vfprintf(stderr, format, argp);
    va_end(argp);
    if (log_file != NULL) { 
      va_start(argp, format); //BF: added to fix segfault
      vfprintf(log_file, format, argp);
      va_end(argp);
    }
    carp_print("\n");
    fflush(stderr);
    if (log_file != NULL) {
      fflush(log_file);
    }
  } 
  if (verbosity == CARP_FATAL) {
    // Fatal carps cause the program to exit
#ifdef DEBUG
    abort(); // Dump core in DEBUG mode.  Use 'make CXXFLAGS=-DDEBUG"'
#else
    exit(1);
#endif
  }
}

void carp ( int verbosity, string& msg) {

  string carp_msg = msg;
  carp(verbosity, "%s", carp_msg.c_str());
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */

