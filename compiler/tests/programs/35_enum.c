/* EXPECT_EXIT: 42 */
enum Color {
    RED = 0,
    GREEN = 1,
    BLUE = 2
};

enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
};

int main(void) {
    int c;
    int d;

    c = GREEN;
    if (c != 1) return 1;

    d = WEST;
    if (d != 3) return 2;

    if (RED + GREEN + BLUE != 3) return 3;

    c = BLUE;
    switch (c) {
        case 0: return 4;
        case 1: return 5;
        case 2: break;
        default: return 6;
    }

    if (NORTH != 0) return 7;
    if (SOUTH != 1) return 8;
    if (EAST != 2) return 9;
    if (WEST != 3) return 10;

    return 42;
}
