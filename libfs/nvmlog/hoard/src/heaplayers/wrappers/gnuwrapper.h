extern "C" {
  void * xxmalloc (size_t);
  void   xxfree (void *);
  void * xxrealloc (void *ptr, size_t sz);
  void *get_page_list(unsigned int *pgcount);	
  void migrate_pages(int node);	
}
