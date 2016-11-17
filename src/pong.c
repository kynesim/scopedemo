#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>

#define BUF_SIZE 4096

typedef struct vertex_s {
  float x;
  float y;
} vertex_t;

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

/* this got much better in cube.c, you might want to use that, 
 * sorry for the copy paste, I was rushing ;)
 */
void draw_vertex_list(draw_buffer_t *db, ao_sample_format *format,
		      vertex_t *vlist, int nlines) {
  int i;

  /*
  
  vertex_t p1 = {-MAX_X,-MAX_Y};
  vertex_t p2 = { MAX_X,-MAX_Y};
  vertex_t p3 = { MAX_X, MAX_Y};
  vertex_t p4 = {-MAX_X, MAX_Y};
  */	

	//drawline(buffer, samples_per_frame, p4, p1);


  char *p = db->buffer;
  int samples_per_line = (db->samples_per_frame / nlines);
  int bytes_per_sample = format->bits/8 * format->channels;
  int bytes_per_line = samples_per_line * bytes_per_sample;

  for (i =0; i < nlines; i++) {
    drawline(p, samples_per_line, vlist[2*i], vlist[(2*i) + 1]);
    p += bytes_per_line;
  }
	
 
  return;
}


void init_buffer(draw_buffer_t *draw_buffer, ao_sample_format *format) {
  /* -- Play some stuff -- */

  draw_buffer->frame_rate = 100;
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

int update_ball(vertex_t *verts, float bx, float by, int start_i) {
#define BALL_W 0.0125
#define BALL_H 0.025  
  verts[start_i].x = bx - BALL_W;
  verts[start_i].y = by - BALL_H;
  verts[start_i + 1].x = bx - BALL_W;
  verts[start_i + 1].y = by + BALL_H;

  verts[start_i + 2].x = bx - BALL_W;
  verts[start_i + 2].y = by + BALL_H;
  verts[start_i + 3].x = bx + BALL_W;
  verts[start_i + 3].y = by + BALL_H;

  verts[start_i + 4].x = bx + BALL_W;
  verts[start_i + 4].y = by + BALL_H;
  verts[start_i + 5].x = bx + BALL_W;
  verts[start_i + 5].y = by - BALL_H;

  verts[start_i + 6].x = bx + BALL_W;
  verts[start_i + 6].y = by - BALL_H;
  verts[start_i + 7].x = bx - BALL_W;
  verts[start_i + 7].y = by - BALL_H;
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

	// sorry!
	float ball_sx = 0.0;
	float ball_sy = 0.0;
        float ball_vx = 0.01;
	float ball_vy = 0.01;

	//coordinates are between -1 and 1 on x and y, scaled later.
	vertex_t verts[16] = {{-1,-1}, { 1,-1}, //field 
			      { 1,-1}, { 1, 1},
			      { 1, 1}, {-1, 1},
			      {-1, 1}, {-1,-1},

			      {-0.1,-0.1}, { 0.1,-0.1}, //ball
			      { 0.1,-0.1}, { 0.1, 0.1},
			      { 0.1, 0.1}, {-0.1, 0.1},
			      {-0.1, 0.1}, {-0.1,-0.1},

	};

	while(1) {
	  memset(draw_buffer.buffer, 0, draw_buffer.buf_size);

	  ball_sx += ball_vx;
	  ball_sy += ball_vy;

	  if (ball_sx > 1.0 || ball_sx < -1.0) {
	    ball_vx *= -1.0;
	  }

	  if (ball_sy > 1.0 || ball_sy < -1.0) {
	    ball_vy *= -1.0;
	  }

	  
	  update_ball(verts, ball_sx, ball_sy, 8);
	    
	  draw_vertex_list(&draw_buffer, &format,
			   verts, 8);
	
	  ao_play(device, draw_buffer.buffer, draw_buffer.buf_size);
	}

	/* -- Close and shutdown -- */
	ao_close(device);

	ao_shutdown();

  return (0);
}
