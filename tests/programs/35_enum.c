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

enum Neg {
    NEG = -5,
    ZERO
};

enum Mixed {
    MA = 10,
    MB,
    MC
};

int g_color = GREEN;

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

    /* Fix 1: case with enum constant */
    c = GREEN;
    switch (c) {
        case RED: return 11;
        case GREEN: break;
        case BLUE: return 12;
        default: return 13;
    }

    /* Fix 2: negative explicit enum value */
    if (NEG != -5) return 14;
    if (ZERO != -4) return 15;

    /* Fix 3: mixed explicit+auto */
    if (MA != 10) return 16;
    if (MB != 11) return 17;
    if (MC != 12) return 18;

    /* Fix 4: global initialized with enum constant */
    if (g_color != 1) return 19;

    return 42;
}
