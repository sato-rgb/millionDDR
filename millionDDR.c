#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include "xwiimote.h"
/* 
input_keyboard[6]
input_keyboard 0,1 device1,2_swing

*/

static const unsigned int PRESS_KEY_TIME =50;
static struct xwii_iface *iface[2];
static unsigned short input_keyboard[6];
static int8_t input_mouse[4];
static int threshold[2];
static const char keyboard_hid_dev_path[] = "/dev/hidg1";
static const char mouse_hid_dev_path[] = "/dev/hidg0";
//static const char keyboard_hid_dev_path[] = "/dev/null";
//static const int POLL_TIMEOUT = -1;
enum {
    MOUSE_BUTTON,
	MOUSE_X_AXIS,
	MOUSE_Y_AXIS,
	MOUSE_WHEEL
};
static char *get_dev(int num)
{
	struct xwii_monitor *mon;
	char *ent;
	int i = 0;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return NULL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		if (++i == num)
			break;
		free(ent);
	}

	xwii_monitor_unref(mon);

	if (!ent)
		printf("Cannot find device with number #%d\n", num);

	return ent;
}
void send_key (FILE* keyboard_hid_dev) {
	fprintf (keyboard_hid_dev, "%c%c%c%c%c%c%c%c", '\0', '\0',input_keyboard[0],
	                                                 input_keyboard[1],
													 input_keyboard[2],
													 input_keyboard[3],
													 input_keyboard[4],
													 input_keyboard[5]);
    fflush(keyboard_hid_dev);
}
void send_mouse (FILE* mouse_hid_dev){
	fprintf (mouse_hid_dev, "%c%c%c%c",input_mouse[MOUSE_BUTTON],
	                                   input_mouse[MOUSE_X_AXIS],
									   input_mouse[MOUSE_Y_AXIS],
									   input_mouse[MOUSE_WHEEL]);
	fflush(mouse_hid_dev);
}
static void accel_swing_check(struct xwii_event *event,int dev){
unsigned short LEFT,
	           RIGHT,
		       UP;
	if (dev == 0){
        LEFT = 0x08,
        RIGHT = 0x09,
        UP = 0x0A;
	}else{
        LEFT = 0x0B,
        RIGHT = 0x0C,
        UP = 0x0D;
	}
	    if (threshold[dev] > SWING_AFTER_THRESHOLD - PRESS_KEY_TIME ) input_keyboard[dev] = 0;
	    if (threshold[dev] > THRESHOLD) threshold[dev] -= THRESHOLD_DESCEND;

	if (event->v.abs[0].x > threshold[dev]){
        threshold[dev] = SWING_AFTER_THRESHOLD;
		input_keyboard[dev] = LEFT;
	}else if (event->v.abs[0].x < -threshold[dev]){
        threshold[dev] = SWING_AFTER_THRESHOLD;
		input_keyboard[dev] = RIGHT;
	}else if (event->v.abs[0].z > threshold[dev] + 100 || event->v.abs[0].y > threshold[dev]){
        threshold[dev] = SWING_AFTER_THRESHOLD;
		input_keyboard[dev] = UP;
	}
}
static void key_show_device1(const struct xwii_event *event)
{
unsigned int code = event->v.key.code;
unsigned short LEFT,RIGHT,UP,DOWN;
    if (event->v.key.state){
	    LEFT = 0xFF,
	    RIGHT = 0x01,
		UP = 0xFF,
		DOWN = 0x01;
		}else{LEFT = 0,RIGHT = 0,UP = 0,DOWN = 0;}
	if (code == XWII_KEY_LEFT) {
	    input_mouse[MOUSE_X_AXIS] = LEFT;
	}else if (code == XWII_KEY_RIGHT) {
		input_mouse[MOUSE_X_AXIS] = RIGHT;
	}else if (code == XWII_KEY_UP) {
		input_mouse[MOUSE_Y_AXIS] = UP;
	}else if (code == XWII_KEY_DOWN) {
		input_mouse[MOUSE_Y_AXIS] = DOWN;
	}
}
static void key_show_device2(const struct xwii_event *event)
{
unsigned int code = event->v.key.code;
	bool pressed = event->v.key.state;
	unsigned short a_btn;
	if (event->v.key.state){
	    a_btn = 0x1;}else{
		a_btn = 0;}
	if (code == XWII_KEY_A){
		input_mouse[MOUSE_BUTTON] = a_btn;
	}
}
static void loop(struct xwii_iface **iface){
    int ret = 0;
    struct xwii_event event[2];
    FILE *keyboard_hid_dev,*mouse_hid_dev;
    keyboard_hid_dev = fopen(keyboard_hid_dev_path,"w");
	mouse_hid_dev = fopen(mouse_hid_dev_path,"w");
if(keyboard_hid_dev == NULL || mouse_hid_dev == NULL ) {
		printf("file not open!\n");
		return;
	}

    for (int dev = 0; dev < 2; dev++){
		xwii_iface_watch(iface[dev], true);		
    } 
   while (true) {
    static uint32_t loop_time;
	loop_time++;
			unsigned short tmp[6] = {
				input_keyboard[0],
				input_keyboard[1],
				input_keyboard[2],
				input_keyboard[3],
				input_keyboard[4],
				input_keyboard[5]};
			int8_t mouse_tmp = input_mouse[0];
		for (int dev = 0; dev < 2; dev++){
			ret = xwii_iface_dispatch(iface[dev], &event[dev], sizeof(event[dev]));
			if (ret) {
			if (ret != -11) {
				printf("Error: Read failed with err:%d",
					    ret);
				break;
			}
		}else{
		switch (event[dev].type){
            case XWII_EVENT_ACCEL:
                    accel_swing_check(&event[dev],dev);
                break;
				case XWII_EVENT_KEY:
				if (dev == 0){
					key_show_device1(&event[dev]);
				}else{
					key_show_device2(&event[dev]);
				}
				break;
			default:
                break;
			}
			}
			}
            for (unsigned short i = 0; i < 6; i++){
				if (input_keyboard[i] != tmp[i]){
					send_key(keyboard_hid_dev);
					break;
				};
			}
				if(!(loop_time % 50)){send_mouse(mouse_hid_dev);}
	}
}


static  unsigned int THRESHOLD;
static  unsigned int SWING_AFTER_THRESHOLD;
static  unsigned int THRESHOLD_DESCEND;

int main(int argc, char const *argv[]){

int ret = 0;
char *path = NULL;
printf({"%d",argc});
// threshold_initialize
if (argc == 3){
	THRESHOLD=atoi(argv[1]);
    threshold[0]=THRESHOLD;
    threshold[1]=THRESHOLD;
    SWING_AFTER_THRESHOLD = atoi(argv[2]);
	THRESHOLD_DESCEND = atoi(argv[3]);
}else{
	THRESHOLD = 200;
    threshold[0] = THRESHOLD;
    threshold[1] = THRESHOLD;
    SWING_AFTER_THRESHOLD = 500;
	THRESHOLD_DESCEND = 10;
}

for (int dev = 0; dev < 2; dev++){
    path = get_dev(dev + 1);
    ret = xwii_iface_new(&iface[dev],path);
    free(path);
    if (!ret){
        ret = xwii_iface_open(iface[dev],
				      xwii_iface_available(iface[dev]) |
				      XWII_IFACE_WRITABLE);
    }else{break;}
	
}

    if (!ret){loop(iface);}
}