TARGET1 := redis2-sds
TARGET2 := redis5-sds
CXX := gcc
CFLAGS := -g
INCLUDE := -I ./


$(TARGET1): demo_sds_1.c demo_sds_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

$(TARGET2): demo_sds_2.c demo_sds_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f