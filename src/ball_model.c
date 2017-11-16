#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>

#include "model.h"

#define NVERTS 4
model_vertex_3d_t ball_verts[NVERTS] = {

  {-0.5, -0.5, 0, 1},
  { 0.5, -0.5, 0, 1},
  { 0.5,  0.5, 0, 1},
  {-0.5,  0.5, 0, 1},    
	  
};

model_connection_t ball_lines[] = {
  {0,1},
  {1,2},
  {2,3},
  {3,0},

  {0,1},
  {1,2},
  {2,3},
  {3,0},

  {0,1},
  {1,2},
  {2,3},
  {3,0},


  {0,1},
  {1,2},
  {2,3},
  {3,0},  


};




int ball_model_get(model_t **ball_model) {
  static int tweaked = 0;


  if (ball_model) {

    *ball_model = malloc(sizeof(model_t));

    if (*ball_model == NULL) {
      printf("No mem\n");
      exit(1);
    }
    
    (*ball_model)->verts = malloc(sizeof(ball_verts));
    (*ball_model)->conns = malloc(sizeof(ball_lines));    
    
    
    memcpy((*ball_model)->verts, ball_verts, sizeof(ball_verts));
    memcpy((*ball_model)->conns, ball_lines, sizeof(ball_lines));

    (*ball_model)->n_verts =  sizeof(ball_verts) / sizeof(model_vertex_3d_t);
    (*ball_model)->n_conns =  sizeof(ball_lines) / sizeof(model_connection_t);    
  }


  return 0;

}


void ball_model_free(model_t **ball_model) {
  free((*ball_model)->verts);
  free((*ball_model)->conns);  
  free(*ball_model);
  *ball_model = NULL;
}



	
