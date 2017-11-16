#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>

#include "model.h"

#define BUF_SIZE 4096


typedef struct draw_buffer_s {
  char * buffer;
  int buf_size;
  int samples_per_frame;
  int frame_rate;
} draw_buffer_t;


// 30000 just in case of artifacts at loud noises? (a bit less that 2^15)
#define MAX_X  30000.0
// 16000 to get as much usable screen as possible, due to step sizes in volts/div on scope
#define MAX_Y  16000.0

#define NVERTS 60
model_vertex_3d_t invader_verts[NVERTS] = {

  {0, 0, 0, 1},

  {1, 0, 0, 1},
  {1, 1, 0, 1},
  {2, 1, 0, 1},
  {2, 2, 0, 1},	  
  {3, 2, 0, 1},
  {3, 3, 0, 1},
  {2, 3, 0, 1},
  {2, 4, 0, 1},
  {3, 4, 0, 1},
  {3, 3, 0, 1},
  {4, 3, 0, 1},
  {4, 2, 0, 1},
  {7, 2, 0, 1},
  {7, 3, 0, 1},
  {8, 3, 0, 1},
  {8, 3, 0, 1},
  {8, 4, 0, 1},
  {9, 4, 0, 1},
  {9, 3, 0, 1},
  {8, 3, 0, 1},
  {8, 2, 0, 1},	  	  	  	  	  	  
  {9, 2, 0, 1},
  {9, 1, 0, 1},
  {10, 1, 0, 1},
  {10, 0, 0, 1},
  {11, 0, 0, 1},
  {11, -3, 0, 1},
  {10, -3, 0, 1},
  {10, -1, 0, 1},
  {9, -1, 0, 1},
  {9, -3, 0, 1},
  {8, -3, 0, 1},
  {8, -4, 0, 1},
  {6, -4, 0, 1},
  {6, -3, 0, 1},
  {6, -3, 0, 1},
  {8, -3, 0, 1},
  {8, -2, 0, 1},
  {3, -2, 0, 1},
  {3, -3, 0, 1},
  {3, -3, 0, 1},
  {5, -3, 0, 1},
  {5, -4, 0, 1},
  {3, -4, 0, 1},
  {3, -3, 0, 1},
  {2, -3, 0, 1},
  {2, -3, 0, 1},
  {2, -1, 0, 1},
  {1, -1, 0, 1},
  {1, -3, 0, 1},
  {0, -3, 0, 1},

  /* eyes */
  {3, 0, 0, 1},
  {3, 1, 0, 1},
  {4, 1, 0, 1},
  {4, 0, 0, 1},

  /* eyes */
  {7, 0, 0, 1},
  {7, 1, 0, 1},
  {8, 1, 0, 1},
  {8, 0, 0, 1},	  	  	  	  	  
	  
};

model_connection_t invader_lines[] = {
  {0,1},
  {1,2},
  {2,3},
  {3,4},
  {4,5},
  {5,6},
  {6,7},
  {7,8},
  {8,9},
  {9,10},
  {10,11},
  {11,12},
  {12,13},
  {13,14},
  {14,15},
  {15,16},
  {16,17},
  {17,18},
  {18,19},
  {19,20},
  {20,21},
  {21,22},
  {22,23},
  {23,24},
  {24,25},
  {25,26},
  {26,27},
  {27,28},
  {28,29},
  {29,20},
  {30,31},
  {31,32},	  	  	  	  	  
  {32,33},
  {33,34},
  {34,35},
  {35,36},
  {36,37},
  {37,38},
  {38,39},
  {39,40},
  {40,41},
  {41,42},	  	  	  	  	  
  {42,43},
  {43,44},
  {44,45},
  {45,46},
  {46,47},
  {47,48},
  {48,49},
  {49,50},
  {50,51},	  	  	  	  
  {51, 0},

  /*	
	// Leave the eyes out.
	
	{52, 53},
	{53, 54},
	{54, 55},
	{55, 52},
	
	{56, 57},
	{57, 58},
	{58, 59},
	{59, 56},	  	  	  	  
  */
};




int invader_model_get(model_t **invader_model) {
  static int tweaked = 0;

  if (!tweaked) {
    int i;
    for (i = 0; i < NVERTS; i++) {
      invader_verts[i].x -= 5.5;
      invader_verts[i].x += 0.5;	    
      invader_verts[i].x /= 20;
      invader_verts[i].y /= 20;
      invader_verts[i].z /= 20;	      
    }

    tweaked++;
  }
  
  if (invader_model) {

    *invader_model = malloc(sizeof(model_t));

    if (*invader_model == NULL) {
      printf("No mem\n");
      exit(1);
    }
    
    (*invader_model)->verts = malloc(sizeof(invader_verts));
    (*invader_model)->conns = malloc(sizeof(invader_lines));    
    
    
    memcpy((*invader_model)->verts, invader_verts, sizeof(invader_verts));
    memcpy((*invader_model)->conns, invader_lines, sizeof(invader_lines));

    (*invader_model)->n_verts =  sizeof(invader_verts) / sizeof(model_vertex_3d_t);
    (*invader_model)->n_conns =  sizeof(invader_lines) / sizeof(model_connection_t);    
  }


  return 0;

}


void invader_model_free(model_t **invader_model) {
  free((*invader_model)->verts);
  free((*invader_model)->conns);  
  free(*invader_model);
  *invader_model = NULL;
}



	
