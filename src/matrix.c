

#include <math.h>
#include "matrix.h"
#include "model.h"

// C = A*B;

void matrix4_mul(matrix4_t *a, matrix4_t *b, matrix4_t *c) {
  int i = 0;
  int j = 0;
  int k = 0;  

  for (j = 0; j < 4; j++) {
    for (i = 0; i < 4; i++) {
      c->elements[j][i] = 0;
      for (k = 0; k < 4; k++) {	
	c->elements[j][i] += a->elements[j][k] * b->elements[k][i];
      }
    }


  }
    
  return;
}



void matrix4_init_rotation(matrix4_t *a, float ax, float ay, float az) {
  matrix4_t rx = { {
    {1,        0,         0, 0},
    {0, cosf(ax), -sinf(ax), 0},
    {0, sinf(ax),   cos(ax), 0},
    {0,        0,         0, 1},        
    }};
  
  matrix4_t ry = { {
    { cosf(ay), 0, sinf(ay), 0},
    {        0, 1,        0, 0},
    {-sinf(ay), 0, cosf(ay), 0},
    {        0, 0,        0, 1},        
    }};
  
  matrix4_t rz = {{
    {cosf(az), -sinf(az), 0, 0},
    {sinf(az),  cosf(az), 0, 0},
    {       0,         0, 1, 0},
    {       0,         0, 0, 1},        
    }};

  matrix4_t rxry;

  matrix4_mul(&rx, &ry, &rxry);
  matrix4_mul(&rxry, &rz, a);  

    
  return;
}


void matrix4_mul_vert(matrix4_t *r, model_vertex_3d_t *in, model_vertex_3d_t *out) {
  out->x = ((r->elements[0][0]*in->x) +
	    (r->elements[0][1]*in->y) +
	    (r->elements[0][2]*in->z) +
	    (r->elements[0][3]*in->w));

  out->y = ((r->elements[1][0]*in->x) +
	    (r->elements[1][1]*in->y) +
	    (r->elements[1][2]*in->z) +
	    (r->elements[1][3]*in->w));

  out->z = ((r->elements[2][0]*in->x) +
	    (r->elements[2][1]*in->y) +
	    (r->elements[2][2]*in->z) +
	    (r->elements[2][3]*in->w));

  out->w = ((r->elements[3][0]*in->x) +
	    (r->elements[3][1]*in->y) +
	    (r->elements[3][2]*in->z) +
	    (r->elements[3][3]*in->w));	      
}

void matrix4_mul_vert_list(model_t *m, matrix4_t *r, model_vertex_3d_t *out) {
  int i = 0;

  for (i = 0; i < m->n_verts; i++) {
    matrix4_mul_vert(r, &(m->verts[i]), &(out[i]));
  }
  
}



void matrix4_print(matrix4_t *m) {
  int j = 0;
  for (j = 0; j < 4; j++) {
    printf("%f %f %f %f\n\n", m->elements[j][0], m->elements[j][1], m->elements[j][2], m->elements[j][3]);
  }
}

