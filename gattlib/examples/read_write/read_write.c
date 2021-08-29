/*
 *
 *  GattLib - GATT Library
 *
 *  Copyright (C) 2016-2019  Olivier Martin <olivier@labapart.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gattlib.h"
#include <unistd.h>

typedef enum { READ, WRITE} operation_t;
operation_t g_operation;

static uuid_t g_uuid;
char value_data[8];



struct RemoteInputData {
	// For example:
	bool Button1Pressed; // Top button
	bool Button2Pressed; // Middle Left button
	bool Button3Pressed; // Middle Right button
	bool Button4Pressed; // Bottom Left button
	bool Button5Pressed; // Bottom Right button
};

struct RemoteOutputData {
	// Need to define what can actually be sent and include high level member variables to make sending easy
	int vibrationIntensity; // each int has a coressponding duty cycle (controls the voltage supplied to the vibration motor)
};

int i, ret;
size_t len;
gatt_connection_t* connection;
uint8_t *buffer;

void setup(){
	uint8_t *buffer = NULL;
	connection = gattlib_connect(NULL, "84:CC:A8:7B:01:2A", GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);

}
void stop(){
	gattlib_disconnect(connection);
	//return ret;
}

void getInputData(struct RemoteInputData inputData){
	gattlib_string_to_uuid("6e400003-b5a3-f393-e0a9-e50e24dcca9e", strlen("6e400003-b5a3-f393-e0a9-e50e24dcca9e") + 1, &g_uuid);

	//connection = gattlib_connect(NULL, "84:CC:A8:7B:01:2A", GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
	//while(1){
	ret = gattlib_read_char_by_uuid(connection, &g_uuid, (void **)&buffer, &len);
	if (ret != GATTLIB_SUCCESS) {
		char uuid_str[MAX_LEN_UUID_STR + 1];

		gattlib_uuid_to_string(&g_uuid, uuid_str, sizeof(uuid_str));

		if (ret == GATTLIB_NOT_FOUND) {
			fprintf(stderr, "Could not find GATT Characteristic with UUID %s. "
				"You might call the program with '--gatt-discovery'.\n", uuid_str);
		} else {
			fprintf(stderr, "Error while reading GATT Characteristic with UUID %s (ret:%d)\n", uuid_str, ret);
		}
		stop();
	}

	printf("Read UUID completed: ");
	for (i = 0; i < len; i++) {
		printf("%02x ", buffer[i]);
	}
	printf("\n");

	if(buffer[i] == 0){

	} else if (buffer[i] == 1){
		inputData.Button1Pressed = true;
	} else if (buffer[i] == 2){
		inputData.Button2Pressed = true;
	} else if (buffer[i] == 3){
		inputData.Button3Pressed = true;
	} else if (buffer[i] == 4){
		inputData.Button4Pressed = true;
	} else if (buffer[i] == 5){
		inputData.Button5Pressed = true;
	} 

	free(buffer);
	//}
	// EXIT:
	// stop();
}


void sendOutputData(struct RemoteOutputData outputData){
	gattlib_string_to_uuid("6e400002-b5a3-f393-e0a9-e50e24dcca9e", strlen("6e400002-b5a3-f393-e0a9-e50e24dcca9e") + 1, &g_uuid);
	
	//connection = gattlib_connect(NULL, "84:CC:A8:7B:01:2A", GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
	
	//while(1){
	sprintf(value_data, "%d", outputData.vibrationIntensity);
	//strcpy(value_data, outputData.vibrationIntensity);
	// printf("%s",value_data);
	// printf("%p",&value_data);
	// printf("%d",sizeof(value_data));
	// printf("%p",&g_uuid);
	//printf("dsadsa\n");
	printf("sending vibration intensity: %d\n", outputData.vibrationIntensity);
	ret = gattlib_write_char_by_uuid(connection, &g_uuid, &value_data, sizeof(value_data)); // line gives seg fault
	//printf("no\n");
	if (ret != GATTLIB_SUCCESS) {
		char uuid_str[MAX_LEN_UUID_STR + 1];

		gattlib_uuid_to_string(&g_uuid, uuid_str, sizeof(uuid_str));

		if (ret == GATTLIB_NOT_FOUND) {
			fprintf(stderr, "Could not find GATT Characteristic with UUID %s. "
				"You might call the program with '--gatt-discovery'.\n", uuid_str);
				stop();
		} else {
			fprintf(stderr, "Error while writing GATT Characteristic with UUID %s (ret:%d)\n",
				uuid_str, ret);
		}
		stop();
	}
	//}
	// EXIT:
	// stop();
}






/*
static void usage(char *argv[]) {
	printf("%s <read|write> [<hex-value-to-write>]\n", argv[0]);
}
*/
int main(int argc, char *argv[]) {
	
	setup();
	
	struct RemoteInputData testInput;
	struct RemoteOutputData testOutput;
	testOutput.vibrationIntensity = strtol(argv[1], NULL, 10);
	//printf("%d\n",testOutput.vibrationIntensity);

	//sendOutputData(testOutput);
	
	for (int k = 0; k < 1000; k++){
		if (k%2 == 0){
			sendOutputData(testOutput);
			usleep(1);
		} else {
			getInputData(testInput);
			usleep(1);
		}
		if (k == 1000){
			k = 0;
		}
	}
	
	return 1;

	// printf("dsaad\n");
	// if (strcmp(argv[1], "read") == 0) {
	// 	getInputData(testInput);
		
	// } else if ((strcmp(argv[1], "write") == 0) && (argc == 3)) {
	//	struct RemoteOutputData testOutput;
	//	testOutput.vibrationIntensity = strtol(argv[2], NULL, 10);
	// 	sendOutputData(testOutput);
		
	// } else {
	// 	usage(argv);
	// 	return 1;
	// }
	
}