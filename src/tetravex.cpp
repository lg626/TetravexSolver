#include "toplevel.h"
//Author @Luiza Georgieva

#define WIDTH 36
#define TILE_LEN 4
#define AVAILABLE 0
#define UNAVAILABLE 1
#define TOP 0
#define RIGHT 1
#define BOTTOM 2
#define LEFT 3
#define UNASSIGNED 0
#define RIGHTCHILD 0
#define DOWNCHILD 1
#define DEFAULT 10
#define TILE 0
#define ORIENTATION 1
#define END_OF_SEARCH 0xffffffff
#define MAXSTORAGE 36
#define MAXCOLOURS 11

//Input data storage
uint32 inputdata[WIDTH][TILE_LEN];
uint6 colourmap[MAXCOLOURS][MAXCOLOURS][MAXSTORAGE][2];
uint3 size;

unsigned numtiles = 0;
unsigned solutions = 0;

void popAndDelete(int tile, uint6* stack, uint1* status, uint2* rotations, uint6 positions[36][2],uint6 tries[36][2],uint6 puzzle[6][6],int &stacktop,bool &stopsearch, int &nexttoexpand);
void rotate(int tile,int value,int orientation, int side, uint2* rotations);
void match(int x, int y, int parent,int firstcolour,int secondcolour, int side, int child, uint2* rotations, uint1* status, uint6* stack, uint6 puzzle[6][6], uint6 positions[36][2],uint6 tries[36][2],bool &matched, int &stacktop, bool &stopsearch, bool &foundsolution);

//Top-level function
void toplevel(hls::stream<uint32> &input, hls::stream<uint32> &output) {
#pragma HLS INTERFACE ap_fifo port=input
#pragma HLS INTERFACE ap_fifo port=output
#pragma HLS RESOURCE variable=input core=AXI4Stream
#pragma HLS RESOURCE variable=output core=AXI4Stream
#pragma HLS INTERFACE ap_ctrl_none port=return

////////////////////////////////////////////////////////////////////////
	int comptile;			//compatible tile id
	int currtile;			//currently used root tile (puzzle(0,0))
	int fcolour,scolour,index; 	//hashmap variables
////////////////////////////////////////////////////////////////////////

	size = input.read();		//read puzzle size
	numtiles = size*size;

	solutions = input.read();	//read requested solutions

	for(int i = 0; i < 11; i++)
	{
		for(int j = 0; j < 11; j++)
		{
			colourmap[i][j][0][TILE] = 0;		//clear hashmap indices
		}
	}

	readloopi:for(int i = 0; i < numtiles; i++)
	{
		readloopj: for(int j = 0; j < TILE_LEN; j++)
		{
			inputdata[i][j] = input.read();		//read data
		}
	}


/*Hash Map of tiles containing a succession of two colours. First colour index is always stored and used to compute orientation. */
	hashloopi:for(int i = 0; i < numtiles; i++)
	{
		hashloopj:for(int j = 0; j < TILE_LEN; j++)
		{
			fcolour = inputdata[i][j];
			scolour = inputdata[i][(j+1) % TILE_LEN];
			index   = colourmap[fcolour][scolour][0][TILE] + 1;

			if(index < MAXSTORAGE)
			{
				colourmap[fcolour][scolour][index][TILE] = i;
				colourmap[fcolour][scolour][index][ORIENTATION] = j;
				colourmap[fcolour][scolour][0][TILE] = index;
			}

			index   = colourmap[fcolour][DEFAULT][0][TILE] + 1;
			if(index < MAXSTORAGE)
			{
				colourmap[fcolour][DEFAULT][index][TILE] = i;
				colourmap[fcolour][DEFAULT][index][ORIENTATION] = j;
				colourmap[fcolour][DEFAULT][0][TILE] = index;
			}
		}
	}

/* Parallelised for loop trying different tiles as a root tile. */
master:for(currtile = 0; currtile < 2;currtile++)
{
	uint3 x;				//tile coordinates
	uint3 y;
	uint6 parent;				//parent tile id

	uint6 stack[36];			//stack containing the last pushed tile
	uint6 puzzle[6][6];			//puzzle solution matrix
	uint6 positions[36][2];			//x and y coordinates of each tile in the pizzle
	uint6 tries[36][2];			//history -> last tried index for right and bottom neightbours
	uint2 rotations[36];			//rotations of the currently used tiles
	uint1 status[36];			//availability status

	int stacktop 		= 1;		//initialise stack top (or in this case the next empty position)
	int nexttoexpand 	= 0;		//next tile to expand (look for neighbours to)
	bool matched 		= false; 	//flag for a matching tile found
	bool stopsearch 	= false;	//flag for stopping the search
	bool foundsolution 	= false;	//flag for output ready

	int neighbourtile;			//neighbour tile
	int child_x;				//child coordinates
	int child_y;

	bool bottomedge;			//check if at bottom edge
	bool rightedge;				//check if at right edge

	if(solutions == 0) break;		//if requested solutions is 0 stop

	for(int i = 0; i < size; i++)		//clear puzzle
		for(int j = 0; j < size; j++)
			puzzle[i][j] = 0;

	stopsearch = false;			//reset flag

	puzzle[0][0] 	= currtile + 1; 	//put root tile
	positions[0][0] = 0;			//store coordinates
	positions[0][1] = 0;

	stack[0] = currtile;			//push it on the stack
	nexttoexpand = 0;			//reset next to expand pointer
	stacktop = 1;				//set stack top

	for(int i = 0; i < numtiles; i++)
	{
		status[i] = AVAILABLE;		//reset
		tries[i][0] = 0;
		tries[i][1] = 0;
		rotations[i] = 0;

	}

	status[currtile] = UNAVAILABLE;	//mark root tile as used

	while(!stopsearch)
	{
		x	= positions[nexttoexpand][0];						//tile coordinates
		y	= positions[nexttoexpand][1];

		fcolour = DEFAULT;
		scolour = DEFAULT;

		bottomedge 		= (x == size - 1) ? true:false;
		rightedge 		= (y == size - 1) ? true:false;

		if(rightedge)
		{
			child_x = x + 1;
			child_y = 0;
			parent	=  puzzle[x][0] - 1;						//if at right edge get parent is at the left edge (with y = 0)
		}
		else
		{
			child_x = x;
			child_y = y + 1;
			parent	=  puzzle[x][y] - 1;						//else parent is always at the right
			//tries[parent][DOWNCHILD] = 0;
		}

		matched 		= false;

		if(puzzle[child_x][child_y] == UNASSIGNED)					//check if child tile is unassigned
		{
			if(rightedge) fcolour = inputdata[parent][(4 + BOTTOM - rotations[parent]) % 4];	//get the bottom value of the up tile
			else
			{
				fcolour = inputdata[parent][(4 + RIGHT - rotations[parent]) % 4];				//get the right of the parent tile
				if(x != 0)
				{
					neighbourtile 	= puzzle[child_x - 1][child_y] - 1;							//get top neighbour tile
					scolour 		= inputdata[neighbourtile][(4 + BOTTOM  - rotations[neighbourtile]) % 4]; //get the bottom value of the top neighbour tile
				}
			}
		}

		match(child_x,child_y,parent,fcolour,scolour,(rightedge)? TOP : LEFT,(rightedge)? DOWNCHILD : RIGHTCHILD,rotations,status,stack,puzzle,positions,tries,matched, stacktop, stopsearch, foundsolution);

		//if no match found - pop top of the stack, else move on
		if(!matched) popAndDelete(stack[stacktop-1],stack, status, rotations, positions, tries , puzzle, stacktop, stopsearch,nexttoexpand);
		else nexttoexpand++;

		if(foundsolution) //if solution is found -> output it and reset the flag
		{
			sendloopi: for(int i = 0; i < size; i++) {
				sendloopj:for(int j = 0; j < size; j++){
					comptile = puzzle[i][j] - 1;
					sendloopz:for(int z = 0; z < TILE_LEN; z++){
						output.write(inputdata[comptile][(z + 4 - rotations[comptile]) % 4]);
					}
				}
			}
			popAndDelete(stack[1],stack, status, rotations, positions, tries , puzzle, stacktop, stopsearch,nexttoexpand); //pop all apart from root tile and start again
			nexttoexpand--;
			if(solutions > 1) solutions = solutions -1;		//decrement solutions requested
			else
			{
				stopsearch = true;
				solutions--;
			}

			foundsolution = false;
		}
	}
}

output.write((uint32)END_OF_SEARCH);

}

/*********************************************************************************************************
*Pop from stack and delete from puzzle the requested tile and everything before it in the stack          *
*																										 *
*@params - tile index and its surrounding information													 *
*																										 *
**********************************************************************************************************/
void popAndDelete(int tile, uint6* stack, uint1* status, uint2* rotations, uint6 positions[36][2],uint6 tries[36][2],uint6 puzzle[6][6],int &stacktop,bool &stopsearch, int &nexttoexpand)
{
	int parent = 0;
	bool stop = false;

	poploopi: for (int i = stacktop-1; (i >= 0 && !stop); i--)	 //pop all items before tile including the tile itself
	{
		int nx	= positions[i][0];				 //tile coordinates in puzzle
		int ny	= positions[i][1];

		if(stack[i] == tile)
		{
			stacktop = i;					//update index to top of the stack
			stop = true;
		}

		status[stack[i]] = AVAILABLE;				//make tile available
		puzzle[nx][ny] = UNASSIGNED;				//delete from puzzle
		tries[stack[i]][DOWNCHILD] = 0;
		tries[stack[i]][RIGHTCHILD] = 0;			//delete the history of tried children
		rotations[stack[i]] = 0;				//remove number of rotations
		stack[i] = 0;						//pop off the stack

		if(nx == 0 && ny == 0)					//if root tile
		{
			if(tile < size*size) tile = tile + 1;		//put next tile as root tile
			else tile = 0;
			status[tile] = UNAVAILABLE;			//make the current tile unavailable
			puzzle[0][0] = tile + 1;			//put new root tile in the puzzle
			stack[0] = tile;				//and in the stack

			positions[0][0] = 0;				//update positions
			positions[0][1] = 0;

			stacktop = 1;					//update top of the stack
		} // root tile is popped off -> assign root tile
	}

	nexttoexpand = stacktop - 1;
}

/*
* Compute the rotation needed for a tile to fit in the puzzle
*
@param - tile, side value, current orientation, side needed and rotations array
*
*/
void rotate(int tile,int value,int orientation, int side, uint2* rotations)
{
	int numrot =  orientation - side;

	if(numrot == 3 || numrot == -1) numrot = 1;
	else if(numrot == -3 || numrot == 1) numrot = 3;
	else if(numrot == -2)    numrot = 2;

	rotations[tile] = numrot;
}

/*********************************************************************************************************
* Find a match to a tile.																				 *
*																										 *
* @param : x,y 						= coordinates														 *
*		   parent 					= parent tile index,												 *
*		   firtcolour,secondcolour  = colour succession,												 *
*		   side 					= side to fit,														 *
*		   child					= BOTTOM or RIGHT child												 *
*																										 *
**********************************************************************************************************/
void match(int x, int y, int parent,int firstcolour,int secondcolour, int side, int child, uint2* rotations, uint1* status, uint6* stack, uint6 puzzle[6][6], uint6 positions[36][2],uint6 tries[36][2],bool &matched, int &stacktop, bool &stopsearch, bool &foundsolution)
{
	int tile;
	int comptile = colourmap[firstcolour][secondcolour][0][TILE];	 //index of compatible tile [may be 0 if there is no tiles with colours firscolour succeeded by second colour]

	if(comptile !=0) 												// if hashmap contains tiles with requested description
	{
		if(tries[parent][child] != 0 && comptile !=0) 		//if there is history of tries
		{
			comptile  = tries[parent][child] - 1; 		//decrement pointer
		}

		matchloopi: for(int i = comptile; i > 0; i--)
		{
			tile = colourmap[firstcolour][secondcolour][i][TILE];	//get tile index in the inputdata

			if(status[tile] == AVAILABLE)				//if flag indicates the tile is available
			{
				int leftedge;
				int orientation = colourmap[firstcolour][secondcolour][i][ORIENTATION];

				stack[stacktop] = tile;				//put tile on the stack
				rotate(tile,firstcolour,orientation,side,rotations);//rotate specifying the tile to rotate, the value and side to be matched

				puzzle[x][y] = tile + 1;			//put tile in puzzle
				positions[stacktop][0] = x;			//record the positions of the tile
				positions[stacktop][1] = y;

				status[tile] = UNAVAILABLE;			//mark as used
			    tries[parent][child] = i;				//store the index of currently used child tilE

			    leftedge = puzzle[x][0] - 1;			//find tile at the left edge on the same row
			    tries[leftedge][DOWNCHILD] = 0;			//when something new is added delete history of bottom row
				stacktop += 1;										//keep track of top of the stack

				if(stacktop == numtiles)			//if stack is full signal that a solution has been found
				{
#ifdef debug
					printf("\n------Solution-----------------------\n");
					for(int i = 0; i < size; i++)
					{
						for(int j = 0; j < size; j++)
						{
							printf("%d ", (int)puzzle[i][j]);
						}
						printf("\n");
					}
					printf("\n-----------------------------\n");
#endif

					foundsolution = true;
				}
				matched = true;
				break;
			}
		}

	}
}



