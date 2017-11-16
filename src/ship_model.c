#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>

#include "model.h"

#define NVERTS 3
model_vertex_3d_t ship_verts[NVERTS] = {

  {-0.5, -0.5, 0, 1},
  {0.5, -0.5, 0, 1},
  {0, 0.5, 0, 1},    
	  
};

model_connection_t ship_lines[] = {
  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},

  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},
  {0,1},
  {1,2},
  {2,0},  
};




int ship_model_get(model_t **ship_model) {
  static int tweaked = 0;


  if (ship_model) {

    *ship_model = malloc(sizeof(model_t));

    if (*ship_model == NULL) {
      printf("No mem\n");
      exit(1);
    }
    
    (*ship_model)->verts = malloc(sizeof(ship_verts));
    (*ship_model)->conns = malloc(sizeof(ship_lines));    
    
    
    memcpy((*ship_model)->verts, ship_verts, sizeof(ship_verts));
    memcpy((*ship_model)->conns, ship_lines, sizeof(ship_lines));

    (*ship_model)->n_verts =  sizeof(ship_verts) / sizeof(model_vertex_3d_t);
    (*ship_model)->n_conns =  sizeof(ship_lines) / sizeof(model_connection_t);    
  }


  return 0;

}


void ship_model_free(model_t **ship_model) {
  free((*ship_model)->verts);
  free((*ship_model)->conns);  
  free(*ship_model);
  *ship_model = NULL;
}



	
