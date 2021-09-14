
typedef struct wn_pfunc_node_t
{
    void (*curr)(void*, void*, void*);
    void (*next)(void*, void*, void*);
} wn_pfunc_node_t;