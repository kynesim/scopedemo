#pragma once

typedef struct model_vertex_s {
  float x;
  float y;
} model_vertex_t;


typedef struct model_vertex_3d_s {
  float x;
  float y;
  float z;
  float w;  
} model_vertex_3d_t;


/* Lines are drawn by connecting verticies, connect vertex with index a to one with index b*/
typedef struct model_connection_s {
  int a;
  int b;
} model_connection_t;


/* Lines are drawn by connecting verticies, connect vertext with index a to one with index b*/
typedef struct model_s {
  model_vertex_3d_t  *verts;  
  model_connection_t *conns;
  int n_verts;
  int n_conns;  
} model_t;
