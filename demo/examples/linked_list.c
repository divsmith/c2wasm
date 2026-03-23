// Linked list traversal
struct Node {
    int val;
    int next;
};

int main() {
    struct Node *a = malloc(sizeof(struct Node));
    struct Node *b = malloc(sizeof(struct Node));
    struct Node *c = malloc(sizeof(struct Node));
    a->val = 10; a->next = b;
    b->val = 20; b->next = c;
    c->val = 30; c->next = 0;

    int *cur = a;
    int sum = 0;
    while (cur != 0) {
        struct Node *n = cur;
        printf("%d -> ", n->val);
        sum = sum + n->val;
        cur = n->next;
    }
    printf("NULL\n");
    printf("sum = %d\n", sum);
    return 0;
}
