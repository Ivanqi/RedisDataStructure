TARGET1 := redis2-ziplist
TARGET2 := redis5-ziplist
CFLAGS := -g -lm
INCLUDE := -I ./

$(TARGET1): demo_ziplist_1_endianconv.c demo_ziplist_1_util.c demo_ziplist_1.c demo_ziplist_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

$(TARGET2): demo_ziplist_2_endianconv.c demo_ziplist_2_util.c demo_ziplist_2.c demo_ziplist_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f