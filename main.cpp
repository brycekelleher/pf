#include <sys/time.h>
#include <unistd.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <math.h>

// simulation timestep in msecs
// eqv to 30 frames per second
#define SIM_TIMESTEP	32
#define TILE_SIZE	16

static unsigned int realtime;
static unsigned int simframe;
static unsigned int simtime;

// --------------------------------------------------------------------------------
// Input

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

// --------------------------------------------------------------------------------
// Move commands

float movex = 0.0f;
float movey = 0.0f;
bool buttonx = false;
bool buttonz = false;

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

	buttonx = keyactions[ka_x];
	buttonz = keyactions[ka_y];
}



// --------------------------------------------------------------------------------
// Game logic

// player state
float prevx, prevy;
float objx = 32.0f;
float objy = 128.0f;
float velx = 0.0f;
float vely = 0.0f;
float nextx = 0.0f;
float nexty = 0.0f;
int lastjump = 0;
static bool ladderstate = false;
static bool onground = false;

//
// Map
// 

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

// type flags
#define	SOLID	(1 << 0)
#define WATER	(1 << 1)
#define LADDER  (1 << 2)
#define FIELD   (1 << 3)

#if 0
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
#endif

static const char map[] = 
"################" \
"#wwwwwwwwwwwwww#" \
"#wwwwwwwwwwwwww#" \
"###########..###" \
"#.............f#" \
"#.1....11#####f#" \
"#....111######f#" \
"#.....11######f#" \
"#.....111#####f#" \
"#......l......f#" \
"#......l......f#" \
"#......l......f#" \
"#######l##....f#" \
"#......l......f#" \
"#......l......f#" \
"################";

// measured in tiles
static char Map_Tile(float x, float y)
{
	int xx = x / 16;
	int yy = y / 16;

	int addr = yy * 16 + xx;
	return map[addr];
}



static int Map_TileType(float x, float y)
{
	char tile = Map_Tile(x, y);
	int type = 0;

	if (tile == 'w')
		type |= WATER;
	if (tile == '#')
		type |= SOLID;
	if (tile == 'l')
		type |= LADDER;
	if (tile == 'f')
		type |= FIELD;

	return type;
}



bool Map_Solid(float x, float y)
{
	char tile = Map_Tile(x, y);
	if (tile == '#')
		return true;

	// jump through collisions
	if (tile == '1')
	{
		// True if and only if the current position and the next position
		// of the object are intersecting the tile boundary and the intersection
		// slop line
		const float slop = 1.0f / 16.0f;
		float line1 = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f);
		float line2 = ((floor((nexty - 4.0f) / 16.0f) + 1) * 16.0f) - slop;
		float u = (objy  - 4.0f);
		float v = (nexty - 4.0f);

		return (u >= v) && (u >= line2) && (v <= line1); 
	}

	// jump through collisions
	if (tile == 'l')
	{
		// True if and only if the current position and the next position
		// of the object are intersecting the tile boundary and the intersection
		// slop line
		const float slop = 1.0f / 16.0f;
		float line1 = ((floor((nextx - 4.0f) / 16.0f) + 1) * 16.0f);
		float line2 = ((floor((nextx - 4.0f) / 16.0f) + 1) * 16.0f) - slop;
		float u = (objx  - 4.0f);
		float v = (nextx - 4.0f);

		return (u >= v) && (u >= line2) && (v <= line1); 
	}

	return false;
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

//
// Player
//

static void Player()
{
	float newvelx = 0.0f;
	float newvely = 0.0f;
	int groundtype = Map_TileType(objx, objy - 4);

	// runnning logic
	if (onground)
	{
		// apply ground friction
		if (!movex)
		{
			velx *= 0.7f;
			if (fabs(velx) < 0.1f)
				velx = 0.0f;
		}

		float runvel = 0.25f * movex;
		if (buttonz)
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
		if (buttonx && simframe > lastjump + 5)
		{
			newvely += 5.0f;
			lastjump = simframe;
			printf("swim\n");
		}
	}

	// jump logic
	{
		if ((onground || ladderstate) && buttonx && simframe > lastjump + 10)
		{
			newvely += 5.0f;
			lastjump = simframe;
			ladderstate = false;
			//printf("jump\n");
		}

		// allow larger jumps
		if ((simframe < lastjump + 10) && buttonx && vely > 0.0f)
			newvely += 1.0f;
	}

	// ladder logic
	{
		// walk on and walk off ladder
		if (Map_OnLadder())
		{
			if (!ladderstate && (movey > 0.0f))
			{
				ladderstate = true;
				velx = vely = 0.0f;
			}

			// check for walk off ladder
			if (ladderstate && onground && (movey <= 0.0f))
				ladderstate = false;
		}

		// check for move off ladder
		if (ladderstate && !Map_OnLadder())
			ladderstate = false;

		// detach from ladder
		if (ladderstate && (movey < 0.0f) && buttonx)
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


//
// Physics / Movement code
//

static bool Move_OnGround()
{
	//bool tl = Map_Solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	//bool tr = Map_Solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	bool bl = Map_Solid(nextx + offsets[BOTTOML][0], nexty - 4.0f);
	bool br = Map_Solid(nextx + offsets[BOTTOMR][0], nexty - 4.0f);

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

#if 0
static bool PointTrace(float ox, float oy)
{
	char tile = Map_Tile(nextx + ox, nexty + oy);

	if (tile == '#')
		return true;

	// jump through collisions
	if (tile == '1')
	{
		// True if and only if the current position and the next position
		// of the object are intersecting the tile boundary and the intersection
		// slop line
		const float slop = 1.0f / 16.0f;
		float line1 = ((floor((nexty + oy) / 16.0f) + 1) * 16.0f);
		float line2 = ((floor((nexty + oy) / 16.0f) + 1) * 16.0f) - slop;
		float u = (objy  - oy);
		float v = (nexty - oy);

		return (oy < 0.0f) && (u >= v) && (u >= line2) && (v <= line1); 
	}

	return false;
}
#endif

static int Move_ClipCode(char tile)
{
	bool tl = Map_Solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	bool tr = Map_Solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	bool bl = Map_Solid(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]);
	bool br = Map_Solid(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]);

	tl &= (Map_Tile(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]) == tile);
	tr &= (Map_Tile(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]) == tile);
	bl &= (Map_Tile(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]) == tile);
	br &= (Map_Tile(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]) == tile);

	int code = (br << 3) | (bl << 2) | (tr << 1) | (tl << 0);

	return code;
}

// the concept here is to resolve the penetration by moving along the smallest axis
// based upon the classification of the intersection
static void Move_Clip_Solid()
{
	// 16 subpixel intersection slop
	//static const float slop = 1.0f / 64.0f;
	static const float slop = 1.0f / 16.0f;

	//bool tl = Map_Solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	//bool tr = Map_Solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	//bool bl = Map_Solid(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]);
	//bool br = Map_Solid(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]);

	//tl &= Map_Tile(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]) == '#';
	//tr &= Map_Tile(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]) == '#';
	//bl &= Map_Tile(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]) == '#';
	//br &= Map_Tile(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]) == '#';

	//int code = (br << 3) | (bl << 2) | (tr << 1) | (tl << 0);
	int code = Move_ClipCode('#');
	//printf("\rcode %i  (%i %i %i %i) " , code, tl, tr, bl, br);
	//printf("code %i\n" , code);
	//fflush(stdout);

	if (code == 0x5)
	{
		// left	
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x - slop;
		nextx += dx;
		velx = 0;
	}
	else if (code == 0xa)
	{
		// right
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f) - slop;
		nextx -= dx;
		velx = 0;
	}
	else if (code == 0x3)
	{
		// top
		float y = nexty + 4.0;
		float dy = y - (floor(y / 16.0f) * 16.0f) - slop;
		nexty -= dy;
		vely = 0;
	}
	else if (code == 0xc)
	{
		// bottom
		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		nexty += dy;
		vely = 0;
	}
	else if (code == 0x4)
	{
		// convex bottom left
		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x - slop;

		if (dx < dy)
		{
			// push out on x axis (left)
			nextx += dx;
			if (velx < 0.0f)
				velx = 0;
		}
		else
		{
			// push out on y axis
			nexty += dy;;
			if (vely < 0.0f)
				vely = 0;
		}
	}
	else if (code == 0x8)
	{
		// convex bottom right
		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f) - slop;

		if (dx < dy)
		{
			nextx -= dx;
			if (velx > 0.0f)
				velx = 0;
		}
		else
		{
			nexty += dy;
			if (vely < 0.0f)
				vely = 0;
		}
	}
	else if (code == 0x1)
	{
		// convex top left	
		float y = nexty + 4.0;
		float dy = y - (floor(y / 16.0f) * 16.0f) - slop;
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x - slop;

		if (dx < dy)
		{
			nextx += dx;
			if (velx < 0.0f)
				velx = 0;
		}
		else
		{
			nexty -= dy;
			if (vely > 0.0f)
				vely = 0;
		}
	}
	else if (code == 0x2)
	{
		// convex top right
		float y = nexty + 4.0;
		float dy = y - (floor(y / 16.0f) * 16.0f) + slop;
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f) + slop;
		
		if (dx < dy)
		{
			nextx -= dx;
			if (velx > 0.0f)
				velx = 0;
		}
		else
		{
			nexty -= dy;
			if (vely > 0.0f)
				vely = 0;
		}
	}
	else if (code == 0x7)
	{
		// concave top left
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x;
		nextx += dx;
		velx = 0;

		float y = nexty + 4.0;
		float dy = y - (floor(y / 16.0f) * 16.0f);
		nexty -= dy;
		vely = 0;
	}
	else if (code == 0xb)
	{
		// concave top right
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f);
		nextx -= dx;
		velx = 0;

		float y = nexty + 4.0;
		float dy = y - (floor(y / 16.0f) * 16.0f);
		nexty -= dy;
		vely = 0;
	}
	else if (code == 0xd)
	{
		// concave bottom left
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x;
		nextx += dx;
		velx = 0;

		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		nexty += dy;
		vely = 0;
	}
	else if (code == 0xe)
	{
		// concave bottom right
		float x = nextx + 4.0;
		float dx = x - (floor(x / 16.0f) * 16.0f);
		nextx -= dx;
		velx = 0;

		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		nexty += dy;
		vely = 0;
	}
	else if (code == 0xf)
	{
		nextx += 1;
		velx = 0;
		vely = 0;
	}
}

// This is the Move_Clip for a one-way tile
static void Move_Clip_OneWay()
{
	// 16 subpixel intersection slop
	//static const float slop = 1.0f / 64.0f;
	static const float slop = 1.0f / 16.0f;
	//static const float slop = 0.0f;

	//bool tl = Map_Solid(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]);
	//bool tr = Map_Solid(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]);
	//bool bl = Map_Solid(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]);
	//bool br = Map_Solid(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]);

	//tl &= Map_Tile(nextx + offsets[TOPL][0], nexty + offsets[TOPL][1]) == '1';
	//tr &= Map_Tile(nextx + offsets[TOPR][0], nexty + offsets[TOPR][1]) == '1';
	//bl &= Map_Tile(nextx + offsets[BOTTOML][0], nexty + offsets[BOTTOML][1]) == '1';
	//br &= Map_Tile(nextx + offsets[BOTTOMR][0], nexty + offsets[BOTTOMR][1]) == '1';

	//int code = (br << 3) | (bl << 2) | (tr << 1) | (tl << 0);
	int code = Move_ClipCode('1');

	if (code == 0x7 || code == 0xb || code == 0xc || code == 0x8 || code == 0x4 || code == 0xd || code == 0xe || code == 0xf)
	{
		//printf("collide %i\n", code);
		float y = nexty - 4.0;
		float dy = ((floor(y / 16.0f) + 1) * 16.0f) - y - slop;
		nexty += dy;
		vely = 0;
	}
}

static void Move_Clip_OneX()
{
	static const float slop = 1.0f / 16.0f;

	int code = Move_ClipCode('l');

	if (code == 0x5 || code == 0x1 || code == 0x4)
	{
		// left	
		float x = nextx - 4.0;
		float dx = ((floor(x / 16.0f) + 1) * 16.0f) - x - slop;
		nextx += dx;
		velx = 0;
	}
}

static void Move_Clip()
{
	Move_Clip_OneWay();

	Move_Clip_OneX();

	// solid must be resolved last
	Move_Clip_Solid();
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
	// fixme: add explicit ladder physics
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

	prevx = objx;
	prevy = objy;
	objx = nextx;
	objy = nexty;

	//printf("obj %f, %f\n", objx, objy);
	// evaluate the 'ground state'
	onground = Move_OnGround();
	//printf("onground %s\n", (onground ? "yes" : "no"));
}

// --------------------------------------------------------------------------------
// Rendering

static float *LookupColor(int x, int y)
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

	char tile = Map_Tile(x, y);
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



static void DrawTile(int x, int y, float color[3])
{
	const float size = TILE_SIZE;

	glColor3fv(color);

	x *= size;
	y *= size;

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f(x + 0.0f, y + 0.0f);
	glVertex2f(x + size, y + 0.0f);
	glVertex2f(x + 0.0f, y + size);
	glVertex2f(x + size, y + size);
	glEnd();
}



static void DrawTiles()
{
	for (int i = 0; i < 256; i++)
	{
		int y = i / 16;
		int x = i % 16;
		
		float *c = LookupColor(x * 16, y * 16);

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
// --------------------------------------------------------------------------------
// Main

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

	// signal a rendering update
	glutPostRedisplay();
}



int main(int argc, char *argv[])
{
	// glutmain
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


