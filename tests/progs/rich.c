typedef struct point {
    int x;
    int y;
} point_t;

enum color { RED, GREEN, BLUE };

struct shape {
    char name[12];
    enum color color;
    point_t origin;
    point_t *corners;
    unsigned short ncorners;
    double scale;
};

point_t square[4] = { {0, 0}, {10, 0}, {10, 10}, {0, 10} };
struct shape box = { "box", GREEN, {5, 5}, square, 4, 1.5 };
char *greeting = "hello, world";
unsigned char flags = 0x81;
short delta = -3;
long big = 123456789;
float ratio = 0.25;
int counts[5] = { 1, 2, 3, 4, 5 };

int area(struct shape *s)
{
    int w;
    int h;
    point_t *c;

    c = s->corners;
    w = c[2].x - c[0].x;
    h = c[2].y - c[0].y;
    return w * h;
}

int main(void)
{
    struct shape *sp;
    int a;
    char tag;

    sp = &box;
    tag = sp->name[0];
    a = area(sp);
    counts[0] = a;
    return a + tag;
}
