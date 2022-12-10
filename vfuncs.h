/* zvector.c */
void p_init_zvect(void);
void **vect_data(Vector_t *v);
bool vect_check_status(Vector_t *v, VectIndex_t flag_id);
bool vect_set_status(Vector_t *v, VectIndex_t flag_id);
bool vect_clear_status(Vector_t *v, VectIndex_t flag_id);
bool vect_toggle_status(Vector_t *v, VectIndex_t flag_id);
VectStatus_t p_vect_clear(Vector_t *v);
void vect_shrink(Vector_t *v);
bool vect_is_empty(Vector_t *v);
VectIndex_t vect_size(Vector_t *v);
VectIndex_t vect_max_size(Vector_t *v);
void *vect_begin(Vector_t *v);
void *vect_end(Vector_t *v);
Vector_t *vect_create(const VectIndex_t init_capacity, const size_t item_size, const uint32_t properties);
void vect_destroy(Vector_t *v);
void vect_clear(Vector_t *v);
void vect_set_wipefunct(Vector_t *v, void (*f1)(const void *, size_t));
void vect_add(Vector_t *v, const void *value);
void vect_add_at(Vector_t *v, const void *value, const VectIndex_t i);
void vect_add_front(Vector_t *v, const void *value);
void *vect_get(Vector_t *v);
void *vect_get_at(Vector_t *v, const VectIndex_t i);
void *vect_get_front(Vector_t *v);
void vect_put(Vector_t *v, const void *value);
void vect_put_at(Vector_t *v, const void *value, const VectIndex_t i);
void vect_put_front(Vector_t *v, const void *value);
void *vect_remove(Vector_t *v);
void *vect_remove_at(Vector_t *v, const VectIndex_t i);
void *vect_remove_front(Vector_t *v);
void vect_delete(Vector_t *v);
void vect_delete_at(Vector_t *v, const VectIndex_t i);
void vect_delete_range(Vector_t *v, const VectIndex_t first_element, const VectIndex_t last_element);
void vect_delete_front(Vector_t *v);
void vect_swap(Vector_t *v, const VectIndex_t i1, const VectIndex_t i2);
void vect_swap_range(Vector_t *v, const VectIndex_t s1, const VectIndex_t e1, const VectIndex_t s2);
void vect_rotate_left(Vector_t *v, const VectIndex_t i);
void vect_rotate_right(Vector_t *v, const VectIndex_t i);
void vect_qsort(Vector_t *v, int (*compare_func)(const void *, const void *));
bool vect_bsearch(Vector_t *v, const void *key, int (*f1)(const void *, const void *), VectIndex_t *item_index);
void vect_add_ordered(Vector_t *v, const void *value, int (*f1)(const void *, const void *));
void vect_apply(Vector_t *v, void (*f)(void *));
void vect_apply_range(Vector_t *v, void (*f)(void *), const VectIndex_t x, const VectIndex_t y);
void vect_apply_if(Vector_t *v1, Vector_t *const v2, void (*f1)(void *), bool (*f2)(void *, void *));
void vect_copy(Vector_t *v1, Vector_t *const v2, const VectIndex_t s2, const VectIndex_t e2);
void vect_insert(Vector_t *v1, Vector_t *v2, const VectIndex_t s2, const VectIndex_t e2, const VectIndex_t s1);
void vect_move(Vector_t *v1, Vector_t *v2, const VectIndex_t s2, const VectIndex_t e2);
VectStatus_t vect_move_if(Vector_t *v1, Vector_t *v2, const VectIndex_t s2, const VectIndex_t e2, VectStatus_t (*f2)(void *, void *));
void vect_merge(Vector_t *v1, Vector_t *v2);