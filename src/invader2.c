#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "invader_model.h"
#include "ship_model.h"
#include "matrix.h"

#define BUF_SIZE 96000

typedef struct draw_buffer_s {
  char * buffer;
  int buf_size;
  int samples_per_frame;
  int frame_rate;
} draw_buffer_t;


typedef struct invader_s {
  float x;
  float y;

  float s;

  int state;
  
} invader_t;


typedef struct projectile_s {
  float x;
  float y;

  float vy;
  
  int state;
  
} projectile_t;

projectile_t p = {0};



typedef struct io_s {
    int fds[2];
} io_t;


// 30000 just in case of artifacts at loud noises? (a bit less that 2^15)
#define MAX_X  30000.0
// 16000 to get as much usable screen as possible, due to step sizes in volts/div on scope
#define MAX_Y  16000.0



float invaders_block_offset_x = 0;

float ship_offset_x = 0;
float ship_vx = 0;

void invaders_block_animate(void) {
  static float xxx = 0;
  xxx += M_PI * 0.015;  
  invaders_block_offset_x = sinf(xxx) * 0.3;  

}



model_vertex_t *output_vertex_list = NULL;
int output_vertex_list_length = 0;

int ensure_draw_list_length(int length) {
  if (length > output_vertex_list_length) {
    output_vertex_list = realloc(output_vertex_list, length*(sizeof(model_vertex_t)));
    output_vertex_list_length = length;

    if (output_vertex_list == NULL) {
      printf("No mem %s %d", __FILE__,  __LINE__);
      exit(0);
    }
  }

  return 0;
}


#define SHIP_VX 0.05;
static void update_ship(int hid_result) {
    if (hid_result & (1<<4)) { 
        /* Stop! */
      ship_vx = 0.0;	
    }
    if (hid_result & (1<<0)) {
      ship_vx = SHIP_VX;
    } 

    if (hid_result & (1<<1)) {
        ship_vx = -SHIP_VX;
    }

    if (hid_result & (1<<2)) {
      if (p.state == 0) {
	p.state = 1;
	p.vy = 0.08;
	p.x = ship_offset_x;
	p.y = -0.8;	
      }
    }     

}



int push_model_to_vertex_list(model_vertex_t *vlist, model_connection_t *con, int n_conns, model_vertex_t *output_list) {
  int i;
  int j = 0;

  for (i = 0; i < n_conns; i++) {
    output_list[j + 0] = vlist[con[i].a];
    output_list[j + 1] = vlist[con[i].b];     
    j = j + 2;
  }
	
 
  return j;
}



/* no need for anything fancy, at 30000x16000 you wont notice. */
void drawline(char *buffer, int n_buffer_samples /*no. samples in buffer*/,
	      model_vertex_t p1, model_vertex_t p2) {
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

#if 0
int draw_vertex_list(draw_buffer_t *db, ao_sample_format *format,
		      model_vertex_t *vlist, model_connection_t *con, int nlines) {
  int i;

  char *p = db->buffer;
  int samples_per_line = (db->samples_per_frame / nlines);
  int bytes_per_sample = format->bits/8 * format->channels;
  int bytes_per_line = samples_per_line * bytes_per_sample;

  for (i =0; i < nlines; i++) {
    drawline(p, samples_per_line, vlist[con[i].a], vlist[con[i].b]);
    p += bytes_per_line;
  }
	
 
  return p - db->buffer;
}
#endif

int draw_output_vertex_list(draw_buffer_t *db, ao_sample_format *format,
			    model_vertex_t *vlist, int n_verts) {
  int i;

  char *p = db->buffer;
  int samples_per_line = (db->samples_per_frame / (n_verts / 2));
  int bytes_per_sample = format->bits/8 * format->channels;
  int bytes_per_line = samples_per_line * bytes_per_sample;

  for (i = 0; i < n_verts; i += 2) {
    drawline(p, samples_per_line, vlist[i], vlist[i + 1]);
    p += bytes_per_line;
  }
	
 
  return p - db->buffer;
}


#define FRAMERATE 100
void init_buffer(draw_buffer_t *draw_buffer, ao_sample_format *format) {
  /* -- Play some stuff -- */

  draw_buffer->frame_rate = FRAMERATE;
  draw_buffer->samples_per_frame = format->rate / draw_buffer->frame_rate;
  
  draw_buffer->buf_size = (format->bits/8 * format->channels * draw_buffer->samples_per_frame);
  draw_buffer->buffer = calloc(draw_buffer->buf_size,
			       sizeof(char));

  printf("Format rate: %d, Frame rate: %d, samples per frame: %d\n", format->rate, FRAMERATE, draw_buffer->samples_per_frame);
  
  


}

void init_format(ao_sample_format *format) {
  memset(format, 0, sizeof(*format));
  format->bits = 16;
  format->channels = 2;
  format->rate = 96000; // 96000
  format->byte_format = AO_FMT_LITTLE;
}



static int poll_hid( int fd ) { 
    char buf[256];
    int rv;
    int outval = 0;
    if (fd < 0) { return 0; }

    rv = read( fd, buf, 256);
    if (rv < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return 0;
        } else {
            printf("Cannot read from hidraw : %s [%d] \n",
                   strerror(errno), errno);
            exit(1);
        }
    } else {
        if (rv >= 8) { 
            //dump( buf, rv );	  
            int key = buf[2];
	    printf("key %d\n", key);
            switch (key) { 
            case 94:
                /* 8 -> move up */
	      outval |= (1 << 0);
                break;
            case 92:
                /* 2 -> move down */
	      outval |= (1<<1);
                break;
            case 93:
	      // fire
	      outval |= (1<<2);
                break;
		
            case 0:
                /* Do nothing */
                break;
            default:
                outval |= 16;
                break;
            }
        }
    }
    return outval;
}



void project(model_vertex_3d_t *verts, model_vertex_t *projected, int nverts) {
  int i;
  float focus = 0.8;

  for (i = 0; i < nverts; i++) {
    projected[i].x = 0.6*(verts[i].x) / ((verts[i].z + 1.3) * focus);
    projected[i].y = 0.6*(verts[i].y) / ((verts[i].z + 1.3) * focus);
  }
  
  return;
}

void rotate(model_vertex_3d_t *model, model_vertex_3d_t *rotated, int nverts) {
  int i = 0;
  /* so sue me */
  static float xr = 0;
  static float yr = 0;
  static float zr = 0;
  static float xxx = 0;

  xxx += M_PI * 0.015;

  zr = M_PI +  (M_PI / 2);
  yr = sinf(xxx) * (M_PI / 8);
  //yr += M_PI * 0.005;
  //zr += M_PI * 0.005;

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



void rotate2(model_t *model, invader_t *invader, model_vertex_3d_t *rotated) {
#if 1
  
  
  /* so sue me */
  static float xr = 0;
  static float yr = 0;
  static float zr = 0;
  static float xxx = 0;

  xxx += M_PI * 0.015;

  zr = M_PI +  (M_PI / 2);
  yr = sinf(xxx) * (M_PI / 12);
#endif  


  matrix4_t rs;
  matrix4_t trs;  
  matrix4_t rotation;
  matrix4_t scale = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};


  matrix4_t translation = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};

  matrix4_t I = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};  


  scale.elements[0][0] = 0.7;
  scale.elements[1][1] = 0.7;
  
  translation.elements[0][3] = invader->x + invaders_block_offset_x;
  translation.elements[1][3] = invader->y;
  
  matrix4_init_rotation(&rotation, 0, yr, 0);//xxx / 1.0);

  matrix4_mul(&rotation, &scale, &rs);  
  matrix4_mul(&translation, &rs, &trs);


  matrix4_mul_vert_list(model, &trs, rotated);


#if 0
  {
    static int i = 0;  
    if ((i++ % 10)  == 0) {
      printf("p: %f %f %f (%f %f %f) %d\n", rotated[20].x, rotated[20].y, rotated[20].z, model->verts[20].x, model->verts[20].y, model->verts[20].z, i);
      
    }
  }
#endif  

  return;
}


void rotate_ball(model_t *model, model_vertex_3d_t *rotated) {
  matrix4_t rs;
  matrix4_t trs;  
  matrix4_t rotation = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};
  
  matrix4_t scale = {{
      {0.02, 0, 0, 0},
      {0, 0.02, 0, 0},
      {0, 0, 0.05, 0},
      {0, 0, 0, 1},
    }};


  matrix4_t translation = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};

  matrix4_t I = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};


  p.y += p.vy;


  translation.elements[0][3] = p.x;
  translation.elements[1][3] = p.y;
  
  //matrix4_init_rotation(&rotation, 0, xxx, 0);//xxx / 1.0);

  matrix4_mul(&rotation, &scale, &rs);  
  matrix4_mul(&translation, &rs, &trs);


  matrix4_mul_vert_list(model, &trs, rotated);


#if 0
  {
    static int i = 0;  
    if ((i++ % 10)  == 0) {
      printf("p: %f %f %f (%f %f %f) %d\n", rotated[20].x, rotated[20].y, rotated[20].z, model->verts[20].x, model->verts[20].y, model->verts[20].z, i);
      
    }
  }
#endif  

  return;
}

void rotate_ship(model_t *model, model_vertex_3d_t *rotated) {
#if 1
  
  
  /* so sue me */
  static float xr = 0;
  static float yr = 0;
  static float zr = 0;
  static float xxx = 0;

  xxx += M_PI * 0.015;

  zr = M_PI +  (M_PI / 2);
  yr = sinf(xxx) * (M_PI / 12);
#endif

  ship_offset_x += ship_vx;  


  matrix4_t rs;
  matrix4_t trs;  
  matrix4_t rotation;
  matrix4_t scale = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};


  matrix4_t translation = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};

  matrix4_t I = {{
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
    }};  


  scale.elements[0][0] = 0.3;//1 + cosf(xxx * 5.0) * 0.1;
  scale.elements[1][1] = 0.3;//1 + cosf(xxx * 5.0) * 0.1;
  scale.elements[2][2] = 0.3;//1 + cosf(xxx * 5.0) * 0.1;  
  
  translation.elements[0][3] = ship_offset_x;
  translation.elements[1][3] = -1.0;
  
  matrix4_init_rotation(&rotation, 0, xxx, 0);//xxx / 1.0);

  matrix4_mul(&rotation, &scale, &rs);  
  matrix4_mul(&translation, &rs, &trs);


  matrix4_mul_vert_list(model, &trs, rotated);


#if 0
  {
    static int i = 0;  
    if ((i++ % 10)  == 0) {
      printf("p: %f %f %f (%f %f %f) %d\n", rotated[20].x, rotated[20].y, rotated[20].z, model->verts[20].x, model->verts[20].y, model->verts[20].z, i);
      
    }
  }
#endif  

  return;
}


matrix4_t a = { {
  {1, 1, 0, 0},
  {0, 2, 3, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 3},
  }
};

matrix4_t b = {{ 
  {0, 1, 0, 5},
  {2, 0, 2, 0},
  {0, 3, 0, 0},
  {0, 0, 0, 1},
  }
}; 


matrix4_t c = {
  {
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  }
} ;


void invaders_init(invader_t *invaders, int n_invaders, int max_row) {
  int i = 0;
  float y = 1.5;
  float x = 0;
  float lx = 0;

  float invader_width = 2.0 / (float)max_row;

  if (n_invaders == 1) {
    lx = 0;
  }
  else if (max_row % 2 == 1) {
    int q = max_row / 2;
    lx = -(invader_width * q);
  } else {
    int q = max_row / 2;
    lx = -(invader_width * ((float)q - 0.5));
  }

  x = lx;

  for (i = 0; i < n_invaders; i++) {
    if (i > 0 && (i % max_row) == 0) {
      y -= 0.7;
      x = lx;
    }

    
    invaders[i].x = x;
    invaders[i].y = y;
    invaders[i].state = 1;    


    x += invader_width;
  }

  {
    int i;
    for (i = 0; i < n_invaders; i++) {
      printf("Invader %d: at the ready. Co-ords: %f %f\n", i, invaders[i].x, invaders[i].y);
    }

				       
  }

}



#define N_INVADERS 8
#define N_INVADERS_PER_LINE 4
int main(int argc, char **argv)
{
	ao_device *device;
	ao_sample_format format;
	int default_driver;
	invader_t invaders[N_INVADERS];
	char buf[64];
        io_t io;
	draw_buffer_t draw_buffer;

	if (argc != 3) {
	  fprintf(stderr, "Syntax: invaders [hid#-player1] [hid#-player2]\n");
	  return 1;
        }


	invaders_init(invaders, N_INVADERS, N_INVADERS_PER_LINE);

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

	        printf("Controls: \n");
        printf(" 8 - bat up, 2 - bat down \n");
        printf(" 4 - bat out, 6 - bat in \n");
        
        sprintf(buf, "/dev/hidraw%d", atoi( argv[1] ));
        printf(" -- Player 1 is %s \n", buf);
        io.fds[0] = open(buf, O_RDONLY | O_NDELAY);
        if (io.fds[0] < 0) {
            fprintf(stderr, "WARNING: Cannot open %s - %s [%d] \n",
                    buf, strerror(errno), errno);
            io.fds[0] = -1;
        }
        sprintf(buf, "/dev/hidraw%d", atoi(argv[2]));
        printf(" -- Player 2 is %s \n", buf);
        io.fds[1] = open(buf, O_RDONLY | O_NDELAY);
        if (io.fds[1] < 0) {
            fprintf(stderr, "WARNING: Cannot open %s - %s [%d] \n",
                    buf, strerror(errno), errno);
            io.fds[1] = -1;
        }


	init_buffer(&draw_buffer, &format);


	model_t *invader_model;
	invader_model_get(&invader_model);


	model_t *ship_model;
	ship_model_get(&ship_model);

	model_t *ball_model;
	ball_model_get(&ball_model);
	

	{
	  int j = 0;
	  matrix4_mul(&a, &b, &c);
	  for (j = 0; j < 4; j++) {
	    printf("%f %f %f %f\n\n", c.elements[j][0], c.elements[j][1], c.elements[j][2], c.elements[j][3]);
	  }
	}

	
	
	while(1) {
	  memset(draw_buffer.buffer, 0, draw_buffer.buf_size);

	  int i;
	  int used = 0;
	  int n_output_verts = 0;


	  {
	    int rv;
	    int thing;
	    struct pollfd fds[2];
	    fds[0].fd = io.fds[0];
	    fds[0].revents = 0;
	    fds[0].events = POLLIN;
	    fds[1].fd = io.fds[1];
	    fds[1].revents = 0;
	    fds[1].events = POLLIN;
	    rv = poll(fds, 2, 0 );
	    if (fds[0].revents) {
	      thing = poll_hid(io.fds[0]);
	      //update_velocity( thing, &bat_vel[0] );
	      update_ship(thing);	      
	    }
	    
	    if (fds[1].revents) {
	      thing = poll_hid(io.fds[1]);
	      //update_velocity( thing, &bat_vel[1] );
	      update_ship(thing);    	      
	    }
	  }
	  


	  invaders_block_animate();
#if 1
	  int invaders_alive = 0;
	  for (int i = 0; i < N_INVADERS; i++) {
	    if (invaders[i].state) {

	      // is projectile live?
	      if (p.state > 0) {
		// has it hit the invader?
		if (p.x > (invaders[i].x + invaders_block_offset_x - 0.2) && p.x < (invaders[i].x + invaders_block_offset_x + 0.2) && (p.y > invaders[i].y - 0.2) && (p.y < invaders[i].y + 0.2)) {
		  invaders[i].state = 0;
		  p.state = 0;
		  continue;
		}
	      }

	      invaders_alive = 1;
	      model_vertex_3d_t rotated_cube_verts[invader_model->n_verts];
	      model_vertex_t projected_verts[invader_model->n_verts];
	      memset(projected_verts, 0, invader_model->n_verts * sizeof(model_vertex_t));
	      rotate2(invader_model, &(invaders[i]), rotated_cube_verts);
	      project(rotated_cube_verts, projected_verts, invader_model->n_verts);
	      
	      
	      ensure_draw_list_length(n_output_verts + (2 * invader_model->n_conns));
	      
	      n_output_verts += push_model_to_vertex_list(projected_verts, invader_model->conns, invader_model->n_conns, &(output_vertex_list[n_output_verts]));

	      
	    }
	  }

	  if (invaders_alive == 0) {
	    invaders_init(invaders, N_INVADERS, N_INVADERS_PER_LINE);	    
	  }
#endif	  

#if 1
	  {
	    model_vertex_3d_t rotated_ship_verts[ship_model->n_verts];
	    model_vertex_t projected_verts[ship_model->n_verts];
	    memset(projected_verts, 0, ship_model->n_verts * sizeof(model_vertex_t));
	    rotate_ship(ship_model, rotated_ship_verts);
	    project(rotated_ship_verts, projected_verts, ship_model->n_verts);


	    ensure_draw_list_length(n_output_verts + (2 * ship_model->n_conns));
	    
	    n_output_verts += push_model_to_vertex_list(projected_verts, ship_model->conns, ship_model->n_conns, &(output_vertex_list[n_output_verts]));
	    
	    
	  }
#endif

#if 1
	  if (p.state != 0){
	    model_vertex_3d_t rotated_ball_verts[ball_model->n_verts];
	    model_vertex_t projected_verts[ball_model->n_verts];
	    memset(projected_verts, 0, ball_model->n_verts * sizeof(model_vertex_t));
	    rotate_ball(ball_model, rotated_ball_verts);
	    project(rotated_ball_verts, projected_verts, ball_model->n_verts);


	    ensure_draw_list_length(n_output_verts + (2 * ball_model->n_conns));
	    
	    n_output_verts += push_model_to_vertex_list(projected_verts, ball_model->conns, ball_model->n_conns, &(output_vertex_list[n_output_verts]));
	    
	    
	  }

	  if (p.y > 1.5) {
	    p.state = 0;
	    p.y = 0;
	  }
#endif	  
	  
	  

	  if (n_output_verts <= 0) {
	    continue;
	  }
	  
	  used = draw_output_vertex_list(&draw_buffer, &format, output_vertex_list, n_output_verts);
	    

	  //	  printf("N %d, %d, %d\n", n_output_verts, used, invader_model->n_conns);
		    

	  {
	    static int k = 0;
	    if (k ++ % 100 == 0) {
	      printf("%d of %d %d\n", used, draw_buffer.samples_per_frame, draw_buffer.buf_size);
	    }
	  }
	  
	  //ao_play(device, draw_buffer.buffer,  draw_buffer.buf_size);
	  ao_play(device, draw_buffer.buffer,  used);
	    
	}

	/* -- Close and shutdown -- */
	ao_close(device);
	ao_shutdown();

	return (0);
}
