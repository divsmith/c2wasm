// File permissions with bit flags
// Features: typedef, const, compound bitwise ops (|=, &=, ^=), ~, ternary

typedef int Perms;

const int READ  = 4;
const int WRITE = 2;
const int EXEC  = 1;

void show(Perms p) {
    int r;
    int w;
    int x;
    r = (p & READ)  ? 1 : 0;
    w = (p & WRITE) ? 1 : 0;
    x = (p & EXEC)  ? 1 : 0;
    printf("r=%d w=%d x=%d\n", r, w, x);
}

int main() {
    Perms owner;
    Perms group;
    Perms other;

    owner = READ | WRITE | EXEC;   /* rwx = 7 */
    group = READ | EXEC;           /* r-x = 5 */
    other = READ;                  /* r-- = 4 */

    printf("Initial:\n");
    printf("  owner: "); show(owner);
    printf("  group: "); show(group);
    printf("  other: "); show(other);

    owner ^= WRITE;       /* revoke write from owner */
    group |= WRITE;       /* grant write to group */
    other &= ~EXEC;       /* ensure no exec for other */

    printf("After chmod:\n");
    printf("  owner: "); show(owner);
    printf("  group: "); show(group);
    printf("  other: "); show(other);
    return 0;
}
