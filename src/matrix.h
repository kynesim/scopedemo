#pragma once

#include "model.h"

typedef struct matrix4_s {
  float elements[4][4];
} matrix4_t;


void matrix4_mul(matrix4_t *a, matrix4_t *b, matrix4_t *c);
void matrix4_mul_vert_list(model_t *m, matrix4_t *r, model_vertex_3d_t *out);
void matrix4_init_rotation(matrix4_t *a, float ax, float ay, float az);
void matrix4_print(matrix4_t *m);
