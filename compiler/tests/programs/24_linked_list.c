// Tests linked list using structs and pointers
struct Node {
    int val;
    int next;
};

int main() {
    struct Node *a = malloc(sizeof(struct Node));
    struct Node *b = malloc(sizeof(struct Node));
    struct Node *c = malloc(sizeof(struct Node));
    a->val = 1; a->next = b;
    b->val = 2; b->next = c;
    c->val = 3; c->next = 0;

    // Traverse the list
    int *cur = a;
    int sum = 0;
    while (cur != 0) {
        struct Node *n = cur;
        sum += n->val;
        cur = n->next;
    }
    printf("sum=%d\n", sum);
    return 0;
}
