TARGET1 := redis2-robj
TARGET2 := redis5-robj
CXX := gcc
CFLAGS := -g -lm
INCLUDE := -I ./


$(TARGET1): demo_robj_1_util.c demo_sds_1.c demo_robj_1.c demo_robj_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

$(TARGET2): demo_robj_2_util.c demo_sds_2.c demo_robj_2.c demo_robj_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f