#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> /*mkdir chdir*/
#include <time.h> /*seed */
#include <string.h> /*memcopy*/
#include <unistd.h> /*pid*/

#define MAX_ROOMS 10
#define PLAY_ROOMS 7
#define MIN_CONNECTIONS 3
#define MAX_CONNECTIONS 6

/*--------------------------DATA & GLOBALS ----------------------------*/
enum room_type {START_ROOM, MID_ROOM, END_ROOM};
const char* room_names[MAX_ROOMS] = {
	"Lakeview",
	"Wrigleyville",
	"Edgewater",
	"OldTown",
	"Loop",
	"SLoop",
	"Lincoln_Park",
	"Wicker_Park",
	"Pilsen",
	"Chinatown"
};

struct Room {
	int id;
	const char* name;
	enum room_type type;
	int numConnections;
	int neighbors[PLAY_ROOMS];
};

/*-------------------------------UTILITY ----------------------------*/
void swap(int *a, int *b){
	int temp = *a;
	*a = *b;
	*b = temp;
}
/* Utility function to print arrays*/
void print_array(int *array, int n){
	int i;
	for (i = 0; i < n; i++){
		printf("%d ", array[i]);
	}
	printf("\n");
}

/* Utility function to print room*/
void print_room(struct Room printRoom){
	printf("room id: %d\n", printRoom.id);
	printf("room name: %s\n", printRoom.name);
	printf("room type: %d\n", printRoom.type);
	printf("room connections: %d\n", printRoom.numConnections);
	printf("room neighbors: ");
	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		printf( "%d ", printRoom.neighbors[i]); 
	}
	printf("\n");
}

/* shuffles array of values in place*/
/*source: https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle*/
void shuffle(int *array, int n){
	int i;
	int j = 0;
	for (i = n - 1; i >= 1; i--){
		j = rand() % (i + 1);
		swap(&array[i],&array[j]);
	}
}

/*---------------------CORE FUNCTIONALITY ------------------------*/
/*function takes a struct of uninitialized rooms
and initializes them in a random order*/
void init_rooms(struct Room rooms[PLAY_ROOMS]){
	int random_order[PLAY_ROOMS];
	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		random_order[i] = i;
	}
	shuffle(random_order, PLAY_ROOMS);

	/*adjacency list default_connections sets all to 0*/
	int default_connections[PLAY_ROOMS]= {0};
	for (i = 0; i < PLAY_ROOMS; i++){
		rooms[i].id = i;
		rooms[i].name = room_names[random_order[i]];
		rooms[i].type = MID_ROOM;
		rooms[i].numConnections = 0;
		memcpy(rooms[i].neighbors, default_connections, sizeof(rooms[i].neighbors));
	}
	
	/* easier to do it this way*/
	rooms[0].type = START_ROOM;
	rooms[PLAY_ROOMS-1].type = END_ROOM;
}

char* createDir(){
	int id = getpid();
	char* directory = malloc(30);
	sprintf(directory, "%s.%d", "mccabmic.rooms", id);
	if (mkdir(directory, 0755) == -1){
		fprintf(stderr, "%s", "Error, directory failed to create\n");
		return 0;
		}

	return directory;
}

/*---------------------GRAPH FUNCTIONS -------------------------*/
/*Source:  https://oregonstate.instructure.com/courses/1706555/pages/2-dot-2-program-outlining-in-program-2*/
/* Thanks again for the outline!*/
int isGraphFull(struct Room rooms[PLAY_ROOMS]){
	int full_rooms = 0;
	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		if (rooms[i].numConnections >= MIN_CONNECTIONS && rooms[i].numConnections < MAX_CONNECTIONS){
			full_rooms += 1;
		}
	}
	return (full_rooms == PLAY_ROOMS - 1);
}

int CanAddConnectionFrom(struct Room* xPtr){
	return (xPtr->numConnections < MAX_CONNECTIONS);
}

int ConnectionAlreadyExists(struct Room* xPtr, struct Room* yPtr){
	return (xPtr->neighbors[yPtr->id] == yPtr->neighbors[xPtr->id] && xPtr->neighbors[yPtr->id] == 1) ;
}

void ConnectRoom(struct Room* xPtr, struct Room* yPtr){
	xPtr->neighbors[yPtr->id] = 1;
	xPtr->numConnections += 1;
}

int isSameRoom (struct Room* xPtr, struct Room* yPtr){
	return (xPtr->id == yPtr->id);
}

void AddRandomConnection(struct Room rooms[PLAY_ROOMS]){
	struct Room* randomRoom;
	struct Room* otherRoom;

	while(1){
		randomRoom = &rooms[rand() % PLAY_ROOMS];
		if (CanAddConnectionFrom(randomRoom) == 1){
			break;
		}
	}

	do{
		otherRoom = &rooms[rand() % PLAY_ROOMS];
	} while(CanAddConnectionFrom(otherRoom) == 0 || isSameRoom(randomRoom, otherRoom) == 1 || ConnectionAlreadyExists(randomRoom, otherRoom) == 1);
	
	ConnectRoom(randomRoom, otherRoom);
	ConnectRoom(otherRoom, randomRoom);
}

void populateConnections(struct Room rooms[PLAY_ROOMS]){
	while (isGraphFull(rooms) == 0){
		AddRandomConnection(rooms);
	}
}

void writeToDisk(struct Room rooms[PLAY_ROOMS], char* main_directory){
	chdir(main_directory);
	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		FILE *fp = fopen(rooms[i].name, "w");
		fprintf(fp, "ROOM NAME: %s\n", rooms[i].name);
		
		/*map array indexes to names*/
		int j;
		int found = 0;
		for (j = 0; j < PLAY_ROOMS; j++){
			int neighbor = rooms[i].neighbors[j];
			if (neighbor){
				found += 1;
				fprintf(fp, "CONNECTION %d: %s\n", found, rooms[j].name);
			}
		}

		/*stringify room_type enum*/
		fprintf(fp, "ROOM TYPE: ");
		if (rooms[i].type == START_ROOM){
			fprintf(fp, "START_ROOM");
		}
		else if (rooms[i].type == END_ROOM){
			fprintf(fp, "END_ROOM");
		}
		else {
			fprintf(fp, "MID_ROOM");
		}
		
		/*add a new line*/
		fprintf(fp, "\n");

		fclose(fp);
	}
}

/* MAIN */
int main(){
	char *main_directory = createDir();
	srand(time(NULL));
	struct Room my_rooms[PLAY_ROOMS];
	init_rooms(my_rooms);
	populateConnections(my_rooms);
	writeToDisk(my_rooms, main_directory);
	free(main_directory);
	return 0;
}
