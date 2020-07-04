TARGET1 := redis2-adlist
CXX := gcc
CFLAGS := -g
INCLUDE := -I ./


$(TARGET1): demo_adlist_1.c demo_adlist_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^


clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f