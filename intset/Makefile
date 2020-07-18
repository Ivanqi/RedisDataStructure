TARGET1 := redis2-intset
TARGET1 := redis5-intset
CFLAGS := -g -lm
INCLUDE := -I ./

$(TARGET1): demo_intset_1_endianconv.c demo_intset_1.c demo_intset_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

$(TARGET2): demo_intset_2_endianconv.c demo_intset_2.c demo_intset_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f