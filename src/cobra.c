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

void scale(vertex_3d_t *model, vertex_3d_t *by, int nverts) { 
    int i;
    for (i =0;i < nverts; ++i) {
        model[i].x = model[i].x * by->x;
        model[i].y = model[i].y * by->y;
        model[i].z = model[i].z * by->z;
    }
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

#define NVERTS 12
	vertex_3d_t cobra_verts[NVERTS] = {
            { 16.3787518, -0.1218359, 36.7125778 },
            { -16.3787518, -0.1218359, 36.7125778 },
            { 0,	13.1859007,	10.0971088 },
            { -61.4203186,	-1.6573437, 	-6.2816429 },
            { 61.4203186,	-1.6573437,	-6.2816429 },
            { 	-45.0415688,	8.0675402	,-22.6603947 },
            { 45.0415688	,8.0675402,	-22.6603947 },
            { 65.515007, 	-4.2165241, 	-22.6603947 },
            { 	-65.515007,	-4.2165241	, -22.7115784 },
            { 0,	13.1859007	,-22.71157841 },
            {	-16.3787518,	-12.4059,	-22.7115784 },
            { 16.3787518,	-12.4059, 	-22.7115784 } };

        vertex_3d_t cobra_scale = 
            { 1/80.0, 1/80.0, 1/80.0 } ;


	connection_t cobra_lines[] = {
    {   0  , 1 },
    { 0, 	2},
    { 0, 	4 },
    { 0, 	6 },
    { 0, 	11},
    { 1, 	2 },
    { 1, 	3 },
    { 1, 	5 },
    { 2, 	6 },
    { 2, 	9 },
    {  3, 	5},
    { 3, 	8 },
    { 4, 	7 },
    { 4, 	11 },
    { 5, 	2 },
    { 5, 	9 },
    { 6, 	4 },
    {  6, 	7} ,
    { 7, 	11 },
    { 8, 	5} ,
    { 9, 	6 },
    { 9, 	11 },
    { 9, 	7 },
    { 9, 	8 },
    { 10, 0 },
    { 10, 	1 },
    { 10, 	3 },
    { 10, 	8 },
    { 10,	11 },
    { 10, 	9 }  };

    vertex_3d_t scaled_cobra_verts[NVERTS];
    vertex_3d_t rotated_cobra_verts[NVERTS];

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
          memcpy( scaled_cobra_verts, cobra_verts, NVERTS * sizeof(vertex_3d_t));
          scale( scaled_cobra_verts, &cobra_scale, NVERTS);
	  rotate(scaled_cobra_verts, rotated_cobra_verts, NVERTS);	 
	  project(rotated_cobra_verts, projected_verts, NVERTS);

	    
	  draw_vertex_list(&draw_buffer, &format,
			   projected_verts, cobra_lines, sizeof(cobra_lines) / sizeof(connection_t));
	
	  ao_play(device, draw_buffer.buffer, draw_buffer.buf_size);
	}

	/* -- Close and shutdown -- */
	ao_close(device);
	ao_shutdown();

	return (0);
}
