CC=$(CROSS_COMPILE)gcc
OBJS := timer.o
worms: $(OBJS)
	$(CC) -o timer $(OBJS)
$(OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@