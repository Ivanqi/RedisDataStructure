TARGET1 := redis2-dict
TARGET2 := redis5-dict
CXX := gcc
CFLAGS := -g
INCLUDE := -I ./


$(TARGET1): demo_dict_1.c demo_dict_1_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

$(TARGET2): demo_dict_2_siphash.c demo_dict_2.c demo_dict_2_test.c
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^

clean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET1) | xargs rm -f
	find . -name $(TARGET2) | xargs rm -f