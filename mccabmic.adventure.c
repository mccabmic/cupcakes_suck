#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define BUFFER 256
#define PLAY_ROOMS 7

/*----------DATA STRUCTURES------------*/
enum room_type {START_ROOM, MID_ROOM, END_ROOM};
struct Room {
	char name[BUFFER], type[BUFFER];
	char connections[PLAY_ROOMS][BUFFER];
	int numConnections;
};

struct Map {
	struct Room rooms[PLAY_ROOMS];
	int room_start;
	int room_end;
	int game_state;
	int numRooms;
};

/*----------THREADS-----------------*/
pthread_mutex_t mutex;
/*----------IO FUNCTIONS------------*/
/*takes a pointer to a room and initializes it with given parameters*/
void init_room(struct Room* room, char* name, char* type, int num_connections, char connections[PLAY_ROOMS][BUFFER]){
	memset(room->name, '\0', BUFFER);
	memset(room->type, '\0', BUFFER);
	strcpy(room->name, name);
	strcpy(room->type, type);
	
	room->numConnections = num_connections;
	int i;
	for (i = 0; i < room->numConnections; i++){
		strcpy(room->connections[i], connections[i]);
	}
}
/* this function opens a string file stream to read from a long multi-line string
and convert this string to a room struct*/
struct Room serialize(char* data_string){
	FILE *sstream;
	sstream = fmemopen(data_string, strlen(data_string), "r");

	char* line = NULL;
	size_t line_len = 0;
	size_t numCharacters = 0;
	int line_number = 0;

	char name[BUFFER], type[BUFFER];
	char myConnections[PLAY_ROOMS][BUFFER];
	
	while ((numCharacters = getline(&line, &line_len, sstream)) != -1){
		if (line_number == 0){
			sscanf(line, "%*s %*s %s", name);
		}
		else{
			sscanf(line, "%*s %*s %s", myConnections[line_number -1]);
		}
		line_number += 1;
	}
	/* my connections has 1 too many connections, but it gets cleared when the scope clears*/
	sscanf(line, "%*s %*s %s", type);
	free(line);
	fclose(sstream);
	
	struct Room test_room;

	/* - 2 for start, end rooms*/
	init_room(&test_room, name, type, line_number - 2, myConnections);
	return test_room;
}
/*given a filename, returns a string of the file contents*/
char* read_file(char* filename){
	FILE *fp = fopen(filename, "r");
	if (fp < 0){
		fprintf(stderr, "Failure to open file %s\n", filename);
		exit(1);
	}
	size_t buffer_size;
	char* file_contents = NULL;
	size_t numCharacters = getdelim(&file_contents, &buffer_size, '\0', fp);
	fclose(fp);
	if (numCharacters != -1){
		return file_contents;
	}
	else {
		free (file_contents);
		return 0;
	}
}
/*checks to see if directory exists*/
int validate_dir(char* directory){
	char* charPtr = strstr(directory, "mccabmic.rooms.");
	if (charPtr == 0){
		return 0;
	}
	else{
		return *charPtr;
	}
}
/* gets most recently modified directory using stat*/
/*Source: https://stackoverflow.com/questions/19117131/get-list-of-file-names-and-store-them-in-array-on-linux-using-c*/
char* get_most_recent(){
	DIR* directoryPtr;
	struct dirent *dir;
	struct stat dirStat;
	
	char* dirName = malloc(sizeof(char)*BUFFER);
	memset(dirName, '\0', BUFFER);
	time_t latest = 0;
	
	directoryPtr = opendir(".");
	if (directoryPtr){
		while ((dir = readdir(directoryPtr)) != NULL){
			memset(&dirStat, 0, sizeof(dirStat));
			/* check for only directories other than root and previous*/
			if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){	
				stat(dir->d_name, &dirStat);
				if( dirStat.st_mtime > latest){
					latest = dirStat.st_mtime;
					strcpy(dirName, dir->d_name);
				}
			}
		}
	}
	closedir(directoryPtr);
	return dirName;
}
/* cycles through a directory and saves file names to the provided array*/
/* returns number of files found*/
int list_of_files(char* directory, char myFiles[PLAY_ROOMS][BUFFER]){
	DIR* directoryPtr;
	struct dirent *dir;
	int count = 0;
	directoryPtr = opendir(directory);
	if(directoryPtr){
		while ((dir = readdir(directoryPtr)) != NULL){
			/* only save files that aren't directories or root */
			if (dir->d_type != DT_DIR && strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){	
				strcpy(myFiles[count], dir->d_name);
				count += 1;
				}
			}
		}	
	closedir(directoryPtr);
	return count;
}
/*---------------------- GAME FUNCTIONS -----------------*/
/* generate map takes a valid directory, reads in the files in the directory
and assigns the files in the directory to a map object*/
struct Map* generate_map(char* directory){
	if (validate_dir(directory) == 0){
		fprintf(stderr, "Error, invalid directory provided to generate_map()\n");
		exit(1);
	}

	struct Map* map = malloc(sizeof(struct Map));
	char myFiles[PLAY_ROOMS][BUFFER];
	
	int count = list_of_files(directory, myFiles);
	if (count < PLAY_ROOMS){
		fprintf(stderr, "Error, directory did not contain sufficient number of room files\n");
		exit(1);
	};
	chdir(directory);

	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		char* file_contents = read_file(myFiles[i]);
		/*template room created for ease of use and less dropped memory*/
		struct Room temp_room = serialize(file_contents);
		memcpy(&map->rooms[i], &temp_room, sizeof(temp_room));
		if(strcmp(map->rooms[i].type, "START_ROOM") == 0){
			map->room_start = i;
			}
		else if (strcmp(map->rooms[i].type, "END_ROOM") == 0){
			map->room_end = i;
		}
		free(file_contents);
	}
	map->numRooms = PLAY_ROOMS;
	map->game_state = 1;
	chdir("..");
	return map;
}
int find_room(char* name, struct Room rooms[PLAY_ROOMS]){
	int i;
	for (i = 0; i < PLAY_ROOMS; i++){
		if (strcmp(name, rooms[i].name) == 0){
			return i;
		}
	}
	return -1;
}
/*thread function to write time*/
void* write_time(void *threadid){
	FILE* fp;
	fp = fopen("currentTime.txt", "w+");
	size_t raw_time = time(NULL);
	char buffer[BUFFER];
	strftime(buffer, sizeof(buffer),"%I:%M%P, %A, %B %e, %Y", localtime(&raw_time));
	fprintf(fp, "%s\n", buffer);
	fclose(fp);

	/*long tid;
	tid = (long)threadid;
	printf("Hey, check it out, I'm here: %d\n", tid);
	*/
	
}
void read_time(){
	char* time_contents = read_file("currentTime.txt");
	printf("%s\n", time_contents);
	free(time_contents);
}

void CreateThread(long id){
	pthread_t thread;
	pthread_mutex_lock(&mutex);
	pthread_create(&thread, NULL, write_time, (void *)id);
	pthread_mutex_unlock(&mutex);
	pthread_join(thread, NULL);
	/*printf("okay, I think thread %d finished!\n", id);*/
}

void game_loop(struct Map* map){
	int player = map->room_start;
	int total_steps = 0;
	int player_history[BUFFER];
	int goal_room = map->room_end;
	char user_input[BUFFER];

	do{
		printf("CURRENT LOCATION: %s\n", map->rooms[player].name);
		printf("POSSIBLE CONNECTIONS:");
		int i;
		for (i = 0; i < map->rooms[player].numConnections - 1; i++){
			printf(" %s,", map->rooms[player].connections[i]);
		}
		printf(" %s.\n", map->rooms[player].connections[i]);
		printf("WHERE TO? >");
		memset(user_input, '\0', BUFFER);
		scanf("%255s", user_input);
		printf("\n");

		/*handle time*/
		long thread_count = 0;
		while (strcmp(user_input, "time") == 0){
			thread_count += 1;
			CreateThread(thread_count);
			read_time();
			printf("WHERE TO? >");
			scanf("%255s", user_input);
		}

		/* handle movement*/
		int destination = -1;
		for (i = 0; i < map->rooms[player].numConnections; i++){
			if (strcmp(user_input, map->rooms[player].connections[i]) == 0){
				destination = find_room(user_input, map->rooms);
				break;
			}
		}
		/* save player history*/
		if (destination >= 0){
			player_history[total_steps] = destination;
			player = destination;
			total_steps += 1;
		}
		else{
			printf("HUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN. \n\n");
		}

		if (player == goal_room){
			map->game_state = 0;
		}
	} while(map->game_state == 1);

	player_history[total_steps] = player;
	printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");
	printf("YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS: \n", total_steps);
	int i;
	for (i = 0; i < total_steps; i++){
		printf("%s\n", map->rooms[player_history[i]].name);
	}
}
 
int main(){
	char* my_dir = get_most_recent();
	struct Map* my_map = generate_map(my_dir);
	game_loop(my_map);
	
	free(my_dir);
	free(my_map);

	return 0;
}
