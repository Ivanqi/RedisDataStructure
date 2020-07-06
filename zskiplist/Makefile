TARGET1 := redis2-zskiplist
CXX := gcc
CFLAGS := -g
INCLUDE := -I ./


$(TARGET1): demo_zskiplist_1_object.c demo_zskiplist_1_util.c demo_zskiplist_1.c demo_zskiplist_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^


clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f