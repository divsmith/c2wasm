// Day of Week
// Features: enum, typedef, const, switch/case, ternary

enum Weekday { MON = 1, TUE, WED, THU, FRI, SAT, SUN };
typedef int Day;

const int WORK_DAYS = 5;

int is_weekend(Day d) {
    return (d == SAT || d == SUN) ? 1 : 0;
}

void print_day(Day d) {
    switch (d) {
        case MON: printf("Monday   "); break;
        case TUE: printf("Tuesday  "); break;
        case WED: printf("Wednesday"); break;
        case THU: printf("Thursday "); break;
        case FRI: printf("Friday   "); break;
        case SAT: printf("Saturday "); break;
        case SUN: printf("Sunday   "); break;
        default:  printf("Unknown  "); break;
    }
    if (is_weekend(d)) {
        printf(" (weekend)\n");
    } else {
        printf(" (weekday)\n");
    }
}

int main() {
    Day d;
    int weekends = 0;
    for (d = MON; d <= SUN; d++) {
        weekends += is_weekend(d);
        print_day(d);
    }
    printf("%d work days, %d weekend days\n", WORK_DAYS, weekends);
    return 0;
}
