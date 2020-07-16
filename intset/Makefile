TARGET1 := redis2-intset
CFLAGS := -g -lm
INCLUDE := -I ./

$(TARGET1): demo_intset_1_endianconv.c demo_intset_1.c demo_intset_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f