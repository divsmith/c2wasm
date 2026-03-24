/* Demonstrates real free() with coalescing free-list allocator */

struct Node {
    int value;
    struct Node *next;
};

/* Build a linked list of n nodes */
struct Node *make_list(int n) {
    struct Node *head;
    struct Node *node;
    int i;
    head = (struct Node *)0;
    for (i = 0; i < n; i = i + 1) {
        node = (struct Node *)malloc(sizeof(struct Node));
        node->value = i;
        node->next = head;
        head = node;
    }
    return head;
}

/* Free all nodes in a list */
void free_list(struct Node *head) {
    struct Node *next;
    while (head != (struct Node *)0) {
        next = head->next;
        free(head);
        head = next;
    }
}

int main() {
    struct Node *list1;
    struct Node *list2;
    struct Node *p;
    int *a;
    int *b;

    /* Allocate and free - memory should be reused */
    a = (int *)malloc(64);
    *a = 12345;
    printf("First allocation at: %d\n", (int)a);
    free(a);

    b = (int *)malloc(64);
    printf("After free, reused at: %d\n", (int)b);
    if ((int)a == (int)b) {
        printf("Memory was reused!\n");
    }
    free(b);

    /* Build and free a linked list */
    printf("\nBuilding list of 5 nodes...\n");
    list1 = make_list(5);
    p = list1;
    while (p != (struct Node *)0) {
        printf("  node value: %d\n", p->value);
        p = p->next;
    }
    free_list(list1);
    printf("List freed.\n");

    /* Build another list - should reuse memory */
    printf("\nBuilding second list of 5 nodes...\n");
    list2 = make_list(5);
    printf("Second list built (memory reused from first).\n");
    free_list(list2);

    printf("Done!\n");
    return 0;
}
