#include "crux_cuda.cu.h"

#include <stdio.h>

#define MAX_XCORR_OFFSET 50
#define NUM 2048

int h_min(int a, int b) {
   return (a<b) ? a : b;
}

int h_max(int a, int b) {
   return (a>b) ? a : b;
}


void do_cross_correlation_obs(float* in, float* ans, int n) {
  int idx;
  int cur_idx;
  for (idx=0;idx<n;idx++) {
    int min_idx = h_max(0, idx - MAX_XCORR_OFFSET);
    int max_idx = h_min(n-1, idx + MAX_XCORR_OFFSET);
    ans[idx] = in[idx];
    for (cur_idx = min_idx ; cur_idx <= max_idx; cur_idx++)
	 ans[idx] -= (in[cur_idx] / (MAX_XCORR_OFFSET * 2.0));
  }
}


int main(int argc, char **argv) {
  float h_values[NUM];
  float h_ans[NUM];
  int i;
  for (i=0;i< NUM;i++) {
    h_values[i] = i;
  }
  
  cross_correlation_obs(h_values, h_ans, NUM);



  float h_ans2[NUM];
  do_cross_correlation_obs(h_values, h_ans2, NUM);

  float rms_error = 0.0;
  int idx;
  for (idx = 0;idx < NUM;idx++) {
    printf ("orig: %f  host: %f device: %f\n",h_values[idx], h_ans2[idx],h_ans[idx]);
    rms_error += (h_ans[idx] - h_ans2[idx]) * (h_ans[idx] - h_ans2[idx])/(float)NUM;
  }

  printf("rms_error:%f\n",rms_error);
}
