#include <sys/time.h>
#include <unistd.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <math.h>

// simulation timestep in msecs
// eqv to 30 frames per second
#define SIM_TIMESTEP	32

unsigned int Sys_Milliseconds (void)
{
	struct timeval	tp;
	static int		secbase;
	static int		curtime;

	gettimeofday(&tp, NULL);

	if (!secbase)
	{
		secbase = tp.tv_sec;
	}

	curtime = (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;

	return curtime;
}

void Sys_Sleep(unsigned int msecs)
{
	usleep(msecs * 1000);
}

static unsigned int realtime;

static unsigned int simframe;
static unsigned int simtime;

float prevx, prevy;
float objx = 32.0f;
float objy = 128.0f;
float velx = 0.0f;
float vely = 0.0f;
float movex = 0.0f;
float movey = 0.0f;
int lastjump = 0;
float nextx = 0.0f;
float nexty = 0.0f;

static const char map[] = 
"################" \
"#wwwwwwwwwwwwww#" \
"#wwwwwwwwwwwwww#" \
"###########..###" \
"#.............f#" \
"#.1....11.....f#" \
"#....111......f#" \
"#.............f#" \
"#.....111#####f#" \
"#......l......f#" \
"#......l......f#" \
"#......l......f#" \
"#######l##....f#" \
"#......l......f#" \
"#......l......f#" \
"################";

// measured in tiles
char mapcell(int x, int y)
{
	int addr = y * 16 + x;
	return map[addr];
}

static void DrawTile(int x, int y, float *color)
{
	int i;
	float scale = 8.0f;

	glColor3fv(color);

	x *= 16.0f;
	y *= 16.0f;

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f(x +  0.0f, y +  0.0f);
	glVertex2f(x + 16.0f, y +  0.0f);
	glVertex2f(x +  0.0f, y + 16.0f);
	glVertex2f(x + 16.0f, y + 16.0f);
	glEnd();
}

static float* LookupColor(int x, int y)
{
	static float colors[6][3] = 
	{
		{ 1, 0, 0 },
		{ 0, 0, 1 },
		{ 1, 1, 1 },
		{ 1, 1, 0 },
		{ 0, 1, 1 },
		{ 0.5, 0, 0 },
	};

	char tile = mapcell(x, y);
	if (tile == '#')
		return colors[0];
	else if (tile == 'w')
		return colors[1];
	else if (tile == 'l')
		return colors[3];
	else if (tile == 'f')
		return colors[4];
	else if (tile == '1')
		return colors[5];
	else
		return colors[2];
}

static void DrawTiles()
{
	for (int i = 0; i < 256; i++)
	{
		int y = i / 16;
		int x = i % 16;
		
		float *c = LookupColor(x, y);

		DrawTile(x, y, c);
	}
}

static void DrawObject(float x, float y)
{
	float xx = x - 4.0f;
	float yy = y - 4.0f;

	glColor3f(1, 0, 1);

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f(xx + 0.0f, yy + 0.0f);
	glVertex2f(xx + 8.0f, yy + 0.0f);
	glVertex2f(xx + 0.0f, yy + 8.0f);
	glVertex2f(xx + 8.0f, yy + 8.0f);
	glEnd();
}

static void ReshapeFunc(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glOrtho(0, 256, 0, 256, -1, 1);

	glViewport(0, 0, w, h);
}

static void DisplayFunc()
{
	glClearColor(0.3, 0.3, 0.3, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	DrawTiles();

	DrawObject(objx, objy);

	glutSwapBuffers();
}

enum keyaction_t
{
	ka_left,
	ka_right,
	ka_up,
	ka_down,
	ka_x,
	ka_y,
	NUM_KEY_ACTIONS
};

static bool keyactions[NUM_KEY_ACTIONS];

static void KeyDown(int key, int x, int y)
{
}

static void KeyUp(int key, int x, int y)
{
}

#if 0
static void KeyDown(int key, int x, int y)
{
	printf("%i\n", key);

}

static void KeyUp(int key, int x, int y)
{
}
#endif

static void KeyDownFunc(unsigned char key, int x, int y)
{
	if (key == 'a')
		keyactions[ka_left] = true;
	if (key == 'd')
		keyactions[ka_right] = true;
	if (key == 'w')
		keyactions[ka_up] = true;
	if (key == 's')
		keyactions[ka_down] = true;
	if (key == 'x')
		keyactions[ka_x] = true;
	if (key == 'z')
		keyactions[ka_y] = true;
}
static void KeyUpFunc(unsigned char key, int x, int y)
{
	if (key == 'a')
		keyactions[ka_left] = false;
	if (key == 'd')
		keyactions[ka_right] = false;
	if (key == 'w')
		keyactions[ka_up] = false;
	if (key == 's')
		keyactions[ka_down] = false;
	if (key == 'x')
		keyactions[ka_x] = false;
	if (key == 'z')
		keyactions[ka_y] = false;
}
static void SpecialDownFunc(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
		keyactions[ka_left] = true;
	if (key == GLUT_KEY_RIGHT)
		keyactions[ka_right] = true;
	if (key == GLUT_KEY_UP)
		keyactions[ka_up] = true;
	if (key == GLUT_KEY_DOWN)
		keyactions[ka_down] = true;
}
static void SpecialUpFunc(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
		keyactions[ka_left] = false;
	if (key == GLUT_KEY_RIGHT)
		keyactions[ka_right] = false;
	if (key == GLUT_KEY_UP)
		keyactions[ka_up] = false;
	if (key == GLUT_KEY_DOWN)
		keyactions[ka_down] = false;
}

static void BuildMoveCommand()
{
	movex = 0;
	if (keyactions[ka_left])
		movex -= 1;
	if (keyactions[ka_right])
		movex += 1;

	movey = 0;
	if (keyactions[ka_down])
		movey -= 1;
	if (keyactions[ka_up])
		movey += 1;
}

//________________________________________
// map stuff

// type flags
#define	SOLID	(1 << 0)
#define WATER	(1 << 1)
#define LADDER  (1 << 2)
#define FIELD   (1 << 3)

// measured in pixels
bool map_solid(float x, float y)
{
	int xx = x / 16;
	int yy = y / 16;

	if (mapcell(xx, yy) == '#')
		return true;

	// jump through collisions
#if 0
	{
		// true if and only if
		// we're a single tile one way
		// - we're falling
		// - we're crossing a tile boundary
		int prevcell = floor((objy  - 4.0f) / 16.0f);
		int nextcell = floor((nexty - 4.0f) / 16.0f);
		printf("objy=%f, nexty=%f, vely=%f, nextcell=%i, prevcell=%i\n", objy, nexty, vely, nextcell, prevcell);
		if (mapcell(xx, yy) == '1' && vely < 0.0f && (prevcell > nextcell))
		{
			printf("1way: falling\n");
			return true;
		}

		// true if standing
		// if velocity is zero
		// if object center is in a different tile
		// fixme: there's probably more robust conditions than this...
		//int ocell = floor((objy - 3.0f) / 16.0f);
		//int gcell = floor((objy - 4.0f) / 16.0f);
		//printf("objy=%f, nexty=%f, vely=%f, ocell=%i, gcell=%i\n", objy, nexty, vely, ocell, gcell);
		//if (mapcell(xx, yy) == '1' && (ocell > gcell))
		//{
		//	printf("1way: standing\n");
		//	return true;
		//}
	}
#endif
#if 0
	printf("objy=%f, nexty=%f, vely=%f\n", objy, nexty, vely);
	{
		const float slop = 1.0f / 16.0f;
		float slopline = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f);
		float u = (objy  - 4.0f);
		float v = (nexty - 4.0f);
		printf("slopline=%f, u=%f, v=%f\n", slopline, u, v);
		if (mapcell(xx, yy) == '1' && (u >= slopline) && (v <= slopline))
		{
			printf("hit 1\n");
			return true;
		}
	}

	{
		const float slop = 1.0f / 16.0f;
		float slopline = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f) - slop;
		float u = (objy  - 4.0f);
		float v = (nexty - 4.0f);
		printf("slopline=%f, u=%f, v=%f\n", slopline, u, v);
		if (mapcell(xx, yy) == '1' && (u >= slopline) && (v <= slopline))
		{
			printf("hit 1\n");
			return true;
		}
	}
#endif
	//printf("objy=%f, nexty=%f, vely=%f\n", objy, nexty, vely);
	{
		// True if and only if the current position and the next position
		// of the object are intersecting the tile boundary and the intersection
		// slop line
		const float slop = 1.0f / 16.0f;
		float line1 = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f);
		float line2 = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f) - slop;
		float u = (objy  - 4.0f);
		float v = (nexty - 4.0f);
		//printf("line1=%f, line2=%f, u=%f, v=%f\n", line1, line2, u, v);
		if (mapcell(xx, yy) == '1')
		{
#if 0
			 return ((u >= line1) && (v <= line1)) ||
			        ((u >= line2) && (v <= line2)) ||
			        //((u >= line2) && (v <= line1)) ||
			        ((u >= line2) && (v <= line1));
#endif
			return (u >= v) && (u >= line2) && (v <= line1); 
		}
	}
#if 0
	// true every 128 frames
	if (mapcell(xx, yy) == '1' && (simframe & 128))
		return true;
#endif		

	return false;
}


#define LEFT	0
#define RIGHT	1
#define BOTTOMC	2
#define BOTTOML 3
#define BOTTOMR 4
#define TOPC	5
#define TOPL	6
#define TOPR	7

static int offsets[][2] =
{
	{ -4,  0 },
	{  4,  0 },
	{  0, -4 },
	{ -4, -4 },
	{  4, -4 },
	{  0,  4 },
	{ -4,  4 },
	{  4,  4 }
};

static int Map_TileType(float x, float y)
{
	int xx = x / 16;
	int yy = y / 16;
	char cell = mapcell(xx, yy);
	int type = 0;

	if (cell == 'w')
		type |= WATER;
	if (cell == '#')
		type |= SOLID;
	if (cell == 'l')
		type |= LADDER;
	if (cell == 'f')
		type |= FIELD;

	return type;
}

static bool Map_OnGround()
{
#if 0
	bool s1 = map_solid(objx + offsets[BOTTOMC][0], objy + offsets[BOTTOMC][1]);
	bool s2 = map_solid(objx + offsets[BOTTOML][0], objy + offsets[BOTTOML][1]);
	bool s3 = map_solid(objx + offsets[BOTTOMR][0], objy + offsets[BOTTOMR][1]);

	return s1 | s2 | s3;
#endif
	bool tl = map_solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	bool tr = map_solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	bool bl = map_solid(nextx + offsets[BOTTOML][0], nexty - 4.0f);
	bool br = map_solid(nextx + offsets[BOTTOMR][0], nexty - 4.0f);

	int code = (br << 3) | (bl << 2);// | (tr << 1) | (tl << 0);
	//printf("code=%i\n", code);

	if (code == 0x4)
	{
		static const float slop = 1.0f / 16.0f;

		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x - slop;

		//printf("dx=%f, dy=%f\n", dx, dy);

		return (dy < dx);
	}
	else if (code == 0x8)
	{
		static const float slop = 1.0f / 16.0f;

		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f) - slop;
		//printf("dx=%f, dy=%f\n", dx, dy);

		return (dy < dx);
	}
	else
	{
		return (code == 0xc) || (code == 0xd) || (code == 0xe);
	}
}

static bool Map_OnContents(int type)
{
	bool tl = (Map_TileType(nextx - 4, nexty + 4) & type) != 0;
	bool tr = (Map_TileType(nextx + 4, nexty + 4) & type) != 0;
	bool bl = (Map_TileType(nextx - 4, nexty - 4) & type) != 0;
	bool br = (Map_TileType(nextx + 4, nexty - 4) & type) != 0;
	
	return tl || tr || bl || br;
}

// oversample on the bottom to allow to get above the last ladder tile
// fixme: need to stop at the top
static bool Map_OnLadder()
{
	return Map_OnContents(LADDER);
}

static bool Map_OnWater()
{
	return Map_OnContents(WATER);
}

static bool ladderstate = false;
static bool onground = false;

static void Player()
{
	float newvelx = 0.0f;
	float newvely = 0.0f;
	int groundtype = Map_TileType(objx, objy - 4);

	// runnning logic
	if (onground)
	{
		// apply ground friction
		//if (!keyactions[ka_left] && !keyactions[ka_right])
		if (!movex)
		{
			velx *= 0.7f;
			if (fabs(velx) < 0.1f)
				velx = 0.0f;
		}

		float runvel = 0.25f * movex;
		if (keyactions[ka_y])
			runvel *= 2;

		// apply input move
		newvelx += runvel;
	}

	// air control
	if (!onground)
	{
		newvelx += 0.1f * movex;
	}

	// swimming
	if (groundtype & WATER)
	{
		if (keyactions[ka_x] && simframe > lastjump + 5)
		{
			newvely += 5.0f;
			lastjump = simframe;
			printf("swim\n");
		}
	}

	// jump logic
	{
		if ((onground || ladderstate) && keyactions[ka_x] && simframe > lastjump + 10)
		{
			newvely += 5.0f;
			lastjump = simframe;
			ladderstate = false;
			//printf("jump\n");
		}

		// allow larger jumps
		if ((simframe < lastjump + 10) && keyactions[ka_x] && vely > 0.0f)
			newvely += 1.0f;
	}

	// ladder logic
	{
		// walk on and walk off ladder
		if (Map_OnLadder())
		{
			if (!ladderstate && keyactions[ka_up])
			{
				ladderstate = true;
				velx = vely = 0.0f;
			}

			// check for walk off ladder
			if (ladderstate && onground && !keyactions[ka_up])
				ladderstate = false;
		}

		// check for move off ladder
		if (ladderstate && !Map_OnLadder())
			ladderstate = false;

		// detach from ladder
		if (ladderstate && keyactions[ka_down] && keyactions[ka_x])
			ladderstate = false;

		// ladder movement
		if (ladderstate)
		{
			velx = vely = 0;

			if (movex > 0)
				velx = 1.0f;
			else if (movex < 0)
				velx = -1.0f;

			if (movey > 0)
				vely = 1.0f;
			else if (movey < 0)
				vely = -1.0f;
		}
	}

	velx += newvelx;
	vely += newvely;
}

// ________________________________________________________________________________ 
// Physics / Movement code

// the concept here is to resolve the penetration by moving along the smallest axis
// based upon the classification of the intersection

// This is the Move_Clip for a one-way tile
static void Move_Clip()
{
	// 16 subpixel intersection slop
	//static const float slop = 1.0f / 64.0f;
	static const float slop = 1.0f / 16.0f;
	//static const float slop = 0.0f;
	static int prevcode = 0;

	bool tl = map_solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	bool tr = map_solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	bool bl = map_solid(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]);
	bool br = map_solid(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]);

	int code = (br << 3) | (bl << 2) | (tr << 1) | (tl << 0);

	int prevcell = floor((objy  - 4.0f) / 16.0f);
	int nextcell = floor((nexty - 4.0f) / 16.0f);

	//printf("code %i  (%i %i %i %i) prevcell=%i, nextcell=%i, velx=%f, vely=%f\n" , code, tl, tr, bl, br, prevcell, nextcell, velx, vely);
	//fflush(stdout);

	//int prevcell = mapcell(objx / 16.0f, (objy + 4.0f) / 16.0f);
	//int nextcell = mapcell(nextx / 16.0f, (nexty + 4.0f) / 16.0f);

	//if (prevcell != nextcell)
	//	printf("notequal\n");

	//printf("prevcell=%i, nextcell=%i\n", prevcell, nextcell);

	if (code == 0x7 || code == 0xb || code == 0xc || code == 0x8 || code == 0x4 || code == 0xd || code == 0xe || code == 0xf)
	{
		//printf("collide %i\n", code);
		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		nexty += dy;
		vely = 0;
	}
}

static void Move_Air()
{
	// apply gravity
	vely -= 1;

	if (vely <= -5)
		vely = -5;

	// clamp the velocities
	if (velx >= 5)
		velx = 5;
	if (velx <= -5)
		velx = -5;
}

static void Move_Water()
{
	// apply sinking
	vely -= 1.0f;

	float maxy = 2.0f;
	if (vely <= -maxy)
		vely = -maxy;

	float maxx = 2.0f;
	if (velx >= maxx)
		velx = maxx;
	if (velx <= -maxx)
		velx = -maxx;
}

static void Movement()
{
	int type = Map_TileType(objx, objy);

	// figure out which physics to apply
	if (!ladderstate)
	{
		if (type & WATER)
			Move_Water();
		else
			Move_Air();

		// field
		if (Map_OnContents(FIELD) && (vely < 10.0f))
			vely += 1.0f;
	}

	// try the move
	nextx = objx + velx;
	nexty = objy + vely;

	// clip the move
	Move_Clip();

	objx = nextx;
	objy = nexty;

	//printf("obj %f, %f\n", objx, objy);
	// evaluate the 'ground state'
	onground = Map_OnGround();
	//printf("onground %s\n", (onground ? "yes" : "no"));
}



static void SimRunFrame()
{
	//printf("===== simrunframe =====\n");
	simframe++;
	simtime = simframe * SIM_TIMESTEP;

	BuildMoveCommand();

	Player();

	Movement();
}

static void MainLoopFunc()
{
	unsigned int newtime = Sys_Milliseconds();

	// yield the thread if no time has advanced
	if (newtime == realtime)
	{
		Sys_Sleep(0);
		return;
	}

	realtime = newtime;

	// run the simulation code
	if (simtime < realtime)
		SimRunFrame();

	// run the rendering code
	glutPostRedisplay();
}

int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	glutInitWindowSize(512, 512);
	glutCreateWindow("test window");

	glutDisplayFunc(DisplayFunc);
	glutReshapeFunc(ReshapeFunc);
	glutIdleFunc(MainLoopFunc);
	glutKeyboardFunc(KeyDownFunc);
	glutKeyboardUpFunc(KeyUpFunc);
	glutSpecialFunc(SpecialDownFunc);
	glutSpecialUpFunc(SpecialUpFunc);
	glutMainLoop();

	return 0;
}


