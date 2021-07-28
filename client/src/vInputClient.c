#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "vInputClient.h"
#define MSGQ_FILE_PATH "/tmp/input-lg"

static unsigned int tracking_id = 0;
static int mqId = -1;

struct mQData {
    long type;
    struct input_event ev;
};

int initvInputClient(const char file[])
{
    char path[100];
    key_t key = 0;
    int id = 0;

    if (!strcmp(file, "/dev/shm/looking-glass0"))
	    id = 0;
    else if (!strcmp(file, "/dev/shm/looking-glass1"))
	    id = 1;
    else if (!strcmp(file, "/dev/shm/looking-glass2"))
	    id = 2;
    else if (!strcmp(file, "/dev/shm/looking-glass3"))
	    id = 3;
    else {
        printf("Error: %s is not mapped to virtual input device", file);
        exit(0);
    }

    sprintf(path, "%s%d", MSGQ_FILE_PATH, id);

    if ((key = ftok(path, 99)) < 0) {
        printf("Failed to get msgq key\n");
        return -1;
    }

    if ((mqId = msgget(key, 0666)) < 0) {
        printf("Failed to get msgq id\n");
        return -1;
    }

    return 0;
}

// ============================================================================
void send_sync_event()
{
  struct mQData mD = {1};
  mD.ev.type = EV_SYN; mD.ev.code = SYN_REPORT; mD.ev.value= 0;
  msgsnd(mqId, &mD, sizeof(struct mQData), 0);
}

void send_event(int32_t type, int32_t code, int32_t value) {
  struct mQData mD = {1};
  mD.ev.type = type; mD.ev.code = code; mD.ev.value= value;
  msgsnd(mqId, &mD, sizeof(struct mQData), 0);
}

bool vinput_touch(action a, u_int32_t x, u_int32_t y, int rotation) {

	if (mqId == -1) {
		return false;
  	}


       	//printf("Original: x = %04x (%d), y = %04x (%d), rotate = %d (%d x %d)\n", x,x, y,y, rotation, xres, yres);

        switch (rotation) {
	    case 1:
	    case 3:
	        x *= XRES_MAX/YRES_MAX; 
	        y *= XRES_MAX/YRES_MAX;
	        y += LTRB_Y;
                break;
	  
	    case 2:
	        x *= XRES_MAX/YRES_MAX; 
	        y *= YRES_MAX/XRES_MAX;
		break;

	    case 0:
	    default:
		break;
	}

       
       	//printf("Transformed: x = %04x (%d), y = %04x (%d)\n", x,x, y,y);

	switch (a) {
		case PRESS:
			send_event(EV_ABS, ABS_MT_SLOT, 0);
			send_event(EV_ABS, ABS_MT_TRACKING_ID, tracking_id++);
			send_event(EV_ABS, ABS_PRESSURE, 50);
			send_event(EV_ABS, ABS_MT_PRESSURE, 50);
			send_event(EV_ABS, ABS_MT_POSITION_X, x);
			send_event(EV_ABS, ABS_MT_POSITION_Y, y);
			send_event(EV_KEY, BTN_TOUCH, 1);
			break;

		case RELEASE:
			send_event(EV_ABS, ABS_MT_SLOT, 0);
			send_event(EV_ABS, ABS_PRESSURE, 0);
			send_event(EV_ABS, ABS_MT_PRESSURE, 0);
			send_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
			send_event(EV_KEY, BTN_TOUCH, 0);
			break;
		case MOVE:
			send_event(EV_ABS, ABS_MT_POSITION_X, x);
     			send_event(EV_ABS, ABS_MT_POSITION_Y, y);
			break;
		default:
			break;
	}

  	send_event(EV_SYN, SYN_REPORT, 0);
	return true;
}
