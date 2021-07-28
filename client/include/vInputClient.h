#define XRES_MAX 540.00
#define YRES_MAX 960.00
#define LTRB_Y   327.00
typedef enum {PRESS, RELEASE, MOVE} action;
//typedef enum {ROTATION_0, ROTATION_1} rotation;

int initvInputClient(const char []);
void send_sync_event();
void send_event(int32_t type, int32_t code, int32_t value);
bool vinput_touch(action a, u_int32_t x, u_int32_t y, int rotation);

