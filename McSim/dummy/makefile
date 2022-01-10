TARGET = hello
OBJS = hello.o
CFLAGS = -O3 -w
CC = g++ 

$(TARGET): $(OBJS)
	$(CC) -static $(OBJS) $(CFLAGS) -o $(TARGET) 

$(OBJS): %.o: %.cc
	$(CC) -c $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(OBJS) $(TARGET) 
