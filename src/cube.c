#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>

#define BUF_SIZE 4096

typedef struct vertex_s {
  float x;
  float y;
} vertex_t;


typedef struct vertex_3d_s {
  float x;
  float y;
  float z;
} vertex_3d_t;


/* Lines are drawn by connecting verticies, connect vertext with index a to one with index b*/
typedef struct connection_s {
  int a;
  int b;
} connection_t;

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

/* no need for anything fancy, at 30000x16000 you wont notice. */
void drawline(char *buffer, int n_buffer_samples /*no. samples in buffer*/,
	      vertex_t p1, vertex_t p2) {
  int i;
  int16_t sample_x;
  int16_t sample_y;

  float x_diff = p2.x - p1.x;
  float y_diff = p2.y - p1.y;
  
  for (i = 0; i < n_buffer_samples; i++) {
    float m = (float)i / (float)n_buffer_samples;

    sample_x = (int16_t)((p1.x + (x_diff * m)) * MAX_X);
    sample_y = (int16_t)((p1.y + (y_diff * m)) * MAX_Y);

    buffer[4*i]   = sample_x & 0xff;
    buffer[4*i+3] = (sample_y >> 8) & 0xff;
    buffer[4*i+2] = sample_y & 0xff;
    buffer[4*i+1] = (sample_x >> 8) & 0xff;
  }



  return;
}

void draw_vertex_list(draw_buffer_t *db, ao_sample_format *format,
		      vertex_t *vlist, connection_t *con, int nlines) {
  int i;

  char *p = db->buffer;
  int samples_per_line = (db->samples_per_frame / nlines);
  int bytes_per_sample = format->bits/8 * format->channels;
  int bytes_per_line = samples_per_line * bytes_per_sample;

  for (i =0; i < nlines; i++) {
    drawline(p, samples_per_line, vlist[con[i].a], vlist[con[i].b]);
    p += bytes_per_line;
  }
	
 
  return;
}


#define FRAMERATE 100
void init_buffer(draw_buffer_t *draw_buffer, ao_sample_format *format) {
  /* -- Play some stuff -- */

  draw_buffer->frame_rate = FRAMERATE;
  draw_buffer->samples_per_frame = format->rate / draw_buffer->frame_rate;
  
  draw_buffer->buf_size = (format->bits/8 * format->channels * draw_buffer->samples_per_frame);
  draw_buffer->buffer = calloc(draw_buffer->buf_size,
			       sizeof(char));


}

void init_format(ao_sample_format *format) {
  memset(format, 0, sizeof(format));
  format->bits = 16;
  format->channels = 2;
  format->rate = 96000;
  format->byte_format = AO_FMT_LITTLE;
}



void project(vertex_3d_t *verts, vertex_t *projected, int nverts) {
  int i;
  float focus = 0.8;

  for (i = 0; i < nverts; i++) {
    projected[i].x = 0.6*(verts[i].x) / ((verts[i].z + 1.3) * focus);
    projected[i].y = 0.6*(verts[i].y) / ((verts[i].z + 1.3) * focus);
  }
  
  return;
}

void rotate(vertex_3d_t *model, vertex_3d_t *rotated, int nverts) {
  int i = 0;
  /* so sue me */
  static float xr = 0;
  static float yr = 0;
  static float zr = 0;

  xr += M_PI * 0.005;
  yr += M_PI * 0.005;
  zr += M_PI * 0.005;

  for (i = 0; i < nverts; i++) {
    float x1 = model[i].x;
    float y1 = model[i].y;
    float z1 = model[i].z;

    float xx = x1;
    float yy = y1*cosf(xr)+z1*sinf(xr);
    float zz = z1*cosf(xr)-y1*sinf(xr);
    y1 = yy;
    x1=xx*cosf(yr)-zz*sinf(yr);
    z1=xx*sinf(yr)+zz*cosf(yr);
    zz=z1;
    xx=x1*cosf(zr)-y1*sinf(zr);
    yy=x1*sinf(zr)+y1*cosf(zr);

    rotated[i].x = xx;
    rotated[i].y = yy;
    rotated[i].z = zz;
  }

  return;
}


int main(int argc, char **argv)
{
	ao_device *device;
	ao_sample_format format;
	int default_driver;
	int16_t sample;
	int16_t sample2;

	draw_buffer_t draw_buffer;

	int i;

	/* -- Initialize -- */

	fprintf(stderr, "libao example program\n");

	ao_initialize();

	/* -- Setup for default driver -- */

	default_driver = ao_default_driver_id();
	
	init_format(&format);

	/* -- Open driver -- */
	device = ao_open_live(default_driver, &format, NULL /* no options */);
	if (device == NULL) {
		fprintf(stderr, "Error opening device.\n");
		return 1;
	}

	init_buffer(&draw_buffer, &format);

#define NVERTS 8
	vertex_3d_t cube_verts[NVERTS] = {
	  {-0.3,-0.3,-0.3},
	  { 0.3,-0.3,-0.3},
	  { 0.3, 0.3,-0.3},
	  {-0.3, 0.3,-0.3},
	  {-0.3,-0.3, 0.3},
	  { 0.3,-0.3, 0.3},
	  { 0.3, 0.3, 0.3},
	  {-0.3, 0.3, 0.3},
	};
	vertex_3d_t rotated_cube_verts[NVERTS];


	connection_t cube_lines[] = {
	  {0,1},
	  {1,2},
	  {2,3},
	  {3,0},

	  {0,4},
	  
	  {4,5},
	  {5,6},
	  {6,7},
	  {7,4},
	  
	  {4,5},
	  {5,1},

	  {1,2},
	  {2,6},
	  {6,7},
	  {7,3},
	  {3,0},
	  

	};

	vertex_t projected_verts[NVERTS] = {
	  {1,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},

	};

	while(1) {
	  memset(draw_buffer.buffer, 0, draw_buffer.buf_size);

	  rotate(cube_verts, rotated_cube_verts, NVERTS);	 
	  project(rotated_cube_verts, projected_verts, NVERTS);

	    
	  draw_vertex_list(&draw_buffer, &format,
			   projected_verts, cube_lines, sizeof(cube_lines) / sizeof(connection_t));
	
	  ao_play(device, draw_buffer.buffer, draw_buffer.buf_size);
	}

	/* -- Close and shutdown -- */
	ao_close(device);
	ao_shutdown();

	return (0);
}
