#include <stdio.h>
#include <string.h>
#include <ao/ao.h>
#include <math.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/poll.h>
#include <float.h>
#include <limits.h>

#define FLOAT_MIN (-999.0)
#define FLOAT_MAX (999.0)

#define BUF_SIZE 4096

/* How fast do things move? */
#define VEL_Y (0.01)
#define VEL_Z (0.01)

typedef struct io_s {
    int fds[2];
} io_t;


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


static void dump(const uint8_t *b, int n) {
    int i;
    for (i =0;i < n; ++i) {
        if (!(i%16)) { printf("\n%04x: ", i); }
        printf("%02x ", b[i]);
    }
    printf("\n");
}


static void init_ball(vertex_3d_t *ball_vec,
                      vertex_3d_t *ball_v) { 
    memset(ball_vec, '\0', sizeof(vertex_3d_t));

    /* Start of a game. Set the ball_v to something random */
    srandom(time(NULL));
    int x,y,z;
    x = (random()%20)-10;
    y = (random()%20)-10;
    z = (random()%20)-10;
    
    ball_v->x = (double)x/500.0;
    ball_v->y = (double)y/500.0;
    ball_v->z = (double)z/800.0;

    if (ball_v->x >= 0.0) { 
        if ( ball_v->x < 0.02) { 
            ball_v->x = 0.02;
        }
    } else {
        if (ball_v->x > -0.02) { 
            ball_v->x = -0.02;
        }
    }
}

#define BAT_Y_MIN (-1.0)
#define BAT_Y_MAX (1.0)

#define BAT_Z_MIN (-0.5)
#define BAT_Z_MAX (0.3)

static void limit_bat( vertex_3d_t * pos ) {
    if (pos->y < BAT_Y_MIN) { pos->y = BAT_Y_MIN; }
    if (pos->z < BAT_Z_MIN) { pos->z = BAT_Z_MIN; }

    if (pos->y > BAT_Y_MAX) { pos->y = BAT_Y_MAX; }
    if (pos->z > BAT_Z_MAX) { pos->z = BAT_Z_MAX; }
}


static void update_velocity( int hid_result, vertex_3d_t * vel ) {
    if (hid_result & (1<<4)) { 
        /* Stop! */
        vel->x = vel->y = vel->z = 0.0;
    }
    if (hid_result & (1<<0)) {
        vel->y = VEL_Y;
    } 

    if (hid_result & (1<<1)) {
        vel->y = -VEL_Y;
    } 

    if (hid_result & (1<<2)) {
        vel->z = VEL_Z;
    } 


    if (hid_result & (1<<3)) {
        vel->z = -VEL_Z;
    } 
}

static void reflect( vertex_3d_t * ball_v, vertex_3d_t * batverts, int nr_batverts) { 
    ball_v->x = -ball_v->x;
    ball_v->y = -ball_v->y;
    ball_v->z = -ball_v->z;
    
    double xx,yy,zz;
    xx = (double)(random()%20)/1000.0;
    yy = (double)(random()%20)/1000.0;
    zz = (double)(random()%20)/1600.0;

    if (ball_v->x < 0.0) { xx = -xx; }
    if (ball_v->y < 0.0) { yy = -yy; }
    if (ball_v->z < 0.0) { zz = -zz; }

    /* add a bit of randomness */
    ball_v->x += xx;
    ball_v->y += yy;
    ball_v->z += zz;
}

/* If any of the vertices of object1 are within the bounding box of object2, return 1;
 *  only works because bats are rectangular 
 */
static int is_inside( vertex_3d_t * object1,  int nr_1, vertex_3d_t * object2 , int nr2 ) {
    float mins[3] = { FLOAT_MAX, FLOAT_MAX, FLOAT_MAX };
    float maxs[3] = { FLOAT_MIN, FLOAT_MIN, FLOAT_MIN };
    int i;



    for (i =0;i< nr2;++i) {
        if (object2[i].x < mins[0]) { mins[0] = object2[i].x; }
        if (object2[i].y < mins[1]) { mins[1] = object2[i].y; } 
        if (object2[i].z < mins[2]) { mins[2] = object2[i].z; }   
        
        if (object2[i].x > maxs[0]) { maxs[0] = object2[i].x; }
        if (object2[i].y > maxs[1]) { maxs[1] = object2[i].y; }
        if (object2[i].z > maxs[2]) { maxs[2] = object2[i].z; }
    }
    for (i =0 ; i <nr_1; ++i) {
        if (object1[i].x > mins[0] &&
            object1[i].x < maxs[0] &&
            object1[i].y > mins[1] && 
            object1[i].y < maxs[1] && 
            object1[i].z > mins[2] && 
            object1[i].z < maxs[2]) { 
            return 1;
        }
    }
    return 0;
}

/** @return 0 for no event, OR 
 *   bit 0 -> move up
 *   bit 1 -> move down
 *   bit 2 -> move in
 *   bit 3 -> move out
 *   bit 4 -> stop moving
 */
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
            switch (key) { 
            case 0x5c: 
                /* 4 -> move in */
                outval |= 4;
                break;
            case 0x5e:
                outval |= 8;
                break;
            case 0x60:
                /* 8 -> move up */
                outval |= 1;
                break;
            case 0x5a:
                /* 2 -> move down */
                outval |= 2;
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


void init_buffer(draw_buffer_t *draw_buffer, ao_sample_format *format) {
  /* -- Play some stuff -- */

  draw_buffer->frame_rate = 100;
  draw_buffer->samples_per_frame = format->rate / draw_buffer->frame_rate;
  
  draw_buffer->buf_size = (format->bits/8 * format->channels * draw_buffer->samples_per_frame);
  draw_buffer->buffer = calloc(draw_buffer->buf_size,
			       sizeof(char));


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

void vec3d_add(vertex_3d_t *result, vertex_3d_t *add) { 
    result->x += add->x;
    result->y += add->y;
    result->z += add->z;
}

void scale(vertex_3d_t *model, vertex_3d_t *by, int nverts) { 
    int i;
    for (i =0;i < nverts; ++i) {
        model[i].x = model[i].x * by->x;
        model[i].y = model[i].y * by->y;
        model[i].z = model[i].z * by->z;
    }
}

void translate(vertex_3d_t *model, vertex_3d_t *by, int nverts) { 
    int i;
    for (i =0;i < nverts; ++i) {
        model[i].x += by->x;
        model[i].y += by->y;
        model[i].z += by->z;
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
        char buf[64];
        io_t io;
	draw_buffer_t draw_buffer;

	int i;

        if (argc != 3) {
            fprintf(stderr, "Syntax: pong [hid#-player1] [hid#-player2]\n");
            return 1;
        }


	/* -- Initialize -- */

        printf("Start up ao.\n");

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

#define BALL_NVERTS 8
#define BAT_NVERTS 8
	vertex_3d_t bat_verts[BAT_NVERTS] = {
	  {-0.3,-0.8,-0.3},
	  { 0.3,-0.8,-0.3},
	  { 0.3, 0.8,-0.3},
	  {-0.3, 0.8,-0.3},
	  {-0.3,-0.8, 0.3},
	  { 0.3,-0.8, 0.3},
	  { 0.3, 0.8, 0.3},
	  {-0.3, 0.8, 0.3},
	};

        vertex_3d_t bat_size = { 0.1, 0.8, 0.5 };

	vertex_3d_t ball_verts[BALL_NVERTS] = {
	  {-0.3,-0.3,-0.3},
	  { 0.3,-0.3,-0.3},
	  { 0.3, 0.3,-0.3},
	  {-0.3, 0.3,-0.3},
	  {-0.3,-0.3, 0.3},
	  { 0.3,-0.3, 0.3},
	  { 0.3, 0.3, 0.3},
	  {-0.3, 0.3, 0.3},
	};
	vertex_3d_t rotated_ball_verts[BALL_NVERTS];
        /* Where is the ball? */
        vertex_3d_t ball_vec = { 0.0, 0.0, 0.0 };
        vertex_3d_t ball_v = { 0.0, 0.0, 0.0 };
        
        vertex_3d_t ball_size = { 0.2, 0.2, 0.2 };

	connection_t ball_lines[] = {
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

	vertex_t projected_ball_verts[BALL_NVERTS] = {
	  {1,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},
	  {0,0},

	};

        vertex_3d_t translated_bat_verts[2][BAT_NVERTS];

	vertex_t projected_bat_verts[2][BAT_NVERTS] = {
            { 
                {1,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
            }, 

            { 
                {1,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
                {0,0},
            }
	};

        vertex_3d_t bat_pos[2] = {
            { -1.0, 0.0, 0.0 },
            { 1.0, 0.0, 0.0 }
        };


        vertex_3d_t bat_vel[2] = {
            { 0.0, 0.0, 0.0 },
            { 0.0, 0.0, 0.0 }
        };

        /* If the ball leaves this box, we'll say it's gone out */
        vertex_3d_t ball_min = {
            -3.0, -3.0, -0.8 
        };
        vertex_3d_t ball_max = {
            3.0, 3.0, 0.8
        };

            
        scale(bat_verts, &bat_size, BAT_NVERTS);
        scale(ball_verts, &ball_size, BALL_NVERTS);
        
        init_ball(&ball_vec, &ball_v);

	while(1) {
            int rv;
            struct pollfd fds[2];
            int thing;
            fds[0].fd = io.fds[0];
            fds[0].revents = 0;
            fds[0].events = POLLIN;
            fds[1].fd = io.fds[1];
            fds[1].revents = 0;
            fds[1].events = POLLIN;
            rv = poll(fds, 2, 0 );
            if (fds[0].revents) { 
                thing = poll_hid(io.fds[0]);
                update_velocity( thing, &bat_vel[0] );
            }

            if (fds[1].revents) { 
                thing = poll_hid(io.fds[1]);
                update_velocity( thing, &bat_vel[1] );
                
            }
                
            /* Update the bats and balls, and make sure they remain in the playing area */
            ball_vec.x += ball_v.x;
            ball_vec.y += ball_v.y;
            ball_vec.z += ball_v.z;

            //            printf("Ball %g,%g,%g\n", ball_vec.x, ball_vec.y, ball_vec.z);
            
            if (ball_vec.x < ball_min.x ||
                ball_vec.x > ball_max.x ||
                ball_vec.y < ball_min.y ||
                ball_vec.y > ball_max.y ||
                ball_vec.z < ball_min.z ||
                ball_vec.z > ball_max.z ) {
                
                /* Game over! New game! */
                printf("New game!\n");
                init_ball(&ball_vec, &ball_v);
            }


            bat_pos[0].x += bat_vel[0].x;
            bat_pos[0].y += bat_vel[0].y;
            bat_pos[0].z += bat_vel[0].z;
            limit_bat ( & bat_pos[0] );


            bat_pos[1].x += bat_vel[1].x;
            bat_pos[1].y += bat_vel[1].y;
            bat_pos[1].z += bat_vel[1].z;
            limit_bat( &bat_pos[1] );




            memset(draw_buffer.buffer, 0, draw_buffer.buf_size);
            
            memcpy( translated_bat_verts[0], bat_verts, BAT_NVERTS * sizeof( vertex_3d_t ) );
            memcpy( translated_bat_verts[1], bat_verts, BAT_NVERTS * sizeof( vertex_3d_t ) );

            translate( translated_bat_verts[0], &bat_pos[0], BAT_NVERTS );
            translate( translated_bat_verts[1], &bat_pos[1], BAT_NVERTS );

            rotate(ball_verts, rotated_ball_verts, BALL_NVERTS);	 
            translate( rotated_ball_verts, &ball_vec, BALL_NVERTS);
            
            {
                int i;
                for (i =0;i < 2; ++i) { 
                    if (is_inside( rotated_ball_verts, BALL_NVERTS, translated_bat_verts[i], BAT_NVERTS)  ) {
                        /* Hit! */
                        reflect ( &ball_v, translated_bat_verts[i], BAT_NVERTS );
                    }
                }
            }

            project(rotated_ball_verts, projected_ball_verts, BALL_NVERTS);
            project( translated_bat_verts[0], projected_bat_verts[0], BAT_NVERTS );
            project( translated_bat_verts[1], projected_bat_verts[1], BAT_NVERTS );

            /* Now combine our vertices and lines */
            {
                 int nr_ball_lines = sizeof(ball_lines) / sizeof(connection_t); 
                 vertex_t display_list[ BALL_NVERTS + (2*BAT_NVERTS) ];
                 connection_t display_lines[ nr_ball_lines + (2* nr_ball_lines) ];
                 int i;

                 memcpy(& display_list[0], projected_ball_verts, sizeof(vertex_t) *BALL_NVERTS );
                 memcpy(& display_list[BALL_NVERTS], projected_bat_verts[0], sizeof(vertex_t) * BAT_NVERTS );
                 memcpy(& display_list[BALL_NVERTS + BAT_NVERTS], projected_bat_verts[1], sizeof(vertex_t) * BAT_NVERTS );
                 
                 memcpy(& display_lines[0], ball_lines, nr_ball_lines * sizeof(connection_t) );
                 memcpy(& display_lines[nr_ball_lines], ball_lines, nr_ball_lines * sizeof(connection_t));
                 memcpy(& display_lines[2*nr_ball_lines], ball_lines, nr_ball_lines * sizeof(connection_t));
                 for (i =0 ;i < nr_ball_lines; ++i) {
                     display_lines[nr_ball_lines + i].a += BALL_NVERTS;
                     display_lines[nr_ball_lines + i].b += BALL_NVERTS;
                 }

                 for (i =0 ;i < nr_ball_lines; ++i) {
                     display_lines[2*nr_ball_lines + i].a += BALL_NVERTS + BAT_NVERTS;
                     display_lines[2*nr_ball_lines + i].b += BALL_NVERTS + BAT_NVERTS;
                 }

                 draw_vertex_list(&draw_buffer, &format,
                                  display_list, display_lines, 3* nr_ball_lines );
            }

	
	  ao_play(device, draw_buffer.buffer, draw_buffer.buf_size);
	}

	/* -- Close and shutdown -- */
	ao_close(device);

	ao_shutdown();

  return (0);
}
