TARGET2 := redis5-quicklist
CFLAGS := -g -lm
INCLUDE := -I ./

$(TARGET2): demo_quick_2_lzf_c.c demo_quick_2_lzf_d.c demo_quicklist_2_endianconv.c demo_quicklist_2_util.c demo_quicklist_2_ziplist.c demo_quicklist_2.c demo_quicklist_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f